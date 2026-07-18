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
#include <sys/time.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

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

/// True once SNTP has completed at least one successful sync.
inline bool p4_ntp_synced() {
    const sntp_sync_status_t s = esp_sntp_get_sync_status();
    return s == SNTP_SYNC_STATUS_COMPLETED;
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

// ─── Memory diagnostics (call from template sensor lambdas) ───────────────

inline float p4_free_heap_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)) / 1024.0f;
}

inline float p4_free_psram_kb() {
    return static_cast<float>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / 1024.0f;
}
