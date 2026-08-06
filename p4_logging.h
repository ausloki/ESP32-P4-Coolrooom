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
//   /sdcard/YYYY-MM-DD.csv        — daily temperature + state log (appended)
//   /sdcard/events.csv            — alarm / fault / defrost events (appended)
//   /sdcard/backup.json           — last saved control parameters
//
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cmath>
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

/// Mount the TF card on SDMMC slot 1 (GPIO 39-44).
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
    host.slot         = SDMMC_HOST_SLOT_1;
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
/// at runtime (e.g. physically removed while running) — a successfully
/// mounted FATFS volume doesn't normally fail a plain fopen("a"/"w") unless
/// the underlying storage genuinely disappeared or corrupted, so this is
/// treated as conclusive. Read-path "file not found" (e.g. no backup.json
/// yet) is NOT failure — callers must not call this for that case. The
/// control loop watches p4_sd_is_ready() for the true→false edge to fire an
/// ntfy alert and flip the sd_card_online diagnostic in real time, instead
/// of that entity silently staying "true" forever after a runtime failure.
/// p4_sd_vfs_registered deliberately stays true here — the mount point is
/// still registered with ESP-IDF even though I/O is failing; the next
/// p4_sd_mount() call (auto-remount) cleans that up before retrying.
inline void p4_sd_mark_failed(const char* where) {
    if (!p4_sd_ready) return;
    p4_sd_ready = false;
    ESP_LOGE(TAG_SD, "SD card write failed in %s — marking card as failed/unavailable", where);
}

// ─── Temperature logging ───────────────────────────────────────────────────

/// Append one CSV row to the daily log file (/sdcard/YYYY-MM-DD.csv).
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
        snprintf(path, sizeof(path), "/sdcard/%04d-%02d-%02d.csv",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }

    // Create with header if new
    struct stat st{};
    bool is_new = (stat(path, &st) != 0);
    FILE* f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG_SD, "Cannot open %s", path);
        p4_sd_mark_failed("p4_sd_log_temps");
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
        else if (n > 0) buf[0] = '\0';
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
        p4_sd_mark_failed("p4_sd_log_event");
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
    float comp_min_run_min,
    float defrost_skip_below_c,
    float defrost_force_max_min,
    bool  defrost_skip_cold_enabled
) {
    if (!p4_sd_ready) return false;

    FILE* f = fopen("/sdcard/backup.json", "w");
    if (!f) {
        ESP_LOGE(TAG_SD, "Cannot open backup.json for write");
        p4_sd_mark_failed("p4_sd_backup_params");
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
        "  \"comp_min_run_min\": %.1f,\n"
        "  \"defrost_skip_below_c\": %.1f,\n"
        "  \"defrost_force_max_min\": %.1f,\n"
        "  \"defrost_skip_cold_enabled\": %s\n"
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
        comp_min_run_min, defrost_skip_below_c, defrost_force_max_min,
        defrost_skip_cold_enabled ? "true" : "false");
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
    float& comp_min_run_min,
    float& defrost_skip_below_c,
    float& defrost_force_max_min,
    bool&  defrost_skip_cold_enabled
) {
    if (!p4_sd_ready) return false;

    FILE* f = fopen("/sdcard/backup.json", "r");
    if (!f) {
        ESP_LOGW(TAG_SD, "backup.json not found");
        return false;
    }

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
    float startup_grace = startup_grace_min;
    float min_run = comp_min_run_min;
    float skip_below = defrost_skip_below_c;
    float force_max = defrost_force_max_min;
    bool b_skip_cold = defrost_skip_cold_enabled;
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
    parse_field("\"comp_min_run_min\"", min_run);
    parse_field("\"defrost_skip_below_c\"", skip_below);
    parse_field("\"defrost_force_max_min\"", force_max);
    parse_bool("\"defrost_skip_cold_enabled\"", b_skip_cold);

    if (!std::isfinite(sp) || !std::isfinite(cd) ||
        !std::isfinite(ah) || !std::isfinite(al) ||
        !std::isfinite(lockout) || !std::isfinite(def_grace) ||
        !std::isfinite(def_int) || !std::isfinite(def_dur) ||
        !std::isfinite(def_drip) || !std::isfinite(def_end) ||
        !std::isfinite(alarm_persist) || !std::isfinite(alarm_hyst) ||
        !std::isfinite(door_delay) || !std::isfinite(no_cool) ||
        !std::isfinite(ice_delta) || !std::isfinite(fb_on) ||
        !std::isfinite(fb_off) || !std::isfinite(smart_delta) ||
        !std::isfinite(smart_dwell)) {
        ESP_LOGE(TAG_SD, "backup.json parse failed");
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
    if (std::isfinite(min_run)) comp_min_run_min = min_run;
    if (std::isfinite(skip_below)) defrost_skip_below_c = skip_below;
    if (std::isfinite(force_max)) defrost_force_max_min = force_max;
    defrost_skip_cold_enabled = b_skip_cold;
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
