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
inline bool p4_ctl_defrost_timeout(uint32_t defrost_start_ms, uint32_t max_duration_ms) {
    return (millis() - defrost_start_ms) >= max_duration_ms;
}

/// True when the compressor is within the off-delay lockout window.
/// comp_last_off_ms: millis() recorded when relay_compressor turned OFF.
/// lockout_ms: minimum off-time (e.g. 3 min = 180 000 ms).
inline bool p4_ctl_comp_locked_out(uint32_t comp_last_off_ms, uint32_t lockout_ms) {
    if (comp_last_off_ms == 0U) return false;
    return (millis() - comp_last_off_ms) < lockout_ms;
}

/// True while the compressor must stay ON even though the room has already
/// reached the cut-out threshold (min-run / short-cycle protection on the
/// ON side — complement to p4_ctl_comp_locked_out).
/// min_run_ms = 0 disables the hold.
inline bool p4_ctl_comp_min_run_hold(
    bool     coolroom_at_or_below_cutout,
    bool     compressor_on,
    uint32_t comp_on_since_ms,
    uint32_t min_run_ms
) {
    if (!compressor_on || min_run_ms == 0U) return false;
    if (!coolroom_at_or_below_cutout) return false;
    if (comp_on_since_ms == 0U) return true;  // unknown start → hold conservatively
    return (millis() - comp_on_since_ms) < min_run_ms;
}

/// Scheduled defrost is due but the evaporator is already at/below the skip
/// threshold (coil already cold / no frost worth clearing). Caller should
/// roll the interval clock forward instead of starting a cycle.
inline bool p4_ctl_defrost_skip_if_cold(
    bool  skip_enabled,
    bool  evap_ok,
    float evap_c,
    float skip_below_c
) {
    if (!skip_enabled || !evap_ok || !p4_rtd_valid(evap_c)) return false;
    return evap_c <= skip_below_c;
}

/// Safety-net: force a defrost when too long has elapsed since the last cycle
/// ended (or started, if end was never recorded). Bypasses skip-if-cold.
inline bool p4_ctl_defrost_force_max_due(
    uint32_t last_end_ms,
    uint32_t last_start_ms,
    uint32_t force_max_ms
) {
    if (force_max_ms == 0U) return false;
    uint32_t ref = last_end_ms != 0U ? last_end_ms : last_start_ms;
    if (ref == 0U) return false;
    return (millis() - ref) >= force_max_ms;
}

/// Frost-rate (humidity-drop) early defrost: large RH drop over the sample
/// window while room temperature stays nearly stable. Returns true once per
/// window when the condition holds; caller latches "already triggered" for
/// the current cooling cycle.
inline bool p4_ctl_frost_rate_ready(
    bool     enabled,
    bool     in_defrost,
    bool     already_triggered,
    float    humidity_pct,
    float    coolroom_c,
    float    last_humidity_pct,
    float    last_temp_c,
    uint32_t last_sample_ms,
    uint32_t window_ms,
    float    humidity_drop_threshold_pct,
    float    max_temp_change_c = 0.5f
) {
    if (!enabled || in_defrost || already_triggered) return false;
    if (isnan(humidity_pct) || isnan(coolroom_c)) return false;
    if (last_sample_ms == 0U) return false;  // caller must seed baseline first
    if ((millis() - last_sample_ms) < window_ms) return false;
    float humidity_drop = last_humidity_pct - humidity_pct;
    float temp_change = fabsf(coolroom_c - last_temp_c);
    return humidity_drop >= humidity_drop_threshold_pct && temp_change <= max_temp_change_c;
}

// ─── Alarm grace periods ────────────────────────────────────────────────────

/// True during the startup quiet period — no alarms should fire.
/// boot_ms: millis() at boot; grace_ms: quiet window (e.g. 15 min).
inline bool p4_ctl_startup_grace(uint32_t boot_ms, uint32_t grace_ms) {
    return (millis() - boot_ms) < grace_ms;
}

/// True during the post-defrost quiet period — alarms suppressed while room recovers.
/// defrost_end_ms: millis() when defrost ended; 0 = never ran.
inline bool p4_ctl_defrost_grace(uint32_t defrost_end_ms, uint32_t grace_ms) {
    if (defrost_end_ms == 0U) return false;
    return (millis() - defrost_end_ms) < grace_ms;
}

/// True once the room has reached within the alarm-safe band around setpoint
/// (used to detect first-ever pulldown completion since boot).
inline bool p4_ctl_pulldown_reached(float coolroom_c, float setpoint_c,
                                     float alarm_high_c, float alarm_low_c) {
    if (!p4_rtd_valid(coolroom_c)) return false;
    return !p4_ctl_alarm_high(coolroom_c, setpoint_c, alarm_high_c) &&
           !p4_ctl_alarm_low(coolroom_c, setpoint_c, alarm_low_c);
}

/// True while the startup alarm-suppression grace is active. Held
/// unconditionally for floor_ms (covers a normal, quick pulldown without
/// nuisance alarms during the approach to setpoint), then continues —
/// capped at ceiling_ms — until the room has reached the alarm-safe band for
/// the first time. Prevents a slow warm-start pulldown from false-alarming
/// before it's had a fair chance to reach setpoint, while still guaranteeing
/// a genuinely stuck/broken pulldown eventually alarms rather than being
/// suppressed forever.
inline bool p4_ctl_startup_grace_active(
    uint32_t boot_ms,
    uint32_t floor_ms,
    uint32_t ceiling_ms,
    bool     pulldown_done
) {
    if (p4_ctl_startup_grace(boot_ms, floor_ms)) return true;
    if (pulldown_done) return false;
    return p4_ctl_startup_grace(boot_ms, ceiling_ms);
}

/// True once the alarm condition has persisted long enough to be considered real.
/// alarm_since_ms: millis() when the alarm was first detected; 0 = not active.
/// persist_ms: minimum duration required before alarm fires the siren.
inline bool p4_ctl_alarm_persisted(uint32_t alarm_since_ms, uint32_t persist_ms) {
    if (alarm_since_ms == 0U) return false;
    return (millis() - alarm_since_ms) >= persist_ms;
}

/// True once temperature has recovered past the hysteresis band.
/// Use to prevent alarm chatter around the threshold.
inline bool p4_ctl_alarm_hysteresis_clear(float coolroom_c, float setpoint_c,
                                           float alarm_delta_c, float hysteresis_c,
                                           bool high_alarm) {
    if (!p4_rtd_valid(coolroom_c)) return false;
    if (high_alarm) return coolroom_c < (setpoint_c + alarm_delta_c - hysteresis_c);
    return coolroom_c > (setpoint_c - alarm_delta_c + hysteresis_c);
}

// ─── Door alarm ─────────────────────────────────────────────────────────────

/// True when the door has been open longer than the alarm delay.
/// door_open_since_ms: millis() when door opened; 0 = closed.
/// delay_ms: time before alarm triggers (e.g. 30 s = 30 000 ms).
inline bool p4_ctl_door_alarm(uint32_t door_open_since_ms, uint32_t delay_ms) {
    if (door_open_since_ms == 0U) return false;
    return (millis() - door_open_since_ms) >= delay_ms;
}

// ─── Smart defrost (temperature-delta triggered) ────────────────────────────

/// True when the coolroom-to-evaporator delta has exceeded the threshold for
/// at least dwell_ms, AND the compressor has been running for at least min_comp_ms.
/// Returns false if either probe is invalid.
inline bool p4_ctl_smart_defrost_ready(
    float    coolroom_c,          ///< probe 1
    float    evap_c,              ///< probe 2
    float    delta_threshold_c,   ///< trigger when (coolroom - evap) >= this
    uint32_t delta_since_ms,      ///< millis() when delta first exceeded threshold
    uint32_t dwell_ms,            ///< how long delta must hold before triggering
    uint32_t comp_on_since_ms,    ///< millis() when compressor turned ON
    uint32_t min_comp_ms,         ///< minimum compressor run time before triggering
    bool     lockout_active       ///< smart defrost locked out after recent defrost
) {
    if (!p4_rtd_valid(coolroom_c) || !p4_rtd_valid(evap_c)) return false;
    if (lockout_active) return false;
    if (comp_on_since_ms == 0U || (millis() - comp_on_since_ms) < min_comp_ms) return false;
    float delta = coolroom_c - evap_c;
    if (delta < delta_threshold_c || delta_since_ms == 0U) return false;
    return (millis() - delta_since_ms) >= dwell_ms;
}

/// True when a running defrost should end because evap temperature reached target.
inline bool p4_ctl_defrost_term_by_temp(float evap_c, float term_c, bool evap_ok) {
    if (!evap_ok || !p4_rtd_valid(evap_c)) return false;
    return evap_c >= term_c;
}

// ─── No-cool alarm ──────────────────────────────────────────────────────────

/// True when the compressor has been running for no_cool_ms but the room
/// has not cooled below the compressor-on threshold (suggests refrigeration failure).
inline bool p4_ctl_no_cool_alarm(
    float    coolroom_c,
    float    setpoint_c,
    float    diff_c,
    bool     comp_on,
    uint32_t comp_on_since_ms,
    uint32_t no_cool_ms
) {
    if (!p4_rtd_valid(coolroom_c) || !comp_on || comp_on_since_ms == 0U) return false;
    if ((millis() - comp_on_since_ms) < no_cool_ms) return false;
    return coolroom_c > (setpoint_c + diff_c / 2.0f + 0.5f);
}

// ─── Ice detection alarm ────────────────────────────────────────────────────

/// Instantaneous ice condition (Precision polarity): a *large* coolroom−evap
/// gap while the compressor runs means the coil is iced / starved of airflow
/// and the evaporator is much colder than the room. Older P4 builds used the
/// inverted "small gap" test — that is deliberately replaced to match Precision.
inline bool p4_ctl_ice_condition(float coolroom_c, float evap_c, float ice_delta_c) {
    if (!p4_rtd_valid(coolroom_c) || !p4_rtd_valid(evap_c)) return false;
    return (coolroom_c - evap_c) >= ice_delta_c;
}

/// True once the ice condition has held continuously for dwell_ms.
/// ice_since_ms: millis() when the condition first appeared; 0 = not active.
/// dwell_ms = 0 → fire on the same tick the condition appears.
inline bool p4_ctl_ice_alarm_dwelt(uint32_t ice_since_ms, uint32_t dwell_ms) {
    if (ice_since_ms == 0U) return false;
    if (dwell_ms == 0U) return true;
    return (millis() - ice_since_ms) >= dwell_ms;
}

/// Bitmask of active alarm / fault conditions for soft-mute tracking.
/// Bit0 high, 1 low, 2 door, 3 no-cool, 4 ice, 5 probe fault.
inline uint8_t p4_ctl_alarm_mask(bool hi, bool lo, bool door, bool no_cool, bool ice, bool probe) {
    uint8_t m = 0;
    if (hi) m |= 1u << 0;
    if (lo) m |= 1u << 1;
    if (door) m |= 1u << 2;
    if (no_cool) m |= 1u << 3;
    if (ice) m |= 1u << 4;
    if (probe) m |= 1u << 5;
    return m;
}

/// Soft-mute should end when nothing is left, or when a *new* condition bit
/// appears that was not active at the moment the operator silenced the bell.
inline bool p4_ctl_alarm_silence_should_clear(bool silenced, uint8_t silenced_mask, uint8_t now_mask) {
    if (!silenced) return false;
    if (now_mask == 0) return true;
    return (now_mask & ~silenced_mask) != 0;
}

/// Legacy name → condition only (no dwell). Prefer p4_ctl_ice_condition + dwell.
inline bool p4_ctl_ice_alarm(float coolroom_c, float evap_c, float ice_delta_c) {
    return p4_ctl_ice_condition(coolroom_c, evap_c, ice_delta_c);
}

// ─── Probe source profiles ──────────────────────────────────────────────────

/// Probe source enum (matches Precision / LCD labels).
enum P4ProbeSource : int {
    P4_PROBE_RTD_RS485 = 0,
    P4_PROBE_SHT31_I2C = 1,
    P4_PROBE_UNUSED    = 2,
};

/// Resolve the coolroom control temperature from the selected source.
/// Returns NAN when the source is unused or the reading is invalid.
inline float p4_ctl_resolve_coolroom_c(int probe1_type, float rtd_c, float sht31_c) {
    if (probe1_type == P4_PROBE_SHT31_I2C) return p4_rtd_valid(sht31_c) ? sht31_c : NAN;
    if (probe1_type == P4_PROBE_UNUSED) return NAN;
    return p4_rtd_valid(rtd_c) ? rtd_c : NAN;
}

/// Resolve evaporator temperature. Unused / non-RTD → NAN (evap_ok false).
inline float p4_ctl_resolve_evap_c(int probe2_type, bool probe2_enabled, float rtd_c) {
    if (!probe2_enabled || probe2_type == P4_PROBE_UNUSED) return NAN;
    // Evap coil is always an RTD on this board; SHT31 as probe2 is rejected.
    return p4_rtd_valid(rtd_c) ? rtd_c : NAN;
}

/// Auto-calibrate offset against an SHT31 reference: offset = ref − probe_avg.
/// Clamped to ±10 °C. Returns NAN if either input is invalid.
inline float p4_ctl_calibration_offset(float sht31_ref_c, float probe_avg_c) {
    if (!p4_rtd_valid(sht31_ref_c) || !p4_rtd_valid(probe_avg_c)) return NAN;
    float off = sht31_ref_c - probe_avg_c;
    if (off > 10.0f) off = 10.0f;
    if (off < -10.0f) off = -10.0f;
    return off;
}

// ─── Sensor fallback duty cycle ─────────────────────────────────────────────

/// When the primary probe is faulted and fallback is enabled, run the compressor
/// on a timed duty cycle (on_ms ON, off_ms OFF).
/// Mutates phase_on and last_toggle_ms in-place; returns true if compressor should run.
inline bool p4_ctl_fallback_should_run(
    bool      fault_active,
    bool      fallback_enabled,
    bool&     phase_on,
    uint32_t& last_toggle_ms,
    uint32_t  on_ms,
    uint32_t  off_ms
) {
    if (!fault_active || !fallback_enabled) return false;
    if (last_toggle_ms == 0U) {
        // Start of fallback: begin with ON phase
        phase_on       = true;
        last_toggle_ms = millis();
        return true;
    }
    uint32_t period_ms = phase_on ? on_ms : off_ms;
    if ((millis() - last_toggle_ms) >= period_ms) {
        phase_on       = !phase_on;
        last_toggle_ms = millis();
        ESP_LOGI("ctl", "Sensor fallback: phase now %s", phase_on ? "ON" : "OFF");
    }
    return phase_on;
}

// ─── Dew point (Magnus formula) ─────────────────────────────────────────────

/// Dew point via the Magnus formula (valid roughly -40..50°C, 1..100% RH).
/// Returns NAN for invalid inputs.
inline float p4_calc_dew_point_c(float temp_c, float humidity_pct) {
    if (isnan(temp_c) || isnan(humidity_pct) || humidity_pct <= 0.0f || humidity_pct > 100.0f)
        return NAN;
    const float a = 17.27f, b = 237.7f;
    float alpha = ((a * temp_c) / (b + temp_c)) + logf(humidity_pct / 100.0f);
    return (b * alpha) / (a - alpha);
}

/// True when the evaporator is cold enough for frost to actually be forming
/// right now — below both freezing and the surrounding air's dew point.
/// An early-defrost trigger layered on top of the fixed interval, not a
/// replacement for it: catches real frost formation between scheduled
/// cycles instead of only defrosting on a timer.
inline bool p4_ctl_dew_point_defrost_ready(
    float evap_c,
    float dew_point_c,
    bool  already_triggered_this_cycle
) {
    if (!p4_rtd_valid(evap_c) || isnan(dew_point_c)) return false;
    if (already_triggered_this_cycle) return false;
    return evap_c < dew_point_c && evap_c < 0.0f;
}

// ─── Probe fault check ─────────────────────────────────────────────────────

/// True when probe 1 is considered faulted (stale reading or out-of-range).
/// Used to drive ctl_probe_fault global and gate all compressor commands.
inline bool p4_ctl_probe_fault(float coolroom_c, uint32_t probe_last_ms, uint32_t max_age_ms) {
    return !p4_rtd_valid(coolroom_c) || !p4_sample_fresh(probe_last_ms, max_age_ms);
}
