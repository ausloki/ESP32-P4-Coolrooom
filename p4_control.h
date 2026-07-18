#pragma once
// ============================================================================
// p4_control.h — ESP32-P4 Coolroom Controller: Phase 3 Control Logic
//
// PRINCIPLE: All control decisions live here as pure C++ functions.
// YAML interval lambdas call these; they never contain business logic.
//
// Control Architecture:
//   - Compressor : hysteresis band control around setpoint
//   - Alarms     : high/low delta thresholds relative to setpoint
//   - Defrost    : time-based scheduling, mutual exclusion with compressor
//   - Safety     : stale probe detection forces compressor OFF immediately
//
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cmath>
#include <esp_log.h>
#include "p4_helpers.h"

// ─── Compressor hysteresis band control ────────────────────────────────────
//
// Symmetric band of ±(diff/2) around setpoint:
//   ON  threshold = setpoint + diff/2   (if compressor currently OFF)
//   OFF threshold = setpoint - diff/2   (if compressor currently ON)
//
// This prevents rapid cycling while tracking the setpoint accurately.

/// Evaluate whether the compressor relay state should change.
///
/// Returns:
///   +1 = command ON   (temp above upper threshold, compressor was OFF)
///   -1 = command OFF  (temp below lower threshold, OR probe fault)
///    0 = hold current state
///
/// Safety: returns -1 immediately if probe is stale or reading is invalid.
inline int p4_ctl_compressor_eval(
    float    coolroom_c,        ///< current probe 1 reading (°C)
    float    setpoint_c,        ///< target temperature (°C)
    float    diff_c,            ///< hysteresis band full-width (°C)
    bool     compressor_on,     ///< current relay_compressor state
    uint32_t probe_last_ms,     ///< millis() timestamp of last valid probe 1 sample
    uint32_t probe_max_age_ms   ///< max acceptable age before declaring fault
) {
    // Safety lockout — stale or out-of-range reading
    if (!p4_rtd_valid(coolroom_c) || !p4_sample_fresh(probe_last_ms, probe_max_age_ms)) {
        if (compressor_on) {
            ESP_LOGW("ctl", "Probe stale/invalid — forcing compressor OFF (safety)");
        }
        return -1;
    }

    const float half = diff_c / 2.0f;
    if (!compressor_on && coolroom_c > (setpoint_c + half)) {
        ESP_LOGI("ctl", "Coolroom %.1f°C > ON-threshold %.1f°C — compressor ON",
                 coolroom_c, setpoint_c + half);
        return +1;
    }
    if (compressor_on && coolroom_c < (setpoint_c - half)) {
        ESP_LOGI("ctl", "Coolroom %.1f°C < OFF-threshold %.1f°C — compressor OFF",
                 coolroom_c, setpoint_c - half);
        return -1;
    }
    return 0;
}

// ─── Alarm thresholds ──────────────────────────────────────────────────────

/// True when the high-temperature alarm condition is met.
/// Alarm activates when coolroom exceeds (setpoint + delta).
/// Returns false for invalid readings (alarm requires a valid probe).
inline bool p4_ctl_alarm_high(float coolroom_c, float setpoint_c, float delta_c) {
    return p4_rtd_valid(coolroom_c) && (coolroom_c > (setpoint_c + delta_c));
}

/// True when the low-temperature alarm condition is met.
/// Alarm activates when coolroom falls below (setpoint - delta).
/// Returns false for invalid readings.
inline bool p4_ctl_alarm_low(float coolroom_c, float setpoint_c, float delta_c) {
    return p4_rtd_valid(coolroom_c) && (coolroom_c < (setpoint_c - delta_c));
}

// ─── Defrost scheduling ─────────────────────────────────────────────────────
//
// Time-based defrost: run for up to max_duration_ms every interval_ms.
// First defrost is triggered after min_uptime_ms to allow system to stabilise.

/// True when a new defrost cycle should start.
///
/// defrost_last_end_ms: millis() recorded when the last defrost relay turned OFF.
///                      0 = defrost has never run (triggers immediately after min_uptime).
/// interval_ms:         time between defrost cycles (e.g. 8h = 28 800 000 ms).
/// min_uptime_ms:       minimum system uptime before the first defrost (default 10 min).
inline bool p4_ctl_defrost_due(
    uint32_t defrost_last_end_ms,
    uint32_t interval_ms,
    uint32_t min_uptime_ms = 600000U   // 10 minutes
) {
    if (millis() < min_uptime_ms) return false;     // not ready yet
    if (defrost_last_end_ms == 0U)  return true;    // never run → start now
    return (millis() - defrost_last_end_ms) >= interval_ms;
}

/// True when a running defrost cycle has exceeded its maximum duration.
///
/// defrost_start_ms: millis() recorded when relay_defrost turned ON.
/// max_duration_ms:  safety maximum (e.g. 30 min = 1 800 000 ms).
inline bool p4_ctl_defrost_timeout(uint32_t defrost_start_ms, uint32_t max_duration_ms) {
    return (millis() - defrost_start_ms) >= max_duration_ms;
}

// ─── Probe fault check ─────────────────────────────────────────────────────

/// True when probe 1 is considered faulted (stale reading or out-of-range).
/// Used to drive ctl_probe_fault global and gate all compressor commands.
inline bool p4_ctl_probe_fault(float coolroom_c, uint32_t probe_last_ms, uint32_t max_age_ms) {
    return !p4_rtd_valid(coolroom_c) || !p4_sample_fresh(probe_last_ms, max_age_ms);
}
