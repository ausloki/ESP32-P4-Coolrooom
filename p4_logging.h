#pragma once
// ============================================================================
// p4_logging.h — ESP32-P4 Coolroom Controller: Phase 5 Logging & Backup
//
// PRINCIPLE: All SD card, backup/restore, and notification helpers live here.
// YAML lambdas call these; they never contain business logic.
//
// SD card uses SDMMC host slot 1 (GPIO 39-44) — separate from WiFi SDIO slot 0.
// Mount point: /sdcard
//
// Log layout on SD card:
//   /sdcard/logs/YYYY-MM-DD.csv   — daily temperature + state log (appended)
//   /sdcard/events.csv            — alarm / fault / defrost events (appended)
//   /sdcard/backup.json           — last saved control parameters
//
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cmath>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <esp_log.h>
#include <esp_err.h>
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "p4_helpers.h"

static const char* TAG_SD = "p4_sd";

// ─── SD mount state ────────────────────────────────────────────────────────

static sdmmc_card_t* p4_sd_card = nullptr;
static bool          p4_sd_ready = false;

// ─── Mount / unmount ───────────────────────────────────────────────────────

/// Mount the TF card on SDMMC slot 1 (GPIO 39-44).
/// Returns true on success. Safe to call multiple times (no-op if already mounted).
inline bool p4_sd_mount() {
    if (p4_sd_ready) return true;

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        .format_if_mount_failed = false,
        .max_files              = 8,
        .allocation_unit_size   = 16 * 1024,
        .disk_status_check_enable = false,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot         = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20 MHz

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

    // Ensure log directory exists
    mkdir("/sdcard/logs", 0755);

    ESP_LOGI(TAG_SD, "SD card mounted. Name: %s, Speed: %u kHz, Size: %llu MB",
             p4_sd_card->cid.name,
             p4_sd_card->max_freq_khz,
             ((uint64_t)p4_sd_card->csd.capacity) * p4_sd_card->csd.sector_size / (1024 * 1024));
    p4_sd_ready = true;
    return true;
}

/// Unmount the SD card cleanly (call before physical removal).
inline void p4_sd_unmount() {
    if (!p4_sd_ready) return;
    esp_vfs_fat_sdcard_unmount("/sdcard", p4_sd_card);
    p4_sd_card  = nullptr;
    p4_sd_ready = false;
    ESP_LOGI(TAG_SD, "SD card unmounted");
}

/// True if card is currently mounted and accessible.
inline bool p4_sd_is_ready() { return p4_sd_ready; }

// ─── Temperature logging ───────────────────────────────────────────────────

/// Append one CSV row to the daily log file (/sdcard/logs/YYYY-MM-DD.csv).
/// Creates the file with a header row if it does not exist.
/// coolroom_c / evap_c / ambient_c: NaN is written as empty field.
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

    // Build date-stamped filename
    char path[48];
    {
        struct timeval tv{};
        gettimeofday(&tv, nullptr);
        struct tm t{};
        localtime_r(&tv.tv_sec, &t);
        snprintf(path, sizeof(path), "/sdcard/logs/%04d-%02d-%02d.csv",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }

    // Create with header if new
    struct stat st{};
    bool is_new = (stat(path, &st) != 0);
    FILE* f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG_SD, "Cannot open %s", path);
        return false;
    }
    if (is_new) {
        fputs("timestamp,coolroom_c,evap_c,ambient_c,setpoint_c,"
              "compressor,defrost,alarm_hi,alarm_lo,probe_fault\n", f);
    }

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));

    // Format each temperature (empty string for NaN)
    auto fmtf = [](char* buf, size_t n, float v) {
        if (std::isfinite(v)) snprintf(buf, n, "%.1f", v);
        else snprintf(buf, n, "");
    };
    char sc[12], ec[12], ac[12], sp[12];
    fmtf(sc, sizeof(sc), coolroom_c);
    fmtf(ec, sizeof(ec), evap_c);
    fmtf(ac, sizeof(ac), ambient_c);
    fmtf(sp, sizeof(sp), setpoint_c);

    fprintf(f, "%s,%s,%s,%s,%s,%d,%d,%d,%d,%d\n",
            ts, sc, ec, ac, sp,
            (int)compressor_on, (int)defrost_on,
            (int)alarm_hi, (int)alarm_lo, (int)probe_fault);
    fclose(f);
    return true;
}

// ─── Event logging ─────────────────────────────────────────────────────────

/// Append an event line to /sdcard/events.csv.
/// event_type: e.g. "ALARM_HI", "ALARM_CLEAR", "DEFROST_START", "PROBE_FAULT"
/// detail: optional extra info, e.g. temperature value string.
inline bool p4_sd_log_event(const char* event_type, const char* detail = "") {
    if (!p4_sd_ready) return false;

    static bool header_written = false;
    FILE* f = fopen("/sdcard/events.csv", "a");
    if (!f) {
        ESP_LOGE(TAG_SD, "Cannot open events.csv");
        return false;
    }

    if (!header_written) {
        // Check if file was empty (new)
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0) fputs("timestamp,event,detail\n", f);
        header_written = true;
    }

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));
    fprintf(f, "%s,%s,%s\n", ts, event_type, detail);
    fclose(f);
    ESP_LOGI(TAG_SD, "Event logged: %s %s", event_type, detail);
    return true;
}

// ─── Backup / restore control parameters ──────────────────────────────────

/// Write control parameters to /sdcard/backup.json.
/// Called manually (button) or after each setpoint change.
inline bool p4_sd_backup_params(
    float setpoint,
    float comp_diff,
    float alarm_high,
    float alarm_low
) {
    if (!p4_sd_ready) return false;

    FILE* f = fopen("/sdcard/backup.json", "w");
    if (!f) {
        ESP_LOGE(TAG_SD, "Cannot open backup.json for write");
        return false;
    }

    char ts[24];
    p4_fmt_time(ts, sizeof(ts));
    fprintf(f,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"setpoint\":   %.1f,\n"
        "  \"comp_diff\":  %.1f,\n"
        "  \"alarm_high\": %.1f,\n"
        "  \"alarm_low\":  %.1f\n"
        "}\n",
        ts, setpoint, comp_diff, alarm_high, alarm_low);
    fclose(f);
    ESP_LOGI(TAG_SD, "Params backed up: SP=%.1f diff=%.1f hi=%.1f lo=%.1f",
             setpoint, comp_diff, alarm_high, alarm_low);
    return true;
}

/// Read /sdcard/backup.json and populate output parameters.
/// Returns true if file was found and all 4 values parsed successfully.
/// On failure, output pointers are unchanged — caller keeps existing NVS values.
inline bool p4_sd_restore_params(
    float& setpoint,
    float& comp_diff,
    float& alarm_high,
    float& alarm_low
) {
    if (!p4_sd_ready) return false;

    FILE* f = fopen("/sdcard/backup.json", "r");
    if (!f) {
        ESP_LOGW(TAG_SD, "backup.json not found");
        return false;
    }

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    // Simple sscanf-based parse (no external JSON library needed)
    float sp = NAN, cd = NAN, ah = NAN, al = NAN;
    // Match each key explicitly
    auto parse_field = [&](const char* key, float& out) {
        const char* p = strstr(buf, key);
        if (p) {
            p = strchr(p, ':');
            if (p) out = strtof(p + 1, nullptr);
        }
    };
    parse_field("\"setpoint\"", sp);
    parse_field("\"comp_diff\"", cd);
    parse_field("\"alarm_high\"", ah);
    parse_field("\"alarm_low\"", al);

    if (!std::isfinite(sp) || !std::isfinite(cd) ||
        !std::isfinite(ah) || !std::isfinite(al)) {
        ESP_LOGE(TAG_SD, "backup.json parse failed");
        return false;
    }

    setpoint   = sp;
    comp_diff  = cd;
    alarm_high = ah;
    alarm_low  = al;
    ESP_LOGI(TAG_SD, "Params restored: SP=%.1f diff=%.1f hi=%.1f lo=%.1f",
             sp, cd, ah, al);
    return true;
}

// ─── SD card free space ────────────────────────────────────────────────────

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
