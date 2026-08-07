#pragma once
// ============================================================================
// p4_helpers.h — ESP32-P4 Coolroom Controller: C++/ESP-IDF helper functions
//
// PRINCIPLE: All non-trivial firmware logic lives here as proper C++ functions.
// YAML lambdas should be one-liners that call these helpers, not business logic.
//
// Requires ESP-IDF framework (no Arduino).
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cstring>
#include <cstdio>
#include <cmath>
#include <inttypes.h>
#include <sys/time.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>

// ─── GT911 touch — shared LCD/touch reset (GPIO33) ─────────────────────────

/// Hold GT911 INT low before the mipi_dsi display driver pulses the shared
/// RST line (GPIO33). That straps address 0x5D during the LCD reset sequence
/// without a second post-init reset — pulsing RST after esp_lcd_panel_init()
/// would hard-reset the panel and leave a black screen (no re-init in ESPHome).
/// Matches Espressif BSP intent; see gt911_touchscreen.cpp address-strap flow.
inline void p4_gt911_prepare_for_lcd_reset(gpio_num_t int_pin) {
    gpio_reset_pin(int_pin);
    gpio_set_direction(int_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(int_pin, 0);
    ESP_LOGI("p4", "GT911 INT held low for shared LCD/touch reset (addr 0x5D)");
}

// ─── Boot diagnostics ──────────────────────────────────────────────────────

/// Log comprehensive boot-time diagnostics using ESP-IDF APIs.
/// Called from esphome: on_boot priority 600 lambda.
inline void p4_log_boot() {
    ESP_LOGI("p4", "========================================");
    ESP_LOGI("p4", "ESP32-P4 Coolroom Controller starting");
    ESP_LOGI("p4", "CPU: RISC-V HP dual-core @ %d MHz",
             CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    ESP_LOGI("p4", "DRAM  total=%5u KB  free=%5u KB",
             (unsigned)(heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    ESP_LOGI("p4", "PSRAM total=%5u KB  free=%5u KB",
             (unsigned)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    ESP_LOGI("p4", "========================================");
}

// ─── NTP / Time ────────────────────────────────────────────────────────────

/// Sticky record of the last successful SNTP sync. ESP-IDF's
/// sntp_get_sync_status() clears COMPLETED back to RESET on the first read
/// (components/lwip/apps/sntp/sntp.c), so polling it from more than one place
/// means whichever caller reads first consumes the flag and every other caller
/// sees "never synced". Latch it here instead.
inline uint32_t &p4_ntp_last_sync_ms_ref() {
    static uint32_t last_sync_ms = 0;   // 0 = no sync yet this boot
    return last_sync_ms;
}

/// Record a completed sync. Called from the time component's on_time_sync.
inline void p4_ntp_mark_synced() {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    p4_ntp_last_sync_ms_ref() = now ? now : 1;
}

/// True once SNTP has completed at least one successful sync this boot.
inline bool p4_ntp_synced() {
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
        p4_ntp_mark_synced();
    return p4_ntp_last_sync_ms_ref() != 0;
}

/// Seconds since the last successful SNTP sync, or -1 if none this boot.
inline int32_t p4_ntp_sync_age_s() {
    const uint32_t last = p4_ntp_last_sync_ms_ref();
    if (last == 0) return -1;
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    return (int32_t)((now - last) / 1000U);
}

/// Format wall-clock time into caller-supplied buffer as "DD-MM-YYYY HH:MM:SS".
/// Falls back to "uptime+<N>s" when NTP has not yet synced.
inline void p4_fmt_time(char* buf, size_t len) {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 86400L) {   // epoch < 1 day ⇒ clock not yet set
        const uint64_t up_s = esp_timer_get_time() / 1000000ULL;
        snprintf(buf, len, "uptime+%llus", (unsigned long long)up_s);
    } else {
        struct tm t{};
        localtime_r(&tv.tv_sec, &t);
        strftime(buf, len, "%d-%m-%Y %H:%M:%S", &t);
    }
}

/// True once the wall clock looks set (SNTP or external RTC), not still at
/// the 1970 epoch. Used to decide whether boot can skip the aggressive NTP
/// poll — if the battery-backed RTC already seeded time, there is no need
/// to hammer the network every minute until first sync.
inline bool p4_wall_clock_ok() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    // 2024-01-01 00:00:00 UTC — anything earlier is "clock never set".
    return tv.tv_sec >= 1704067200L;
}

/// Set the SNTP re-sync interval at runtime using the ESP-IDF API.
/// interval_ms: milliseconds between NTP polls (e.g. 24*3600*1000 = daily).
inline void p4_ntp_set_interval(uint32_t interval_ms) {
    esp_sntp_set_sync_interval(interval_ms);
    ESP_LOGI("ntp", "SNTP sync interval set to %u ms", (unsigned)interval_ms);
}

/// Reduce SNTP poll interval to speed up first sync after boot.
/// Call from on_boot; restore to normal once p4_ntp_synced() is true.
inline void p4_ntp_set_fast_sync(uint32_t fast_interval_ms) {
    esp_sntp_set_sync_interval(fast_interval_ms);
    ESP_LOGI("ntp", "SNTP fast-sync mode: %u ms", (unsigned)fast_interval_ms);
}

// ─── System memory diagnostics ─────────────────────────────────────────────

/// Format heap usage summary.
inline void p4_fmt_heap_mb(char* buf, size_t len) {
    uint32_t total = esp_get_free_heap_size();
    uint32_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    snprintf(buf, len, "Free: %" PRIu32 " KB | Largest: %" PRIu32 " KB",
             total / 1024, largest_free / 1024);
}

/// Format PSRAM usage summary (if available).
inline void p4_fmt_psram_mb(char* buf, size_t len) {
    snprintf(buf, len, "PSRAM: functional");
}

// ─── RS485 / Probe health ──────────────────────────────────────────────────

/// Seconds elapsed since a Modbus sensor last delivered a sample.
/// last_ms: millis() timestamp from the sensor callback; 0 = never received.
/// Returns NaN if never received (renders as "unavailable" in HA).
inline float p4_sample_age_s(uint32_t last_ms) {
    if (last_ms == 0U) return NAN;
    return static_cast<float>(millis() - last_ms) / 1000.0f;
}

/// True if the sample is no older than max_age_ms milliseconds.
inline bool p4_sample_fresh(uint32_t last_ms, uint32_t max_age_ms) {
    if (last_ms == 0U) return false;
    return (millis() - last_ms) < max_age_ms;
}

/// Validate an RTD temperature reading: must be finite and in physical range.
/// Coolroom applications: -50 °C to +80 °C covers any realistic operating point.
inline bool p4_rtd_valid(float t) {
    return std::isfinite(t) && t > -50.0f && t < 80.0f;
}

// ─── I2C humidity / ambient (SHT31 / SHT20) online ─────────────────────────
//
// ESPHome's htu21d (and sht3xd) platforms set status_warning on I2C failure but
// do **not** publish NaN — the last raw temperature sticks. Online checks that
// only test `!isnan(raw)` therefore stay true after disconnect, and also when
// the operator enable switch is OFF (public templates gate to NaN, raw does not).
// Match the CT-clamp pattern: enabled AND component healthy AND a finite reading.

/// True when an enabled I2C humidity/ambient sensor is responding.
inline bool p4_i2c_hum_online(bool enabled, bool failed, bool warning, float temp_c) {
    if (!enabled || failed || warning) return false;
    return std::isfinite(temp_c);
}

/// Sticky raw readings should be wiped when the sensor is disabled or unhealthy.
inline bool p4_i2c_hum_should_clear(bool enabled, bool failed, bool warning) {
    return !enabled || failed || warning;
}

/// Return the validated RTD reading, or NaN if invalid.
inline float p4_rtd_or_nan(float t) {
    return p4_rtd_valid(t) ? t : NAN;
}

// ─── Temperature display ───────────────────────────────────────────────────

/// Format a Celsius temperature for display, with optional °F conversion.
/// use_fahrenheit: convert to °F before formatting.
/// na_text: returned when t is NaN (e.g. "---").
inline std::string p4_fmt_temp(float t_c, bool use_fahrenheit,
                                const char* na_text = "---") {
    if (!std::isfinite(t_c)) return std::string{na_text};
    char buf[16];
    if (use_fahrenheit) {
        snprintf(buf, sizeof(buf), "%.1f°F", t_c * 9.0f / 5.0f + 32.0f);
    } else {
        snprintf(buf, sizeof(buf), "%.1f°C", t_c);
    }
    return std::string{buf};
}

/// Format a temperature difference/offset. Fahrenheit deltas scale by 9/5
/// without the +32 absolute-temperature offset.
inline std::string p4_fmt_temp_delta(float delta_c, bool use_fahrenheit,
                                      const char* na_text = "---") {
    if (!std::isfinite(delta_c)) return std::string{na_text};
    char buf[16];
    if (use_fahrenheit) {
        snprintf(buf, sizeof(buf), "%.1f°F", delta_c * 9.0f / 5.0f);
    } else {
        snprintf(buf, sizeof(buf), "%.1f°C", delta_c);
    }
    return std::string{buf};
}

inline std::string p4_fmt_humidity(float rh, const char* na_text = "--%RH") {
    if (!std::isfinite(rh)) return std::string{na_text};
    char buf[12];
    snprintf(buf, sizeof(buf), "%.0f%%RH", rh);
    return std::string{buf};
}

/// Home right-rail: evaporator probe (classic + alt). Keeps the "Evap" prefix
/// even when the reading is offline so the slot stays identifiable.
inline std::string p4_fmt_evap_label(float t_c, bool use_fahrenheit) {
    return std::string("Evap\n") + p4_fmt_temp(t_c, use_fahrenheit);
}

/// Home right-rail: internal SHT31 combined temp + RH (classic + alt).
inline std::string p4_fmt_int_humidity_label(float t_c, float rh,
                                              bool use_fahrenheit) {
    return "Int " + p4_fmt_temp(t_c, use_fahrenheit) + "\n" +
           p4_fmt_humidity(rh);
}

/// Home right-rail: external SHT20 combined temp + RH (classic + alt).
inline std::string p4_fmt_ext_humidity_label(float t_c, float rh,
                                              bool use_fahrenheit) {
    return "Ext " + p4_fmt_temp(t_c, use_fahrenheit) + "\n" +
           p4_fmt_humidity(rh);
}

// ─── Memory diagnostics (call from template sensor lambdas) ───────────────

inline float p4_free_heap_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)) / 1024.0f;
}

inline float p4_free_psram_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / 1024.0f;
}

// ─── CPU utilisation (windowed, average + per-core) ───────────────────────
// Needs CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS. Samples idle-task counters
// between calls and returns busy% = 100 − idle fraction. First call after
// boot returns NAN until a second sample lands — callers treat that as
// "unknown". One shared sampler so avg / core0 / core1 stay in sync; a short
// debounce avoids three back-to-back sensor lambdas carving tiny windows.

#if (configGENERATE_RUN_TIME_STATS == 1) && (INCLUDE_xTaskGetIdleTaskHandle == 1) && !defined(CONFIG_FREERTOS_SMP)

inline float &p4_cpu_snap_avg_ref_() {
    static float v = NAN;
    return v;
}
inline float *p4_cpu_snap_core_ref_() {
    static float v[configNUMBER_OF_CORES];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < (int) configNUMBER_OF_CORES; i++) v[i] = NAN;
        init = true;
    }
    return v;
}

inline void p4_cpu_refresh_() {
    static uint32_t last_idle[configNUMBER_OF_CORES] = {};
    static uint32_t last_total = 0;
    static uint32_t last_refresh_ms = 0;

    const uint32_t now_ms = millis();
    // Same-tick callers (avg + C0 + C1 sensors, or fmt) share one snapshot.
    if (last_total != 0 && (now_ms - last_refresh_ms) < 400) return;

    uint32_t total_now = (uint32_t) portGET_RUN_TIME_COUNTER_VALUE();
    uint32_t idle_now[configNUMBER_OF_CORES];
    for (BaseType_t c = 0; c < (BaseType_t) configNUMBER_OF_CORES; c++) {
        idle_now[c] = (uint32_t) ulTaskGetIdleRunTimeCounterForCore(c);
    }

    float &snap_avg = p4_cpu_snap_avg_ref_();
    float *snap_core = p4_cpu_snap_core_ref_();

    if (last_total == 0 || total_now <= last_total) {
        for (int c = 0; c < (int) configNUMBER_OF_CORES; c++) last_idle[c] = idle_now[c];
        last_total = total_now;
        last_refresh_ms = now_ms;
        return;
    }

    const float dt = (float) (total_now - last_total);
    float idle_sum = 0.0f;
    for (int c = 0; c < (int) configNUMBER_OF_CORES; c++) {
        float idle_frac = (float) (idle_now[c] - last_idle[c]) / dt;
        if (idle_frac < 0.0f) idle_frac = 0.0f;
        if (idle_frac > 1.0f) idle_frac = 1.0f;
        snap_core[c] = (1.0f - idle_frac) * 100.0f;
        idle_sum += idle_frac;
        last_idle[c] = idle_now[c];
    }
    last_total = total_now;
    last_refresh_ms = now_ms;
    snap_avg = (1.0f - idle_sum / (float) configNUMBER_OF_CORES) * 100.0f;
}

inline float p4_cpu_usage_pct() {
    p4_cpu_refresh_();
    return p4_cpu_snap_avg_ref_();
}

inline float p4_cpu_usage_core_pct(int core) {
    p4_cpu_refresh_();
    if (core < 0 || core >= (int) configNUMBER_OF_CORES) return NAN;
    return p4_cpu_snap_core_ref_()[core];
}

#else

inline float p4_cpu_usage_pct() {
    return NAN;  // run-time stats not compiled in
}

inline float p4_cpu_usage_core_pct(int /*core*/) {
    return NAN;
}

#endif

/// Format "CPU: 45%  C0:40% C1:50%" / "CPU: --%" into buf (LVGL Info page).
inline void p4_fmt_cpu_usage(char *buf, size_t n) {
    float pct = p4_cpu_usage_pct();
    float c0 = p4_cpu_usage_core_pct(0);
    float c1 = p4_cpu_usage_core_pct(1);
    if (!std::isfinite(pct)) {
        snprintf(buf, n, "CPU: --%%");
        return;
    }
    if (std::isfinite(c0) && std::isfinite(c1)) {
        snprintf(buf, n, "CPU: %.0f%%  C0:%.0f%% C1:%.0f%%", pct, c0, c1);
        return;
    }
    snprintf(buf, n, "CPU: %.0f%%", pct);
}

// ─── Settings-screen PIN lock ──────────────────────────────────────────────
// Same design as the earlier S3 project: the 4-digit PIN is never stored in
// plaintext — only a djb2 hash (ctl_pin_hash global) persists across
// restarts. ctl_pin_buf holds the digits currently being typed on either the
// LVGL keypad (page_pin_entry / page_set_pin) and is cleared immediately
// after each hash check or save.

/// Append a digit to the PIN entry buffer, capped at max_len (default 4).
inline bool p4_pin_append_digit(std::string& buf, char digit, size_t max_len = 4) {
    if (buf.size() >= max_len) return false;
    buf.push_back(digit);
    return true;
}

/// Remove the last digit from the PIN entry buffer.
inline bool p4_pin_backspace(std::string& buf) {
    if (buf.empty()) return false;
    buf.pop_back();
    return true;
}

/// Mask the PIN entry buffer for on-screen display (each digit -> '*').
inline std::string p4_pin_masked(const std::string& buf) {
    return std::string(buf.size(), '*');
}

/// djb2 string hash — used to compare/store the PIN without keeping it in
/// plaintext. Not cryptographic; sufficient for a 4-digit local keypad lock
/// where the threat model is "don't leave the PIN sitting in flash/logs as
/// plaintext", not "resist a targeted brute-force attack."
inline uint32_t p4_pin_hash_djb2(const std::string& pin) {
    uint32_t h = 5381;
    for (char c : pin) h = ((h << 5) + h) + static_cast<uint8_t>(c);
    return h;
}

// ─── Home Assistant native API gate ─────────────────────────────────────────
// When "Home Assistant API Enabled" is off, drop any connected native-API
// clients so HA cannot subscribe to sensors/switches. The listen socket stays
// up (ESPHome has no public stop API); new clients are rejected the same way
// via on_client_connected + the 1 s poll.

#ifdef USE_API
#include "esphome/components/api/api_server.h"

inline void p4_ha_api_drop_clients() {
    if (esphome::api::global_api_server == nullptr) return;
    if (!esphome::api::global_api_server->is_connected()) return;
    for (auto &c : esphome::api::global_api_server->active_clients()) {
        if (c) c->on_fatal_error();
    }
    ESP_LOGW("ha", "Home Assistant API disabled — dropped client(s)");
}
#else
inline void p4_ha_api_drop_clients() {}
#endif

// ─── Speaker volume (UI % ↔ ES8311 codec float) ─────────────────────────────
// ESPHome's ES8311 driver remaps 0.0–1.0 onto DAC register 0x00–0xFF, where
// 0.75 ≈ 0xBF = 0 dB (unity). Raw ui%/100 at 85–100 → 0.85–1.0 is loud but
// crackles on this NS4150B + speaker. Cap modestly above unity (not 0.90–1.0).
//
//   codec = 0.35 + (clamp(ui_pct, 20, 100) − 20) / 80 × 0.47
//   UI 20% → 0.35   UI 70% → 0.644   UI 85% → 0.732   UI 100% → 0.82
//
// Floor 20% so alarm speech cannot be silenced by accident. Soft-start /
// click suppression is unchanged — only the gain applied at unmute differs.

constexpr float P4_AUDIO_CODEC_GAIN_FLOOR = 0.35f;
constexpr float P4_AUDIO_CODEC_GAIN_CEILING = 0.82f;  // mild boost above unity
constexpr float P4_AUDIO_UI_MIN_PCT = 20.0f;
constexpr float P4_AUDIO_UI_MAX_PCT = 100.0f;

inline float p4_audio_clamp_ui_pct(float pct) {
    if (pct < P4_AUDIO_UI_MIN_PCT) return P4_AUDIO_UI_MIN_PCT;
    if (pct > P4_AUDIO_UI_MAX_PCT) return P4_AUDIO_UI_MAX_PCT;
    return pct;
}

/// Operator-facing percent → ES8311 / media_player volume float (≤ ceiling).
inline float p4_audio_ui_to_codec(float ui_pct) {
    const float ui = p4_audio_clamp_ui_pct(ui_pct);
    const float span = P4_AUDIO_UI_MAX_PCT - P4_AUDIO_UI_MIN_PCT;
    const float gain_span =
        P4_AUDIO_CODEC_GAIN_CEILING - P4_AUDIO_CODEC_GAIN_FLOOR;
    float codec = P4_AUDIO_CODEC_GAIN_FLOOR +
                  (ui - P4_AUDIO_UI_MIN_PCT) / span * gain_span;
    if (codec > P4_AUDIO_CODEC_GAIN_CEILING) return P4_AUDIO_CODEC_GAIN_CEILING;
    if (codec < P4_AUDIO_CODEC_GAIN_FLOOR) return P4_AUDIO_CODEC_GAIN_FLOOR;
    return codec;
}

/// Media-player / codec float → operator-facing percent (for HA on_volume sync).
inline float p4_audio_codec_to_ui(float codec) {
    if (codec <= P4_AUDIO_CODEC_GAIN_FLOOR) return P4_AUDIO_UI_MIN_PCT;
    if (codec >= P4_AUDIO_CODEC_GAIN_CEILING) return P4_AUDIO_UI_MAX_PCT;
    const float span = P4_AUDIO_UI_MAX_PCT - P4_AUDIO_UI_MIN_PCT;
    const float gain_span =
        P4_AUDIO_CODEC_GAIN_CEILING - P4_AUDIO_CODEC_GAIN_FLOOR;
    return p4_audio_clamp_ui_pct(
        P4_AUDIO_UI_MIN_PCT +
        (codec - P4_AUDIO_CODEC_GAIN_FLOOR) / gain_span * span);
}

// ─── Wall clock (SoC LP RTC + SNTP) ─────────────────────────────────────────
// Waveshare FAQ for ESP32-P4-WIFI6-Touch-LCD-7B: use the chip's internal 48-bit
// low-power RTC via settimeofday() / POSIX time, plus NTP over C6 Wi-Fi.
// Optional 1220 cell in holder item 11 backs VBAT so time can survive power
// loss. There is no on-board I2C RTC chip in the published docs — do not probe
// 0x51/0x68 for timekeeping.
//
// ESP32-P4 / ESP-IDF quirk: flashing or resetting via the USB-to-UART bridge
// (DTR/RTS → EN) reports ESP_RST_POWERON, not a software reboot. A correct
// clock right after USB reset usually means NTP already ran (or VBAT held
// time across the power-on-like reset).
// ────────────────────────────────────────────────────────────────────────────
