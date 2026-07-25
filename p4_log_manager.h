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
//   GET  /logs                     -> JSON [{"name":"...","size":N}, ...]
//   GET  /logs/download?file=NAME  -> raw file content, attachment download
//   POST /logs/delete?file=NAME    -> {"ok":true|false}
//
// NAME must be "events.csv" or "YYYY-MM-DD.csv" (see is_valid_log_filename)
// — this is the only defense against path traversal, so it is intentionally
// strict rather than a blocklist of "..", slashes, etc.
// ============================================================================

#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <esp_log.h>
#include "esphome/components/web_server_base/web_server_base.h"
#include "p4_logging.h"

namespace p4_logs {

static const char *const TAG_LOGMGR = "p4_logmgr";

inline bool is_valid_log_filename(const std::string &name) {
  if (name == "events.csv")
    return true;
  if (name.size() != 14)  // "YYYY-MM-DD.csv"
    return false;
  if (name.compare(10, 4, ".csv") != 0)
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
    }
  }

  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void handle_list_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    if (!p4_sd_is_ready()) {
      request->send(503, "application/json", "{\"error\":\"sd_not_mounted\"}");
      return;
    }
    std::vector<std::pair<std::string, size_t>> files;
    DIR *dir = opendir("/sdcard");
    if (dir != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (!is_valid_log_filename(name))
          continue;
        std::string path = std::string("/sdcard/") + name;
        struct stat st{};
        if (stat(path.c_str(), &st) != 0)
          continue;
        files.emplace_back(name, static_cast<size_t>(st.st_size));
      }
      closedir(dir);
    }
    // Newest first — daily files sort naturally by name (YYYY-MM-DD), and
    // events.csv (no date) is pinned first since it's the most-referenced.
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
      if (a.first == "events.csv")
        return true;
      if (b.first == "events.csv")
        return false;
      return a.first > b.first;
    });

    std::string json = "[";
    for (size_t i = 0; i < files.size(); i++) {
      if (i > 0)
        json += ",";
      json += "{\"name\":\"" + files[i].first + "\",\"size\":" + std::to_string(files[i].second) + "}";
    }
    json += "]";
    request->send(200, "application/json", json.c_str());
  }

  void handle_download_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    std::string name = request->arg("file");
    if (!is_valid_log_filename(name)) {
      request->send(400, "application/json", "{\"error\":\"invalid_filename\"}");
      return;
    }
    std::string path = std::string("/sdcard/") + name;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
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
    content.resize(read);

    auto *response = request->beginResponse(200, "text/csv", content);
    response->addHeader("Content-Disposition", ("attachment; filename=\"" + name + "\"").c_str());
    request->send(response);
  }

  void handle_delete_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    std::string name = request->arg("file");
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
