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

// ─── Memory diagnostics (call from template sensor lambdas) ───────────────

inline float p4_free_heap_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)) / 1024.0f;
}

inline float p4_free_psram_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / 1024.0f;
}

// ─── CPU utilisation (windowed, average across cores) ─────────────────────
// Needs CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS. Samples idle-task counters
// between calls and returns busy% = 100 − mean idle fraction. First call
// after boot (or after enabling the option) returns NAN until a second
// sample lands — callers should treat that as "unknown".

#if (configGENERATE_RUN_TIME_STATS == 1) && (INCLUDE_xTaskGetIdleTaskHandle == 1) && !defined(CONFIG_FREERTOS_SMP)

inline float p4_cpu_usage_pct() {
    static uint32_t last_idle[configNUMBER_OF_CORES] = {};
    static uint32_t last_total = 0;
    static float last_pct = NAN;

    uint32_t total_now = (uint32_t) portGET_RUN_TIME_COUNTER_VALUE();
    uint32_t idle_now[configNUMBER_OF_CORES];
    for (BaseType_t c = 0; c < (BaseType_t) configNUMBER_OF_CORES; c++) {
        idle_now[c] = (uint32_t) ulTaskGetIdleRunTimeCounterForCore(c);
    }

    if (last_total == 0 || total_now <= last_total) {
        for (int c = 0; c < configNUMBER_OF_CORES; c++) last_idle[c] = idle_now[c];
        last_total = total_now;
        return last_pct;  // NAN until we have a real window
    }

    const float dt = (float) (total_now - last_total);
    float idle_sum = 0.0f;
    for (int c = 0; c < configNUMBER_OF_CORES; c++) {
        idle_sum += (float) (idle_now[c] - last_idle[c]);
        last_idle[c] = idle_now[c];
    }
    last_total = total_now;

    // Each core contributes one wall-time worth of idle; average across cores.
    float idle_frac = idle_sum / (dt * (float) configNUMBER_OF_CORES);
    if (idle_frac < 0.0f) idle_frac = 0.0f;
    if (idle_frac > 1.0f) idle_frac = 1.0f;
    last_pct = (1.0f - idle_frac) * 100.0f;
    return last_pct;
}

#else

inline float p4_cpu_usage_pct() {
    return NAN;  // run-time stats not compiled in
}

#endif

/// Format "CPU: 23%" / "CPU: --%" into buf (for LVGL info page).
inline void p4_fmt_cpu_usage(char *buf, size_t n) {
    float pct = p4_cpu_usage_pct();
    if (!std::isfinite(pct)) {
        snprintf(buf, n, "CPU: --%%");
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
