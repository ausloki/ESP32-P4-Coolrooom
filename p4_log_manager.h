#pragma once
// ============================================================================
// p4_log_manager.h — ESP32-P4 Coolroom Controller: SD log file manager
//
// Registers a small custom HTTP handler on the same AsyncWebServer instance
// ESPHome's web_server component uses, so it automatically inherits the same
// basic-auth credentials (see WebServerBase::add_handler()). Nothing like
// this exists in stock ESPHome — web_server only exposes entity read/write,
// not arbitrary file access — so this is a purpose-built, narrowly-scoped
// addition: it only ever touches files matching a strict daily-log filename
// pattern under /sdcard, never an arbitrary path.
//
// Routes:
//   GET  /logs                     -> JSON object:
//        { "mounted":bool, "card":"...", "speed_khz":N,
//          "free_mb":N, "total_mb":N, "used_mb":N,
//          "opens":N, "reads":N, "writes":N,
//          "bytes_read":N, "bytes_written":N,
//          "files":[{"name","size","mtime","kind"}, ...] }
//   GET  /logs/download?file=NAME  -> raw file content, attachment download
//   POST /logs/delete?file=NAME    -> {"ok":true|false}
//   GET  /api/events?since=N       -> {"seq":N,"events":[{seq,ts,event,detail},...]}
//        Live ring of recent p4_sd_log_event() calls (RAM, survives no reboot).
//        Pass since=<last seq> to receive only newer entries.
//
// Listing and download accept a log file ("events.csv", "nodate.csv" or
// "YYYY-MM-DD.csv") or the settings backup ("backup.json"). Delete accepts
// log files only: backup.json is the operator's saved settings, not a log,
// and the Backup button already overwrites it — offering "delete" for it in
// a log browser is a footgun with no matching use case.
// This whitelist is the only defense against path traversal, so it is
// intentionally strict rather than a blocklist of "..", slashes, etc.
// ============================================================================

#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <esp_log.h>
#include "esphome/components/web_server_base/web_server_base.h"
#include "p4_logging.h"

namespace p4_logs {

static const char *const TAG_LOGMGR = "p4_logmgr";

// FAT stores 8.3 short names upper-cased, so readdir() reports "EVENTS.CSV"
// for any file written while long-filename support was off. Compare
// case-insensitively so those older files stay listable and downloadable
// rather than silently vanishing from /logs.
inline bool name_equals_ci(const std::string &name, const char *expected) {
  size_t len = strlen(expected);
  if (name.size() != len)
    return false;
  for (size_t i = 0; i < len; i++) {
    if (tolower(static_cast<unsigned char>(name[i])) != expected[i])
      return false;
  }
  return true;
}

inline bool is_valid_log_filename(const std::string &name) {
  if (name_equals_ci(name, "events.csv"))
    return true;
  // Readings taken before the clock is trustworthy (see p4_sd_log_temps).
  if (name_equals_ci(name, "nodate.csv"))
    return true;
  if (name.size() != 14)  // "YYYY-MM-DD.csv"
    return false;
  if (!name_equals_ci(name.substr(10), ".csv"))
    return false;
  for (int i = 0; i < 10; i++) {
    char c = name[i];
    if (i == 4 || i == 7) {
      if (c != '-')
        return false;
    } else if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

/// The settings backup written by p4_sd_backup_params(). Listed and
/// downloadable alongside the logs so an operator can actually see that a
/// backup exists and save a copy off the device.
inline bool is_backup_filename(const std::string &name) {
  return name_equals_ci(name, "backup.json");
}

inline bool is_downloadable_filename(const std::string &name) {
  return is_valid_log_filename(name) || is_backup_filename(name);
}

/// Seconds east of UTC in effect at `when`, derived by differencing the two
/// broken-down forms of the same instant.
///
/// Deliberately avoids mktime()/timegm(): on this toolchain's newlib, mktime()
/// does not apply the TZ that localtime_r() plainly does, so anything built on
/// it silently converts by zero. localtime_r() and gmtime_r() are the two calls
/// this device demonstrably gets right, so the offset is measured from them.
inline long tz_offset_seconds(time_t when) {
  struct tm lt {};
  struct tm gt {};
  localtime_r(&when, &lt);
  gmtime_r(&when, &gt);
  long secs = (lt.tm_hour - gt.tm_hour) * 3600L + (lt.tm_min - gt.tm_min) * 60L +
              (lt.tm_sec - gt.tm_sec);
  int day_diff = lt.tm_yday - gt.tm_yday;
  if (lt.tm_year != gt.tm_year)  // year boundary: the difference is only ever a day
    day_diff = (lt.tm_year > gt.tm_year) ? 1 : -1;
  return secs + day_diff * 86400L;
}

/// Convert a FAT modification time into a real UTC epoch.
///
/// FAT directory entries hold bare wall-clock fields with no timezone. ESP-IDF
/// fills them from localtime() on write, but rebuilds st_mtime from them with
/// mktime() — which, per the note above, does not re-apply the zone here. The
/// value that reaches us is therefore local wall clock expressed as an epoch:
/// 8 hours ahead of the true instant in Australia/Perth, which the dashboard
/// then rendered as local time again, dating every log file 8 hours ahead.
inline time_t fat_mtime_to_utc(time_t fat_mtime) {
  if (fat_mtime <= 0)
    return fat_mtime;
  return fat_mtime - tz_offset_seconds(fat_mtime);
}

class LogManagerHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    esphome::StringRef url = request->url_to(buf);
    auto method = request->method();
    if (url == "/logs" && method == HTTP_GET)
      return true;
    if (url == "/logs/download" && method == HTTP_GET)
      return true;
    if (url == "/logs/delete" && method == HTTP_POST)
      return true;
    if (url == "/api/events" && method == HTTP_GET)
      return true;
    return false;
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    esphome::StringRef url = request->url_to(buf);

    if (url == "/logs") {
      this->handle_list_(request);
    } else if (url == "/logs/download") {
      this->handle_download_(request);
    } else if (url == "/logs/delete") {
      this->handle_delete_(request);
    } else if (url == "/api/events") {
      this->handle_events_(request);
    }
  }

  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void handle_events_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    // Full ring snapshot; the dashboard filters with since= client-side.
    // The string lives in p4_logging's static cache, not on this stack.
    request->send(200, "application/json", p4_event_ring_json().c_str());
  }
  void handle_list_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    if (!p4_sd_is_ready()) {
      request->send(503, "application/json",
                    "{\"error\":\"sd_not_mounted\",\"mounted\":false,\"files\":[]}");
      return;
    }
    struct Entry {
      std::string name;
      size_t size;
      time_t mtime;
    };
    std::vector<Entry> files;
    DIR *dir = opendir("/sdcard");
    if (dir == nullptr) {
      ESP_LOGW(TAG_LOGMGR, "opendir(/sdcard) failed (errno %d) — listing empty", errno);
    } else {
      // An empty listing is ambiguous on its own — it can mean an empty card,
      // a directory that would not open, or names that failed the filter. Name
      // the skipped entries (bounded) so the cause is visible from one log line.
      std::string skipped;
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (!is_downloadable_filename(name)) {
          if (skipped.size() < 200)
            skipped += (skipped.empty() ? "" : ", ") + name;
          continue;
        }
        std::string path = std::string("/sdcard/") + name;
        struct stat st{};
        if (stat(path.c_str(), &st) != 0) {
          ESP_LOGW(TAG_LOGMGR, "stat('%s') failed (errno %d) — omitted from listing",
                   path.c_str(), errno);
          continue;
        }
        files.push_back({name, static_cast<size_t>(st.st_size), fat_mtime_to_utc(st.st_mtime)});
      }
      closedir(dir);
      ESP_LOGI(TAG_LOGMGR, "listed %u log file(s); skipped: [%s]",
               (unsigned) files.size(), skipped.c_str());
    }
    // Newest first — daily files sort naturally by name (YYYY-MM-DD). The two
    // undated files are pinned to the top: backup.json first (an operator
    // looking here is usually after the settings backup), then events.csv.
    auto rank = [](const std::string &n) {
      if (is_backup_filename(n))
        return 0;
      if (name_equals_ci(n, "events.csv"))
        return 1;
      return 2;
    };
    std::sort(files.begin(), files.end(), [&rank](const Entry &a, const Entry &b) {
      int ra = rank(a.name), rb = rank(b.name);
      if (ra != rb)
        return ra < rb;
      return a.name > b.name;
    });

    float free_mb = p4_sd_free_mb();
    float total_mb = p4_sd_total_mb();
    float used_mb = (total_mb > 0 && free_mb >= 0) ? (total_mb - free_mb) : -1.0f;
    const char *card = p4_sd_card_name();

    // Escape card name for JSON (CID names are short ASCII, but be safe).
    std::string card_json;
    for (const char *p = card; p && *p; ++p) {
      if (*p == '"' || *p == '\\') card_json += '\\';
      card_json += *p;
    }

    char stats[384];
    snprintf(stats, sizeof(stats),
             "{\"mounted\":true,\"card\":\"%s\",\"speed_khz\":%lu,"
             "\"free_mb\":%.1f,\"total_mb\":%.1f,\"used_mb\":%.1f,"
             "\"opens\":%lu,\"reads\":%lu,\"writes\":%lu,"
             "\"bytes_read\":%llu,\"bytes_written\":%llu,\"files\":[",
             card_json.c_str(),
             (unsigned long) p4_sd_speed_khz(),
             free_mb, total_mb, used_mb,
             (unsigned long) p4_sd_stat_opens(),
             (unsigned long) p4_sd_stat_reads(),
             (unsigned long) p4_sd_stat_writes(),
             (unsigned long long) p4_sd_stat_bytes_read(),
             (unsigned long long) p4_sd_stat_bytes_written());

    std::string json = stats;
    for (size_t i = 0; i < files.size(); i++) {
      if (i > 0)
        json += ",";
      json += "{\"name\":\"" + files[i].name + "\",\"size\":" + std::to_string(files[i].size) +
              ",\"mtime\":" + std::to_string((long long) files[i].mtime) +
              ",\"kind\":\"" + (is_backup_filename(files[i].name) ? "backup" : "log") + "\"}";
    }
    json += "]}";
    request->send(200, "application/json", json.c_str());
  }

  void handle_download_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    std::string name = request->arg("file");
    if (!is_downloadable_filename(name)) {
      request->send(400, "application/json", "{\"error\":\"invalid_filename\"}");
      return;
    }
    std::string path = std::string("/sdcard/") + name;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
      ESP_LOGW(TAG_LOGMGR, "stat('%s') failed (errno %d)", path.c_str(), errno);
      request->send(404, "application/json", "{\"error\":\"not_found\"}");
      return;
    }
    // Daily CSV logs are small (5-minute samples); a generous cap avoids an
    // unbounded read if a file somehow grew far larger than expected.
    static constexpr size_t MAX_DOWNLOAD_BYTES = 4 * 1024 * 1024;
    if (static_cast<size_t>(st.st_size) > MAX_DOWNLOAD_BYTES) {
      request->send(413, "application/json", "{\"error\":\"file_too_large\"}");
      return;
    }
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
      request->send(500, "application/json", "{\"error\":\"open_failed\"}");
      return;
    }
    std::string content;
    content.resize(static_cast<size_t>(st.st_size));
    size_t read = fread(content.data(), 1, content.size(), f);
    fclose(f);
    p4_sd_note_read(read);
    content.resize(read);

    auto *response = request->beginResponse(
        200, is_backup_filename(name) ? "application/json" : "text/csv", content);
    response->addHeader("Content-Disposition", ("attachment; filename=\"" + name + "\"").c_str());
    request->send(response);
  }

  void handle_delete_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    std::string name = request->arg("file");
    if (is_backup_filename(name)) {
      request->send(403, "application/json", "{\"error\":\"backup_not_deletable\"}");
      return;
    }
    if (!is_valid_log_filename(name)) {
      request->send(400, "application/json", "{\"error\":\"invalid_filename\"}");
      return;
    }
    std::string path = std::string("/sdcard/") + name;
    if (unlink(path.c_str()) == 0) {
      ESP_LOGI(TAG_LOGMGR, "Deleted log file: %s", name.c_str());
      request->send(200, "application/json", "{\"ok\":true}");
    } else {
      ESP_LOGW(TAG_LOGMGR, "Failed to delete log file: %s", name.c_str());
      request->send(500, "application/json", "{\"ok\":false}");
    }
  }
};

}  // namespace p4_logs
