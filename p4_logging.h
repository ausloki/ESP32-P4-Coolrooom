#pragma once
// ============================================================================
// p4_logging.h — ESP32-P4 Coolroom Controller: Phase 5 Logging & Backup
//
// PRINCIPLE: All SD card, backup/restore, and notification helpers live here.
// YAML lambdas call these; they never contain business logic.
//
// SD card uses SDMMC host slot 0 (GPIO 39-44, IO-MUX / UHS pins).
// ESP-Hosted WiFi (ESP32-C6) uses SDMMC host slot 1 (GPIO 14-19).
// Matching Waveshare BSP esp32_p4_wifi6_touch_lcd_7b + Espressif
// host_sdcard_with_hosted: both peripherals must use different slots.
// Mount point: /sdcard
//
// Log layout on SD card:
//   /sdcard/YYYY-MM-DD.csv        — daily temperature + state + CPU % log
//                                   (cols: timestamp, temps, relays/alarms,
//                                   cpu_pct, cpu_c0_pct, cpu_c1_pct)
//   /sdcard/nodate.csv            — samples taken before wall clock is valid
//   /sdcard/events.csv            — alarm / fault / defrost events (appended)
//   /sdcard/backup.json           — last saved control parameters
//
// Safeguards:
//   - Daily YYYY-MM-DD.csv files older than the retention setting are pruned
//     (events.csv / nodate.csv / backup.json are never auto-deleted).
//   - Appends and backup writes are skipped when free space is below
//     P4_SD_MIN_FREE_MB (card stays "mounted OK" — full ≠ dead).
//
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <esp_log.h>
#include <esp_err.h>
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "p4_helpers.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

static const char* TAG_SD = "p4_sd";

// Soft I/O counters for lifespan estimation. Consumer SD cards do not expose
 // wear-level SMART data over SDMMC, so we count our own open/read/write
 // traffic instead — enough to see write rate and relative wear from logging
 // and backup. Reset on reboot (not persisted).
static uint32_t p4_sd_open_count   = 0;
static uint32_t p4_sd_read_count   = 0;
static uint32_t p4_sd_write_count  = 0;
static uint64_t p4_sd_bytes_read   = 0;
static uint64_t p4_sd_bytes_written = 0;

inline void p4_sd_note_open()  { ++p4_sd_open_count; }
inline void p4_sd_note_read(size_t n)  { ++p4_sd_read_count;  p4_sd_bytes_read    += n; }
inline void p4_sd_note_write(size_t n) { ++p4_sd_write_count; p4_sd_bytes_written += n; }

// ─── Live event ring (web Events tab) ──────────────────────────────────────
// The SD events.csv is the durable record; this ring is what the dashboard
// polls so an operator can watch alarms/errors land without downloading a
// file. Capacity is deliberately small — a few minutes of dense activity.

static constexpr size_t P4_EVENT_RING_CAP = 80;
static constexpr size_t P4_EVENT_TYPE_LEN = 28;
static constexpr size_t P4_EVENT_DETAIL_LEN = 96;

struct P4EventRingEntry {
  char ts[24];
  char type[P4_EVENT_TYPE_LEN];
  char detail[P4_EVENT_DETAIL_LEN];
};

static P4EventRingEntry p4_event_ring[P4_EVENT_RING_CAP];
static size_t p4_event_ring_head = 0;   // next write index
static size_t p4_event_ring_count = 0;  // 0..CAP
static uint32_t p4_event_ring_seq = 0;  // monotonic, for "since" polling
// Prebuilt full-ring JSON. The HTTP handler must not build into a stack
// temporary whose .c_str() dies before the IDF web server finishes the
// write — same pattern as p4_wifi's status_json.
static std::string p4_event_ring_full_json{"{\"seq\":0,\"events\":[]}"};

inline void p4_event_ring_rebuild_json_() {
  std::string events = "[";
  if (p4_event_ring_count > 0) {
    size_t start = (p4_event_ring_head + P4_EVENT_RING_CAP - p4_event_ring_count) % P4_EVENT_RING_CAP;
    uint32_t oldest_seq = p4_event_ring_seq - (uint32_t) p4_event_ring_count + 1;
    auto esc = [](const char *s) -> std::string {
      std::string o;
      for (; s && *s; ++s) {
        if (*s == '"' || *s == '\\') { o += '\\'; o += *s; }
        else if ((unsigned char)*s < 0x20) { /* drop controls */ }
        else o += *s;
      }
      return o;
    };
    for (size_t i = 0; i < p4_event_ring_count; i++) {
      if (i) events += ",";
      const P4EventRingEntry &e = p4_event_ring[(start + i) % P4_EVENT_RING_CAP];
      uint32_t seq = oldest_seq + (uint32_t) i;
      events += "{\"seq\":" + std::to_string(seq) +
                ",\"ts\":\"" + esc(e.ts) +
                "\",\"event\":\"" + esc(e.type) +
                "\",\"detail\":\"" + esc(e.detail) + "\"}";
    }
  }
  events += "]";
  p4_event_ring_full_json = std::string("{\"seq\":") + std::to_string(p4_event_ring_seq) +
                            ",\"events\":" + events + "}";
}

inline void p4_event_ring_push(const char *event_type, const char *detail) {
  P4EventRingEntry &e = p4_event_ring[p4_event_ring_head];
  p4_fmt_time(e.ts, sizeof(e.ts));
  snprintf(e.type, sizeof(e.type), "%s", event_type != nullptr ? event_type : "");
  snprintf(e.detail, sizeof(e.detail), "%s", detail != nullptr ? detail : "");
  p4_event_ring_head = (p4_event_ring_head + 1) % P4_EVENT_RING_CAP;
  if (p4_event_ring_count < P4_EVENT_RING_CAP) p4_event_ring_count++;
  p4_event_ring_seq++;
  p4_event_ring_rebuild_json_();
}

/// Snapshot of the live ring for the Events tab. Filter client-side with since.
inline const std::string &p4_event_ring_json() { return p4_event_ring_full_json; }

// ─── SD mount state ────────────────────────────────────────────────────────

static sdmmc_card_t* p4_sd_card = nullptr;
static bool          p4_sd_ready = false;
#if SOC_SDMMC_IO_POWER_EXTERNAL
// ESP32-P4's high-speed SDMMC pins draw card VDD from an on-chip LDO rather
// than a fixed board rail -- confirmed against Waveshare's own SD example
// for this board (examples/ESP-IDF/04_sdmmc), which enables this by default
// with LDO channel 4. Without powering this LDO first, the card's I/O lines
// never get real power and the mount silently fails/behaves unreliably even
// though the GPIO pin assignments themselves are correct. Initialized once
// (the driver handle persists across remounts) rather than per-mount-call.
static sd_pwr_ctrl_handle_t p4_sd_pwr_ctrl_handle = nullptr;
#endif
// True once esp_vfs_fat_sdmmc_mount() has succeeded, false only after an
// explicit unmount — deliberately tracked separately from p4_sd_ready.
// p4_sd_ready can go false at runtime (p4_sd_mark_failed(), a write
// failure) while the VFS mount point is still technically registered with
// ESP-IDF; a remount attempt must clean that up first or
// esp_vfs_fat_sdmmc_mount() will simply fail again for "already mounted".
static bool          p4_sd_vfs_registered = false;

// ─── Mount / unmount ───────────────────────────────────────────────────────

/// Mount the TF card on SDMMC slot 0 (GPIO 39-44 IO-MUX).
/// Returns true on success. Safe to call repeatedly — a no-op if already
/// mounted and healthy, and cleans up a stale (failed-but-still-registered)
/// mount before retrying, so this also serves as the auto-remount entry
/// point after p4_sd_mark_failed() or a boot-time mount failure.
inline bool p4_sd_mount() {
    if (p4_sd_ready) return true;

    if (p4_sd_vfs_registered) {
        esp_vfs_fat_sdcard_unmount("/sdcard", p4_sd_card);
        p4_sd_card = nullptr;
        p4_sd_vfs_registered = false;
    }

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        .format_if_mount_failed = false,
        .max_files              = 8,
        .allocation_unit_size   = 16 * 1024,
        .disk_status_check_enable = false,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // Slot 0 = TF card (IO-MUX pins 39-44). Slot 1 = ESP-Hosted C6 WiFi
    // (GPIO matrix 14-19). Waveshare BSP + Espressif host_sdcard_with_hosted
    // both use this split — sharing slot 1 broke the hosted link.
    host.slot         = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20 MHz

#if SOC_SDMMC_IO_POWER_EXTERNAL
    if (p4_sd_pwr_ctrl_handle == nullptr) {
        sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = 4 };
        esp_err_t pwr_err = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &p4_sd_pwr_ctrl_handle);
        if (pwr_err != ESP_OK) {
            ESP_LOGE(TAG_SD, "Failed to init SD I/O LDO power control (chan 4): %s",
                     esp_err_to_name(pwr_err));
            return false;
        }
    }
    host.pwr_ctrl_handle = p4_sd_pwr_ctrl_handle;
#endif

    // Slot 0 on ESP32-P4 is IO-MUX (fixed pins). Waveshare BSP leaves the
    // pin fields unset; we still set them explicitly for clarity — they
    // must match the board's TF wiring (39-44).
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk   = GPIO_NUM_43;
    slot.cmd   = GPIO_NUM_44;
    slot.d0    = GPIO_NUM_39;
    slot.d1    = GPIO_NUM_40;
    slot.d2    = GPIO_NUM_41;
    slot.d3    = GPIO_NUM_42;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mnt, &p4_sd_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SD, "Mount failed: %s", esp_err_to_name(err));
        p4_sd_ready = false;
        return false;
    }

    ESP_LOGI(TAG_SD, "SD card mounted. Name: %s, Speed: %" PRIu32 " kHz, Size: %" PRIu64 " MB",
             p4_sd_card->cid.name,
             p4_sd_card->max_freq_khz,
             ((uint64_t)p4_sd_card->csd.capacity) * p4_sd_card->csd.sector_size / (1024 * 1024));
    p4_sd_vfs_registered = true;
    p4_sd_ready = true;
    return true;
}

/// Unmount the SD card cleanly (call before physical removal). Unlike
/// p4_sd_mount()'s internal cleanup, this is for a deliberate, intentional
/// unmount — gated on p4_sd_vfs_registered (not p4_sd_ready) so it still
/// works after a runtime failure has already cleared p4_sd_ready.
inline void p4_sd_unmount() {
    if (!p4_sd_vfs_registered) return;
    esp_vfs_fat_sdcard_unmount("/sdcard", p4_sd_card);
    p4_sd_card  = nullptr;
    p4_sd_vfs_registered = false;
    p4_sd_ready = false;
    ESP_LOGI(TAG_SD, "SD card unmounted");
}

/// True if card is currently mounted and accessible.
inline bool p4_sd_is_ready() { return p4_sd_ready; }

/// Marks the card as no longer usable after a write-path failure discovered
/// at runtime (e.g. physically removed while running). Read-path "file not
/// found" (e.g. no backup.json yet) is NOT failure — callers must not call
/// this for that case. The control loop watches p4_sd_is_ready() for the
/// true→false edge to fire an ntfy alert and flip the sd_card_online
/// diagnostic in real time, instead of that entity silently staying "true"
/// forever after a runtime failure.
/// p4_sd_vfs_registered deliberately stays true here — the mount point is
/// still registered with ESP-IDF even though I/O is failing; the next
/// p4_sd_mount() call (auto-remount) cleans that up before retrying.
inline void p4_sd_mark_failed(const char* where) {
    if (!p4_sd_ready) return;
    p4_sd_ready = false;
    ESP_LOGE(TAG_SD, "SD card write failed in %s — marking card as failed/unavailable", where);
}

/// Cheap write probe used to tell "this one filename could not be opened"
/// apart from "the card is gone". A healthy card can still refuse a name the
/// filesystem cannot represent, and treating that as a dead card took
/// logging, backup and restore offline on every log tick — then the
/// auto-remount watchdog brought it straight back, flapping the SD
/// diagnostic and the ntfy fault/recovery pair. The probe name is 8.3-safe
/// so it succeeds even on a volume built without long-filename support.
inline bool p4_sd_probe_writable() {
    FILE* p = fopen("/sdcard/sdprobe.tmp", "w");
    if (!p) return false;
    fclose(p);
    remove("/sdcard/sdprobe.tmp");
    return true;
}

/// Handles a failed fopen() on the card: marks the card failed only when the
/// card itself is genuinely unwritable, and otherwise reports the offending
/// path and leaves the rest of the SD features running.
inline bool p4_sd_handle_open_failure(const char* where, const char* path) {
    if (p4_sd_probe_writable()) {
        ESP_LOGE(TAG_SD,
                 "Cannot open %s in %s — card is still writable, so this is a "
                 "filename/filesystem problem (check FATFS long-filename support)",
                 path, where);
        return false;
    }
    ESP_LOGE(TAG_SD, "Cannot open %s", path);
    p4_sd_mark_failed(where);
    return false;
}

// ─── Free space + write gate + daily-log prune ─────────────────────────────

/// Minimum free space (MB) required before appending logs or writing backup.
/// Fixed threshold — not a user setting. Documented in USER_MANUAL §4.9.
static constexpr float P4_SD_MIN_FREE_MB = 32.0f;

/// Latched for the current low-space episode so we only emit SD_SPACE_LOW once
/// until free space recovers above the threshold (e.g. after prune / delete).
static bool p4_sd_space_low_latched = false;

/// Day-key of the last successful prune pass (year<<9 | yday). -1 = never.
static int p4_sd_last_prune_day_key = -1;

/// Returns free space on the SD card in megabytes, or -1 if not mounted.
inline float p4_sd_free_mb() {
    if (!p4_sd_ready) return -1.0f;
    FATFS* fs;
    DWORD  free_clust;
    if (f_getfree("0:", &free_clust, &fs) != FR_OK) return -1.0f;
    const uint64_t free_bytes =
        (uint64_t)free_clust * fs->csize * 512UL;  // 512 bytes per sector (standard)
    return static_cast<float>(free_bytes) / (1024.0f * 1024.0f);
}

/// Returns total SD capacity in megabytes, or -1 if not mounted.
inline float p4_sd_total_mb() {
    if (!p4_sd_ready || p4_sd_card == nullptr) return -1.0f;
    const uint64_t total_bytes =
        ((uint64_t)p4_sd_card->csd.capacity) * p4_sd_card->csd.sector_size;
    return static_cast<float>(total_bytes) / (1024.0f * 1024.0f);
}

/// True when the card has enough free space for a log/backup write.
/// On the first low reading of an episode, pushes SD_SPACE_LOW to the live
/// event ring (not events.csv — that write is also gated). Does not flip
/// sd_card_ok / p4_sd_ready: a full card is still mounted and readable.
inline bool p4_sd_allow_write(const char* where) {
    if (!p4_sd_ready) return false;
    const float free_mb = p4_sd_free_mb();
    // If free-space query fails, do not block writes — the fopen path still
    // has p4_sd_handle_open_failure() for genuine I/O failure.
    if (free_mb < 0.0f) return true;
    if (free_mb >= P4_SD_MIN_FREE_MB) {
        if (p4_sd_space_low_latched) {
            p4_sd_space_low_latched = false;
            ESP_LOGI(TAG_SD, "SD free space recovered (%.1f MB)", free_mb);
        }
        return true;
    }
    if (!p4_sd_space_low_latched) {
        p4_sd_space_low_latched = true;
        char detail[56];
        snprintf(detail, sizeof(detail), "free=%.0fMB min=%.0fMB",
                 free_mb, P4_SD_MIN_FREE_MB);
        ESP_LOGW(TAG_SD,
                 "SD free space low (%.1f MB < %.0f MB) — skipping write in %s",
                 free_mb, P4_SD_MIN_FREE_MB, where ? where : "?");
        p4_event_ring_push("SD_SPACE_LOW", detail);
    }
    return false;
}

/// True if name is exactly YYYY-MM-DD.csv with plausible digits.
inline bool p4_sd_is_daily_temp_log_name(const char* name, int* y, int* m, int* d) {
    if (name == nullptr) return false;
    if (std::strlen(name) != 14) return false;
    if (name[4] != '-' || name[7] != '-' || std::strcmp(name + 10, ".csv") != 0)
        return false;
    static const int digit_idx[] = {0, 1, 2, 3, 5, 6, 8, 9};
    for (int idx : digit_idx) {
        if (name[idx] < '0' || name[idx] > '9') return false;
    }
    const int yy = (name[0] - '0') * 1000 + (name[1] - '0') * 100 +
                   (name[2] - '0') * 10 + (name[3] - '0');
    const int mm = (name[5] - '0') * 10 + (name[6] - '0');
    const int dd = (name[8] - '0') * 10 + (name[9] - '0');
    if (yy < 2020 || yy > 2100 || mm < 1 || mm > 12 || dd < 1 || dd > 31)
        return false;
    if (y) *y = yy;
    if (m) *m = mm;
    if (d) *d = dd;
    return true;
}

/// Days since civil 1970-01-01 (UTC-independent calendar arithmetic).
inline int p4_sd_civil_days(int y, int m, int d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy =
        (153U * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2U) / 5U +
        static_cast<unsigned>(d) - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

/// Delete /sdcard/YYYY-MM-DD.csv files older than keep_days.
/// Does not touch events.csv, nodate.csv, or backup.json.
/// Returns the number of files deleted, or -1 if the card is not ready /
/// the wall clock is not trustworthy (avoids mass-delete under epoch time).
inline int p4_sd_prune_temp_logs(int keep_days) {
    if (!p4_sd_ready) return -1;
    if (keep_days < 1) keep_days = 1;

    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    struct tm now_tm{};
    localtime_r(&tv.tv_sec, &now_tm);
    const int year = now_tm.tm_year + 1900;
    if (year < 2020) {
        ESP_LOGW(TAG_SD, "Skipping temp-log prune — wall clock not set");
        return -1;
    }

    const int today = p4_sd_civil_days(year, now_tm.tm_mon + 1, now_tm.tm_mday);
    DIR* dir = opendir("/sdcard");
    if (dir == nullptr) {
        ESP_LOGE(TAG_SD, "Cannot opendir /sdcard for prune");
        return -1;
    }

    int deleted = 0;
    while (dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.') continue;
        int fy = 0, fm = 0, fd = 0;
        if (!p4_sd_is_daily_temp_log_name(ent->d_name, &fy, &fm, &fd)) continue;
        const int age = today - p4_sd_civil_days(fy, fm, fd);
        if (age < keep_days) continue;

        // Name already validated as exactly "YYYY-MM-DD.csv" (14 chars).
        char path[24];
        snprintf(path, sizeof(path), "/sdcard/%.14s", ent->d_name);
        if (unlink(path) == 0) {
            ++deleted;
            ESP_LOGI(TAG_SD, "Pruned old temp log %s (age=%d days, keep=%d)",
                     ent->d_name, age, keep_days);
        } else {
            ESP_LOGW(TAG_SD, "Failed to prune %s", path);
        }
    }
    closedir(dir);

    if (deleted > 0) {
        ESP_LOGI(TAG_SD, "Temp-log prune removed %d file(s), retention=%d days",
                 deleted, keep_days);
    }
    return deleted;
}

// ─── Temperature logging ───────────────────────────────────────────────────

/// Append one CSV row to the daily log file (/sdcard/YYYY-MM-DD.csv).
/// Creates the file with a header row if it does not exist.
/// coolroom_c / evap_c / ambient_c / cpu_*: NaN is written as empty field.
/// CPU % is sampled here via p4_cpu_usage_* (same helpers as Info/web sensors).
inline bool p4_sd_log_temps(
    float coolroom_c,
    float evap_c,
    float ambient_c,
    float setpoint_c,
    bool  compressor_on,
    bool  defrost_on,
    bool  alarm_hi,
    bool  alarm_lo,
    bool  probe_fault
) {
    if (!p4_sd_ready) return false;
    if (!p4_sd_allow_write("p4_sd_log_temps")) return false;

    // Build date-stamped filename. Before NTP/RTC sync the clock reads 1970,
    // and dating a day's readings 1970-01-01 buries real data in a file that
    // every later boot appends to as well — park those rows in nodate.csv
    // until the clock is trustworthy instead.
    char path[48];
    {
        struct timeval tv{};
        gettimeofday(&tv, nullptr);
        struct tm t{};
        localtime_r(&tv.tv_sec, &t);
        const int year = t.tm_year + 1900;
        if (year < 2020)
            snprintf(path, sizeof(path), "/sdcard/nodate.csv");
        else
            snprintf(path, sizeof(path), "/sdcard/%04d-%02d-%02d.csv",
                     year, t.tm_mon + 1, t.tm_mday);
    }

    // Create with header if new
    struct stat st{};
    bool is_new = (stat(path, &st) != 0);
    FILE* f = fopen(path, "a");
    if (!f)
        return p4_sd_handle_open_failure("p4_sd_log_temps", path);
    p4_sd_note_open();
    if (is_new) {
        fputs("timestamp,coolroom_c,evap_c,ambient_c,setpoint_c,"
              "compressor,defrost,alarm_hi,alarm_lo,probe_fault,"
              "cpu_pct,cpu_c0_pct,cpu_c1_pct\n", f);
    }

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));

    // Format each float (empty string for NaN)
    auto fmtf = [](char* buf, size_t n, float v) {
        if (std::isfinite(v)) snprintf(buf, n, "%.1f", v);
        else if (n > 0) buf[0] = '\0';
    };
    char sc[12], ec[12], ac[12], sp[12];
    char cpu[12], c0[12], c1[12];
    fmtf(sc, sizeof(sc), coolroom_c);
    fmtf(ec, sizeof(ec), evap_c);
    fmtf(ac, sizeof(ac), ambient_c);
    fmtf(sp, sizeof(sp), setpoint_c);
    fmtf(cpu, sizeof(cpu), p4_cpu_usage_pct());
    fmtf(c0, sizeof(c0), p4_cpu_usage_core_pct(0));
    fmtf(c1, sizeof(c1), p4_cpu_usage_core_pct(1));

    int written = fprintf(f, "%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%s,%s,%s\n",
            ts, sc, ec, ac, sp,
            (int)compressor_on, (int)defrost_on,
            (int)alarm_hi, (int)alarm_lo, (int)probe_fault,
            cpu, c0, c1);
    if (written > 0) p4_sd_note_write((size_t) written);
    fclose(f);
    return true;
}

// ─── Event logging ─────────────────────────────────────────────────────────

/// Append an event line to /sdcard/events.csv.
/// event_type: e.g. "ALARM_HI", "ALARM_CLEAR", "DEFROST_START", "PROBE_FAULT"
/// detail: optional extra info, e.g. temperature value string.
inline bool p4_sd_log_event(const char* event_type, const char* detail = "") {
    // Always land in the live ring — even when the SD card is missing — so
    // the Events tab still shows what just happened.
    p4_event_ring_push(event_type, detail);

    if (!p4_sd_ready) return false;
    if (!p4_sd_allow_write("p4_sd_log_event")) return false;

    static bool header_written = false;
    FILE* f = fopen("/sdcard/events.csv", "a");
    if (!f)
        return p4_sd_handle_open_failure("p4_sd_log_event", "/sdcard/events.csv");
    p4_sd_note_open();

    if (!header_written) {
        // Check if file was empty (new)
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0) fputs("timestamp,event,detail\n", f);
        header_written = true;
    }

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));
    int written = fprintf(f, "%s,%s,%s\n", ts, event_type, detail);
    if (written > 0) p4_sd_note_write((size_t) written);
    fclose(f);
    ESP_LOGI(TAG_SD, "Event logged: %s %s", event_type, detail);
    return true;
}

/// Run prune at most once per local calendar day (or force=true after mount /
/// retention change). Logs SD_PRUNE when any file was removed.
inline void p4_sd_prune_temp_logs_if_due(int keep_days, bool force = false) {
    if (!p4_sd_ready) return;

    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    struct tm now_tm{};
    localtime_r(&tv.tv_sec, &now_tm);
    const int year = now_tm.tm_year + 1900;
    if (year < 2020) return;

    const int day_key = (year << 9) | now_tm.tm_yday;
    if (!force && day_key == p4_sd_last_prune_day_key) return;

    const int deleted = p4_sd_prune_temp_logs(keep_days);
    if (deleted >= 0)
        p4_sd_last_prune_day_key = day_key;
    if (deleted > 0) {
        char detail[48];
        snprintf(detail, sizeof(detail), "deleted=%d keep=%d", deleted, keep_days);
        p4_sd_log_event("SD_PRUNE", detail);
    }
}

// ─── Backup / restore control parameters ──────────────────────────────────

/// Write control parameters to /sdcard/backup.json.
/// Called manually (button) or after each setpoint change.
inline bool p4_sd_backup_params(
    float setpoint,
    float comp_diff,
    float alarm_high,
    float alarm_low,
    float comp_lockout_min,
    float defrost_grace_min,
    float defrost_interval_min,
    float defrost_duration_min,
    float defrost_drip_min,
    float defrost_end_c,
    float alarm_persist_min,
    float alarm_hysteresis_c,
    float door_alarm_delay_s,
    float no_cool_alarm_min,
    float ice_delta_c,
    float fallback_on_min,
    float fallback_off_min,
    float smart_delta_c,
    float smart_dwell_min,
    bool  probe2_enabled,
    bool  door_sensor_mode_is_nc,
    bool  door_sensor_enabled,
    bool  siren_enabled,
    bool  defrost_enabled,
    bool  fallback_enabled,
    bool  defrost_term_temp_enabled,
    bool  defrost_drip_enabled,
    bool  smart_defrost_enabled,
    bool  humidity_internal_enabled,
    bool  humidity_external_enabled,
    float probe1_offset_c,
    float probe2_offset_c,
    bool  dew_point_trigger_enabled,
    float startup_grace_min,
    bool  door_light_enabled,
    bool  door_hold_compressor
) {
    if (!p4_sd_ready) {
        ESP_LOGW(TAG_SD, "SD not ready for backup — attempting remount");
        if (!p4_sd_mount()) {
            ESP_LOGE(TAG_SD, "Cannot write backup.json — SD card not mounted");
            return false;
        }
    }
    if (!p4_sd_allow_write("p4_sd_backup_params")) {
        ESP_LOGW(TAG_SD, "Cannot write backup.json — free space below %.0f MB",
                 P4_SD_MIN_FREE_MB);
        return false;
    }

    FILE* f = fopen("/sdcard/backup.json", "w");
    if (!f) {
        ESP_LOGW(TAG_SD, "backup.json open failed — remount and retry");
        p4_sd_unmount();
        if (p4_sd_mount())
            f = fopen("/sdcard/backup.json", "w");
    }
    if (!f)
        return p4_sd_handle_open_failure("p4_sd_backup_params", "/sdcard/backup.json");
    p4_sd_note_open();

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));
    int written = fprintf(f,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"setpoint\":   %.1f,\n"
        "  \"comp_diff\":  %.1f,\n"
        "  \"alarm_high\": %.1f,\n"
        "  \"alarm_low\":  %.1f,\n"
        "  \"comp_lockout_min\": %.1f,\n"
        "  \"defrost_grace_min\": %.1f,\n"
        "  \"defrost_interval_min\": %.1f,\n"
        "  \"defrost_duration_min\": %.1f,\n"
        "  \"defrost_drip_min\": %.1f,\n"
        "  \"defrost_end_c\": %.1f,\n"
        "  \"alarm_persist_min\": %.1f,\n"
        "  \"alarm_hysteresis_c\": %.1f,\n"
        "  \"door_alarm_delay_s\": %.1f,\n"
        "  \"no_cool_alarm_min\": %.1f,\n"
        "  \"ice_delta_c\": %.1f,\n"
        "  \"fallback_on_min\": %.1f,\n"
        "  \"fallback_off_min\": %.1f,\n"
        "  \"smart_delta_c\": %.1f,\n"
        "  \"smart_dwell_min\": %.1f,\n"
        "  \"probe2_enabled\": %s,\n"
        "  \"door_sensor_mode_is_nc\": %s,\n"
        "  \"door_sensor_enabled\": %s,\n"
        "  \"siren_enabled\": %s,\n"
        "  \"defrost_enabled\": %s,\n"
        "  \"fallback_enabled\": %s,\n"
        "  \"defrost_term_temp_enabled\": %s,\n"
        "  \"defrost_drip_enabled\": %s,\n"
        "  \"smart_defrost_enabled\": %s,\n"
        "  \"humidity_internal_enabled\": %s,\n"
        "  \"humidity_external_enabled\": %s,\n"
        "  \"probe1_offset_c\": %.2f,\n"
        "  \"probe2_offset_c\": %.2f,\n"
        "  \"dew_point_trigger_enabled\": %s,\n"
        "  \"startup_grace_min\": %.1f,\n"
        "  \"door_light_enabled\": %s,\n"
        "  \"door_hold_compressor\": %s\n"
        "}\n",
        ts,
        setpoint, comp_diff, alarm_high, alarm_low,
        comp_lockout_min, defrost_grace_min, defrost_interval_min,
        defrost_duration_min, defrost_drip_min, defrost_end_c,
        alarm_persist_min, alarm_hysteresis_c, door_alarm_delay_s,
        no_cool_alarm_min, ice_delta_c, fallback_on_min,
        fallback_off_min, smart_delta_c, smart_dwell_min,
        probe2_enabled ? "true" : "false",
        door_sensor_mode_is_nc ? "true" : "false",
        door_sensor_enabled ? "true" : "false",
        siren_enabled ? "true" : "false",
        defrost_enabled ? "true" : "false",
        fallback_enabled ? "true" : "false",
        defrost_term_temp_enabled ? "true" : "false",
        defrost_drip_enabled ? "true" : "false",
        smart_defrost_enabled ? "true" : "false",
        humidity_internal_enabled ? "true" : "false",
        humidity_external_enabled ? "true" : "false",
        probe1_offset_c, probe2_offset_c,
        dew_point_trigger_enabled ? "true" : "false",
        startup_grace_min,
        door_light_enabled ? "true" : "false",
        door_hold_compressor ? "true" : "false");
    if (written > 0) p4_sd_note_write((size_t) written);
    fclose(f);
    ESP_LOGI(TAG_SD, "Params backed up: SP=%.1f diff=%.1f hi=%.1f lo=%.1f",
             setpoint, comp_diff, alarm_high, alarm_low);
    return true;
}

/// True if a settings backup is present on the card. Used at boot to report
/// whether the Restore button has anything to work with, without applying it.
inline bool p4_sd_backup_present(size_t* size_out = nullptr) {
    if (!p4_sd_ready) return false;
    struct stat st{};
    if (stat("/sdcard/backup.json", &st) != 0) return false;
    if (size_out != nullptr) *size_out = (size_t) st.st_size;
    return st.st_size > 0;
}

/// Read /sdcard/backup.json and populate output parameters.
/// Returns true if file was found and all 4 values parsed successfully.
/// On failure, output pointers are unchanged — caller keeps existing NVS values.
inline bool p4_sd_restore_params(
    float& setpoint,
    float& comp_diff,
    float& alarm_high,
    float& alarm_low,
    float& comp_lockout_min,
    float& defrost_grace_min,
    float& defrost_interval_min,
    float& defrost_duration_min,
    float& defrost_drip_min,
    float& defrost_end_c,
    float& alarm_persist_min,
    float& alarm_hysteresis_c,
    float& door_alarm_delay_s,
    float& no_cool_alarm_min,
    float& ice_delta_c,
    float& fallback_on_min,
    float& fallback_off_min,
    float& smart_delta_c,
    float& smart_dwell_min,
    bool&  probe2_enabled,
    bool&  door_sensor_mode_is_nc,
    bool&  door_sensor_enabled,
    bool&  siren_enabled,
    bool&  defrost_enabled,
    bool&  fallback_enabled,
    bool&  defrost_term_temp_enabled,
    bool&  defrost_drip_enabled,
    bool&  smart_defrost_enabled,
    bool&  humidity_internal_enabled,
    bool&  humidity_external_enabled,
    float& probe1_offset_c,
    float& probe2_offset_c,
    bool&  dew_point_trigger_enabled,
    float& startup_grace_min,
    bool&  door_light_enabled,
    bool&  door_hold_compressor
) {
    if (!p4_sd_ready) return false;

    FILE* f = fopen("/sdcard/backup.json", "r");
    if (!f) {
        ESP_LOGW(TAG_SD, "backup.json not found");
        return false;
    }
    p4_sd_note_open();

    // Sized well past the full field set (~840 B at last count) so every key
    // actually lands in buf — this was previously char buf[256], which
    // silently truncated the file before most of the bool fields and caused
    // them to never be found by strstr (restore quietly kept in-memory
    // defaults instead of the backed-up value). Discovered while adding the
    // humidity toggles below; fixed here since they'd otherwise be dead on
    // arrival too.
    char buf[1536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    p4_sd_note_read(n);
    buf[n] = '\0';

    // Simple sscanf-based parse (no external JSON library needed)
    float sp = NAN, cd = NAN, ah = NAN, al = NAN;
    float lockout = NAN, def_grace = NAN, def_int = NAN, def_dur = NAN;
    float def_drip = NAN, def_end = NAN, alarm_persist = NAN, alarm_hyst = NAN;
    float door_delay = NAN, no_cool = NAN, ice_delta = NAN;
    float fb_on = NAN, fb_off = NAN, smart_delta = NAN, smart_dwell = NAN;
    bool b_probe2 = probe2_enabled;
    bool b_mode_nc = door_sensor_mode_is_nc;
    bool b_door_en = door_sensor_enabled;
    bool b_siren_en = siren_enabled;
    bool b_defrost_en = defrost_enabled;
    bool b_fallback_en = fallback_enabled;
    bool b_def_term_en = defrost_term_temp_enabled;
    bool b_def_drip_en = defrost_drip_enabled;
    bool b_smart_def_en = smart_defrost_enabled;
    bool b_hum_int_en = humidity_internal_enabled;
    bool b_hum_ext_en = humidity_external_enabled;
    // Seeded with the caller's current value, not NAN — these three were
    // added after the JSON format was already in use, so an older
    // backup.json simply won't have these keys. Falling back to the current
    // value (rather than failing the whole restore) keeps old backups
    // restorable instead of breaking them retroactively.
    float p1_off = probe1_offset_c;
    float p2_off = probe2_offset_c;
    bool b_dew_trigger = dew_point_trigger_enabled;
    // Also seeded, not NAN, for the same reason: startup_grace_min was a
    // pre-existing setting that had simply never been added to backup/
    // restore until now (a real bug, not a new field), so older
    // backup.json files won't have this key either.
    // door_light_enabled: brand new setting, same seeding rationale.
    bool b_door_light_en = door_light_enabled;
    bool b_door_hold_en = door_hold_compressor;
    float startup_grace = startup_grace_min;
    // Match each key explicitly
    auto parse_field = [&](const char* key, float& out) {
        const char* p = strstr(buf, key);
        if (p) {
            p = strchr(p, ':');
            if (p) out = strtof(p + 1, nullptr);
        }
    };
    auto parse_bool = [&](const char* key, bool& out) {
        const char* p = strstr(buf, key);
        if (!p) return;
        p = strchr(p, ':');
        if (!p) return;
        p += 1;
        while (*p == ' ' || *p == '\t') ++p;
        if (strncmp(p, "true", 4) == 0) out = true;
        else if (strncmp(p, "false", 5) == 0) out = false;
    };

    parse_field("\"setpoint\"", sp);
    parse_field("\"comp_diff\"", cd);
    parse_field("\"alarm_high\"", ah);
    parse_field("\"alarm_low\"", al);
    parse_field("\"comp_lockout_min\"", lockout);
    parse_field("\"defrost_grace_min\"", def_grace);
    parse_field("\"defrost_interval_min\"", def_int);
    parse_field("\"defrost_duration_min\"", def_dur);
    parse_field("\"defrost_drip_min\"", def_drip);
    parse_field("\"defrost_end_c\"", def_end);
    parse_field("\"alarm_persist_min\"", alarm_persist);
    parse_field("\"alarm_hysteresis_c\"", alarm_hyst);
    parse_field("\"door_alarm_delay_s\"", door_delay);
    parse_field("\"no_cool_alarm_min\"", no_cool);
    parse_field("\"ice_delta_c\"", ice_delta);
    parse_field("\"fallback_on_min\"", fb_on);
    parse_field("\"fallback_off_min\"", fb_off);
    parse_field("\"smart_delta_c\"", smart_delta);
    parse_field("\"smart_dwell_min\"", smart_dwell);

    parse_bool("\"probe2_enabled\"", b_probe2);
    parse_bool("\"door_sensor_mode_is_nc\"", b_mode_nc);
    parse_bool("\"door_sensor_enabled\"", b_door_en);
    parse_bool("\"siren_enabled\"", b_siren_en);
    parse_bool("\"defrost_enabled\"", b_defrost_en);
    parse_bool("\"fallback_enabled\"", b_fallback_en);
    parse_bool("\"defrost_term_temp_enabled\"", b_def_term_en);
    parse_bool("\"defrost_drip_enabled\"", b_def_drip_en);
    parse_bool("\"smart_defrost_enabled\"", b_smart_def_en);
    parse_bool("\"humidity_internal_enabled\"", b_hum_int_en);
    parse_bool("\"humidity_external_enabled\"", b_hum_ext_en);
    parse_field("\"probe1_offset_c\"", p1_off);
    parse_field("\"probe2_offset_c\"", p2_off);
    parse_bool("\"dew_point_trigger_enabled\"", b_dew_trigger);
    parse_field("\"startup_grace_min\"", startup_grace);
    parse_bool("\"door_light_enabled\"", b_door_light_en);
    parse_bool("\"door_hold_compressor\"", b_door_hold_en);

    // Naming the offending key matters: a bare "parse failed" gives no way to
    // tell a missing key from a malformed value from a stale file, and every
    // one of those needs a different fix.
    const struct { const char* key; float value; } required[] = {
        {"setpoint", sp}, {"comp_diff", cd},
        {"alarm_high", ah}, {"alarm_low", al},
        {"comp_lockout_min", lockout}, {"defrost_grace_min", def_grace},
        {"defrost_interval_min", def_int}, {"defrost_duration_min", def_dur},
        {"defrost_drip_min", def_drip}, {"defrost_end_c", def_end},
        {"alarm_persist_min", alarm_persist}, {"alarm_hysteresis_c", alarm_hyst},
        {"door_alarm_delay_s", door_delay}, {"no_cool_alarm_min", no_cool},
        {"ice_delta_c", ice_delta}, {"fallback_on_min", fb_on},
        {"fallback_off_min", fb_off}, {"smart_delta_c", smart_delta},
        {"smart_dwell_min", smart_dwell},
    };
    bool ok = true;
    for (const auto& r : required) {
        if (!std::isfinite(r.value)) {
            ESP_LOGE(TAG_SD, "backup.json: key \"%s\" missing or not a number", r.key);
            ok = false;
        }
    }
    if (!ok) {
        ESP_LOGE(TAG_SD, "backup.json parse failed (%u bytes read)", (unsigned) n);
        return false;
    }

    setpoint   = sp;
    comp_diff  = cd;
    alarm_high = ah;
    alarm_low  = al;
    comp_lockout_min   = lockout;
    defrost_grace_min  = def_grace;
    defrost_interval_min = def_int;
    defrost_duration_min = def_dur;
    defrost_drip_min   = def_drip;
    defrost_end_c      = def_end;
    alarm_persist_min  = alarm_persist;
    alarm_hysteresis_c = alarm_hyst;
    door_alarm_delay_s = door_delay;
    no_cool_alarm_min  = no_cool;
    ice_delta_c        = ice_delta;
    fallback_on_min    = fb_on;
    fallback_off_min   = fb_off;
    smart_delta_c      = smart_delta;
    smart_dwell_min    = smart_dwell;
    probe2_enabled     = b_probe2;
    door_sensor_mode_is_nc = b_mode_nc;
    door_sensor_enabled = b_door_en;
    siren_enabled      = b_siren_en;
    defrost_enabled    = b_defrost_en;
    fallback_enabled   = b_fallback_en;
    defrost_term_temp_enabled = b_def_term_en;
    defrost_drip_enabled = b_def_drip_en;
    smart_defrost_enabled = b_smart_def_en;
    humidity_internal_enabled = b_hum_int_en;
    humidity_external_enabled = b_hum_ext_en;
    probe1_offset_c = p1_off;
    probe2_offset_c = p2_off;
    dew_point_trigger_enabled = b_dew_trigger;
    startup_grace_min = startup_grace;
    door_light_enabled = b_door_light_en;
    door_hold_compressor = b_door_hold_en;
    ESP_LOGI(TAG_SD, "Params restored: SP=%.1f diff=%.1f hi=%.1f lo=%.1f",
             sp, cd, ah, al);
    return true;
}

// ─── SD card identity / I/O stats ──────────────────────────────────────────

/// Card name from CID (empty string if not mounted).
inline const char* p4_sd_card_name() {
    if (!p4_sd_ready || p4_sd_card == nullptr) return "";
    return p4_sd_card->cid.name;
}

inline uint32_t p4_sd_speed_khz() {
    if (!p4_sd_ready || p4_sd_card == nullptr) return 0;
    return p4_sd_card->max_freq_khz;
}

inline uint32_t p4_sd_stat_opens()   { return p4_sd_open_count; }
inline uint32_t p4_sd_stat_reads()   { return p4_sd_read_count; }
inline uint32_t p4_sd_stat_writes()  { return p4_sd_write_count; }
inline uint64_t p4_sd_stat_bytes_read()    { return p4_sd_bytes_read; }
inline uint64_t p4_sd_stat_bytes_written() { return p4_sd_bytes_written; }
