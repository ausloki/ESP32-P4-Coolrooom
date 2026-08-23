#pragma once
// ============================================================================
// p4_ctl_tick.h — 10 s control orchestrator (elevated from YAML).
//
// Must only be #included via ESPHome `esphome: includes:` into main.cpp, AFTER
// the generated entity pointer declarations. Uses those file-scope pointers
// directly (same names ESPHome's id() rewrite produces).
//
// Core affinity (Option A): p4_ctl_tick_worker runs this on CPU 0. Do NOT call
// LVGL from here. Shared globals: control tick is the primary writer of
// compressor / defrost / alarm / probe-fault state; the UI loop (CPU 1) only
// reads those for display. Relay coil writes and speak/ntfy script triggers
// run on the worker core; Modbus TX still serialises inside the component.
// A binary semaphore wakes the worker from the 10 s YAML interval on CPU 1.
// ============================================================================

#include <cstdio>
#include <cmath>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "p4_control.h"
#include "p4_logging.h"

// Assumptions for C0 worker vs C1 UI (document for future maintainers):
//  - Single control writer: only p4_ctl_tick() mutates ctl_* alarm/defrost/
//    compressor timing globals and drives relay_compressor / relay_fan /
//    relay_siren from the control path.
//  - UI may read those globals without a mutex (bool/float aligned loads are
//    atomic enough for display). UI must not write the same control globals
//    except well-known operator actions (manual defrost req, alarm silence)
//    which are edge flags the tick consumes.
//  - SD appends run here on C0; mount/remount stays on the 60 s C1 interval.
//  - Never call LVGL APIs from this translation path / worker.

inline void p4_ctl_tick(uint32_t probe_stale_ms,
                        uint32_t startup_grace_max_min,
                        uint32_t log_interval_min) {

const uint32_t now_ms = millis();
 
float rtd1 = probe1_temp->state;
float rtd2 = probe2_temp->state;
float t = p4_ctl_resolve_coolroom_c(rtd1);
float evap = p4_ctl_resolve_evap_c(input_probe2_enabled->value(), rtd2);
bool evap_ok = p4_rtd_valid(evap) &&
               p4_sample_fresh(rtd1_ch2_last_ms->value(), probe_stale_ms * 3);
 
 
 
bool comp_was_on = relay_compressor->state;
bool sd_was_ready = p4_sd_is_ready();

 
bool fault = p4_ctl_probe_fault(t, rtd1_ch1_last_ms->value(), probe_stale_ms);
ctl_probe_fault->value() = fault;

 
const uint32_t startup_grace_ms     = (uint32_t)(ctl_startup_grace_min->value()  * 60000.0f);
const uint32_t startup_grace_max_ms = (uint32_t)(startup_grace_max_min * 60000.0f);
const uint32_t defrost_grace_ms     = (uint32_t)(ctl_defrost_grace_min->value()  * 60000.0f);
const uint32_t alarm_persist_ms     = (uint32_t)(ctl_alarm_persist_min->value()  * 60000.0f);

 
 
 
 
if (!ctl_startup_pulldown_done->value() &&
    p4_ctl_pulldown_reached(t, ctl_setpoint->value(),
                             ctl_alarm_high_delta->value(), ctl_alarm_low_delta->value())) {
    ctl_startup_pulldown_done->value() = true;
    ESP_LOGI("ctl", "Startup pulldown reached — normal alarm grace resumes");
}
bool in_grace = p4_ctl_startup_grace_active(ctl_boot_ms->value(), startup_grace_ms,
                                             startup_grace_max_ms,
                                             ctl_startup_pulldown_done->value()) ||
                p4_ctl_defrost_grace(ctl_defrost_last_end_ms->value(), defrost_grace_ms);

 
 
 
 
 
 
const uint32_t lockout_ms = (uint32_t)(ctl_comp_lockout_min->value() * 60000.0f);
bool locked_out = p4_ctl_comp_locked_out(ctl_comp_last_off_ms->value(), lockout_ms);
 
 
 
 
 
const bool relay_board_ok = hw_rs485_relay_ok->value();
 
 
 
const bool door_open_now = input_door_sensor_enabled->value() &&
    (ctl_door_open_since_ms->value() > 0);
const bool door_hold_comp = input_door_hold_compressor->value() && door_open_now;
const bool comp_start_blocked = locked_out || !relay_board_ok || door_hold_comp;

 
bool fallback_run = p4_ctl_fallback_should_run(
    fault, input_fallback_enabled->value(),
    ctl_fallback_phase_on->value(), ctl_fallback_last_toggle_ms->value(),
    (uint32_t)(ctl_fallback_on_min->value()  * 60000.0f),
    (uint32_t)(ctl_fallback_off_min->value() * 60000.0f),
    comp_start_blocked);
if (fault) {
    ctl_sensor_fallback_active->value() = input_fallback_enabled->value();
    if (!ctl_defrost_active->value()) {
        if (fallback_run && !comp_start_blocked && !relay_compressor->state) {
            relay_compressor->turn_on();
            ctl_comp_on_since_ms->value() = now_ms;
        } else if ((!fallback_run || door_hold_comp) && relay_compressor->state) {
            relay_compressor->turn_off();
            ctl_comp_last_off_ms->value() = now_ms;
            ctl_comp_on_since_ms->value() = 0;
        }
    }
     
    if (input_siren_enabled->value() && !ctl_alarm_silenced->value() &&
        !relay_siren->state)
        relay_siren->turn_on();
    else if (ctl_alarm_silenced->value() && relay_siren->state)
        relay_siren->turn_off();
} else {
    if (ctl_sensor_fallback_active->value()) {
        ctl_sensor_fallback_active->value() = false;
        ctl_fallback_last_toggle_ms->value() = 0;
    }
}

 
 
 
 
 
if (!fault || ctl_defrost_active->value() || ctl_defrost_dripping->value() ||
    ctl_manual_defrost_req->value()) {
    const uint32_t def_interval_ms  = (uint32_t)(ctl_defrost_interval_min->value() * 60000.0f);
    const uint32_t def_duration_ms  = (uint32_t)(ctl_defrost_duration_min->value() * 60000.0f);
    const uint32_t drip_ms          = (uint32_t)(ctl_defrost_drip_min->value()     * 60000.0f);

    if (ctl_defrost_dripping->value()) {
         
        if (relay_compressor->state) {
            relay_compressor->turn_off();
            ctl_comp_last_off_ms->value() = now_ms;
            ctl_comp_on_since_ms->value() = 0;
        }
        if ((now_ms - ctl_drip_start_ms->value()) >= drip_ms) {
            ctl_defrost_dripping->value() = false;
            ESP_LOGI("ctl", "Drip phase complete");
        }
    } else if (ctl_defrost_active->value()) {
         
         
         
         
         
         
        if (relay_compressor->state) {
            relay_compressor->turn_off();
            ctl_comp_last_off_ms->value() = now_ms;
            ctl_comp_on_since_ms->value() = 0;
        }
        bool end_by_temp = evap_ok && input_defrost_term_temp_enabled->value() &&
                           p4_ctl_defrost_term_by_temp(evap, ctl_defrost_end_c->value(), true);
        bool end_by_time = p4_ctl_defrost_timeout(ctl_defrost_on_since_ms->value(), def_duration_ms);
        if (end_by_temp || end_by_time) {
            ctl_defrost_active->value() = false;
            ctl_defrost_on_since_ms->value() = 0;
            ctl_defrost_last_end_ms->value() = now_ms;
            ctl_dew_point_defrost_triggered->value() = false;
            ctl_frost_rate_defrost_triggered->value() = false;
            {
                char detail[96];
                snprintf(detail, sizeof(detail), "%s coolroom=%.1f evap=%.1f term_c=%.1f",
                         end_by_temp ? "evap_temp" : "timeout", t, evap, ctl_defrost_end_c->value());
                p4_sd_log_event("DEFROST_END", detail);
                speak_info_defrost_off->execute();
            }
            if (input_defrost_drip_enabled->value() && drip_ms > 0) {
                ctl_defrost_dripping->value() = true;
                ctl_drip_start_ms->value() = now_ms;
            }
        }
    } else {
         
        bool manual_start = ctl_manual_defrost_req->value();
         
         
        if (input_defrost_enabled->value() && input_smart_defrost_enabled->value() && evap_ok) {
            float delta = t - evap;
            if (delta >= ctl_smart_delta_c->value() && relay_compressor->state) {
                if (ctl_defrost_delta_since_ms->value() == 0)
                    ctl_defrost_delta_since_ms->value() = now_ms;
            } else {
                ctl_defrost_delta_since_ms->value() = 0;
            }
        }
        // After a defrost ends, reuse Post-Defrost Alarm Grace as the
        // smart/dew/frost-rate lockout so early triggers cannot fire
        // back-to-back. Manual / interval / force-max are unaffected.
        const bool smart_lockout = p4_ctl_defrost_grace(
            ctl_defrost_last_end_ms->value(), defrost_grace_ms);
        bool smart_start = !fault && input_defrost_enabled->value() &&
            input_smart_defrost_enabled->value() &&
            p4_ctl_smart_defrost_ready(t, evap,
                ctl_smart_delta_c->value(),
                ctl_defrost_delta_since_ms->value(),
                (uint32_t)(ctl_smart_dwell_min->value() * 60000.0f),
                ctl_comp_on_since_ms->value(),
                (uint32_t)(15UL * 60000UL),
                smart_lockout);
        bool interval_elapsed = !fault && input_defrost_enabled->value() &&
            p4_ctl_defrost_due(ctl_defrost_last_end_ms->value(), def_interval_ms);

         
        bool force_max_start = !fault && input_defrost_enabled->value() && !manual_start &&
            p4_ctl_defrost_force_max_due(
                ctl_defrost_last_end_ms->value(),
                ctl_defrost_last_start_ms->value(),
                (uint32_t)(ctl_defrost_force_max_min->value() * 60000.0f));

         
        bool skip_scheduled = interval_elapsed && !manual_start && !force_max_start &&
            p4_ctl_defrost_skip_if_cold(
                input_defrost_skip_cold_enabled->value(), evap_ok, evap,
                ctl_defrost_skip_below_c->value());
        if (skip_scheduled) {
            ctl_defrost_last_end_ms->value() = now_ms;   
            ESP_LOGI("ctl", "Scheduled defrost skipped: evap %.1f°C <= %.1f°C",
                     evap, ctl_defrost_skip_below_c->value());
        }
        bool time_start = interval_elapsed && !skip_scheduled;

         
        bool dew_point_start = false;
        if (!fault && !smart_lockout && input_defrost_enabled->value() &&
            input_dew_point_trigger_enabled->value() &&
            input_humidity_internal_enabled->value()) {
            float internal_t = probe_internal_temp->state;
            float internal_rh = probe_internal_humidity->state;
            ctl_last_dew_point_c->value() = p4_calc_dew_point_c(internal_t, internal_rh);
            dew_point_start = p4_ctl_dew_point_defrost_ready(
                evap, ctl_last_dew_point_c->value(), ctl_dew_point_defrost_triggered->value());
            if (dew_point_start) ctl_dew_point_defrost_triggered->value() = true;
        }

         
        bool frost_rate_start = false;
        if (!fault && !smart_lockout && input_defrost_enabled->value() &&
            input_frost_rate_monitoring_enabled->value() &&
            input_humidity_internal_enabled->value()) {
            float rh = probe_internal_humidity->state;
            if (ctl_frost_rate_last_sample_ms->value() == 0) {
                if (!isnan(rh) && p4_rtd_valid(t)) {
                    ctl_frost_rate_last_humidity_pct->value() = rh;
                    ctl_frost_rate_last_temp_c->value() = t;
                    ctl_frost_rate_last_sample_ms->value() = now_ms;
                }
            } else if (p4_ctl_frost_rate_ready(
                true, false, ctl_frost_rate_defrost_triggered->value(),
                rh, t,
                ctl_frost_rate_last_humidity_pct->value(),
                ctl_frost_rate_last_temp_c->value(),
                ctl_frost_rate_last_sample_ms->value(),
                (uint32_t)(ctl_frost_rate_window_s->value() * 1000.0f),
                ctl_frost_rate_humidity_threshold_pct->value())) {
                frost_rate_start = true;
                ctl_frost_rate_defrost_triggered->value() = true;
                ESP_LOGI("ctl", "Frost-rate defrost: humidity drop over window");
            }
             
            uint32_t win_ms = (uint32_t)(ctl_frost_rate_window_s->value() * 1000.0f);
            if (ctl_frost_rate_last_sample_ms->value() != 0 &&
                (now_ms - ctl_frost_rate_last_sample_ms->value()) >= win_ms &&
                !isnan(rh) && p4_rtd_valid(t)) {
                ctl_frost_rate_last_humidity_pct->value() = rh;
                ctl_frost_rate_last_temp_c->value() = t;
                ctl_frost_rate_last_sample_ms->value() = now_ms;
            }
        } else {
            ctl_frost_rate_last_sample_ms->value() = 0;
        }

        if (manual_start || smart_start || time_start || dew_point_start ||
            force_max_start || frost_rate_start) {
            ctl_manual_defrost_req->value() = false;
            ctl_defrost_delta_since_ms->value() = 0;
            if (relay_compressor->state) {
                relay_compressor->turn_off();
                ctl_comp_last_off_ms->value() = now_ms;
                ctl_comp_on_since_ms->value() = 0;
            }
            ctl_defrost_active->value() = true;
            ctl_defrost_on_since_ms->value() = now_ms;
            ctl_defrost_last_start_ms->value() = now_ms;
             
             
            const char* reason = manual_start ? "manual" :
                                 (force_max_start ? "force_max" :
                                 (frost_rate_start ? "frost_rate" :
                                 (smart_start ? "smart_delta" :
                                 (dew_point_start ? "dew_point" : "interval"))));
            ESP_LOGI("ctl", "Defrost started: %s", reason);
            {
                char detail[112];
                snprintf(detail, sizeof(detail), "%s coolroom=%.1f evap=%.1f setpoint=%.1f",
                         reason, t, evap, ctl_setpoint->value());
                p4_sd_log_event("DEFROST_START", detail);
                speak_info_defrost_on->execute();
            }
        }

         
        if (!locked_out) {
            int cmd = p4_ctl_compressor_eval(
                t, ctl_setpoint->value(), ctl_comp_diff->value(),
                relay_compressor->state,
                rtd1_ch1_last_ms->value(),
                probe_stale_ms);
             
            // Min-run holds ON while room is already at/below cut-out (SP).
            if (cmd == -1 && relay_compressor->state && p4_rtd_valid(t)) {
                bool at_cutout = t < ctl_setpoint->value();
                if (p4_ctl_comp_min_run_hold(
                        at_cutout, true, ctl_comp_on_since_ms->value(),
                        (uint32_t)(ctl_comp_min_run_min->value() * 60000.0f))) {
                    cmd = 0;
                }
            }
            if (cmd == 1 && relay_board_ok && !door_hold_comp) {
                relay_compressor->turn_on();
                if (ctl_comp_on_since_ms->value() == 0) ctl_comp_on_since_ms->value() = now_ms;
            }
            if (cmd == -1 || door_hold_comp) {
                if (relay_compressor->state) {
                    relay_compressor->turn_off();
                    ctl_comp_last_off_ms->value() = now_ms;
                    ctl_comp_on_since_ms->value() = 0;
                }
            }
        }
    }
}

 
 
 
 
 
 
 
 
 
 
if (input_door_sensor_enabled->value() && ctl_door_open_since_ms->value() > 0) {
    const uint32_t door_delay_ms = (uint32_t)(ctl_door_alarm_delay_s->value() * 1000.0f);
    if ((now_ms - ctl_door_open_since_ms->value()) >= door_delay_ms)
        ctl_door_alarm_active->value() = true;
}

 
if (!fault && !in_grace) {
    const float sp = ctl_setpoint->value();
    const float hi_delta = ctl_alarm_high_delta->value();
    const float lo_delta = ctl_alarm_low_delta->value();
    const float alarm_hyst = ctl_alarm_hysteresis_c->value();
    bool raw_hi = p4_ctl_alarm_high(t, sp, hi_delta);
    bool raw_lo = p4_ctl_alarm_low(t, sp, lo_delta);

    // Latch hi/lo after persist; clear only after recovery hysteresis band
    // (p4_ctl_alarm_hysteresis_clear) so the siren does not chatter at the
    // threshold.
    if (ctl_alarm_high_active->value()) {
        if (p4_ctl_alarm_hysteresis_clear(t, sp, hi_delta, alarm_hyst, true)) {
            ctl_alarm_high_active->value() = false;
            ctl_high_alarm_since_ms->value() = 0;
        }
    } else if (raw_hi) {
        if (ctl_high_alarm_since_ms->value() == 0) ctl_high_alarm_since_ms->value() = now_ms;
        if (p4_ctl_alarm_persisted(ctl_high_alarm_since_ms->value(), alarm_persist_ms))
            ctl_alarm_high_active->value() = true;
    } else {
        ctl_high_alarm_since_ms->value() = 0;
    }

    if (ctl_alarm_low_active->value()) {
        if (p4_ctl_alarm_hysteresis_clear(t, sp, lo_delta, alarm_hyst, false)) {
            ctl_alarm_low_active->value() = false;
            ctl_low_alarm_since_ms->value() = 0;
        }
    } else if (raw_lo) {
        if (ctl_low_alarm_since_ms->value() == 0) ctl_low_alarm_since_ms->value() = now_ms;
        if (p4_ctl_alarm_persisted(ctl_low_alarm_since_ms->value(), alarm_persist_ms))
            ctl_alarm_low_active->value() = true;
    } else {
        ctl_low_alarm_since_ms->value() = 0;
    }

    bool hi = ctl_alarm_high_active->value();
    bool lo = ctl_alarm_low_active->value();
     
    bool nc = p4_ctl_no_cool_alarm(t, ctl_setpoint->value(), ctl_comp_diff->value(),
                                    relay_compressor->state, ctl_comp_on_since_ms->value(),
                                    (uint32_t)(ctl_no_cool_alarm_min->value() * 60000.0f));
    ctl_no_cool_alarm_active->value() = nc;
     
     
    bool ice = false;
    if (input_ice_detect_enabled->value() && evap_ok &&
        !ctl_defrost_active->value() && relay_compressor->state &&
        p4_ctl_ice_condition(t, evap, ctl_ice_delta_c->value())) {
        if (ctl_ice_since_ms->value() == 0) ctl_ice_since_ms->value() = now_ms;
        ice = p4_ctl_ice_alarm_dwelt(
            ctl_ice_since_ms->value(),
            (uint32_t)(ctl_ice_dwell_min->value() * 60000.0f));
    } else {
        ctl_ice_since_ms->value() = 0;
    }
    ctl_ice_alarm_active->value() = ice;
     
     
     
    bool any_alarm = hi || lo || nc || ice || ctl_door_alarm_active->value() ||
                     ctl_ct_fail_to_start_active->value() ||
                     ctl_ct_stuck_on_active->value() ||
                     ctl_ct_overcurrent_active->value();
    if (input_siren_enabled->value()) {
        if (any_alarm && !ctl_alarm_silenced->value() && !relay_siren->state)
            relay_siren->turn_on();
        if ((!any_alarm || ctl_alarm_silenced->value()) && relay_siren->state)
            relay_siren->turn_off();
    }
} else if (!fault) {
     
    ctl_alarm_high_active->value() = false;
    ctl_alarm_low_active->value()  = false;
    ctl_no_cool_alarm_active->value() = false;
    ctl_ice_alarm_active->value()  = false;
    ctl_ice_since_ms->value() = 0;
    ctl_alarm_silenced->value() = false;
    ctl_alarm_silenced_mask->value() = 0;
    if (relay_siren->state) relay_siren->turn_off();
}

 
// CT run-proof — independent of temp grace/fault. Only when CT enabled+online
// and run-proof opted in. Clears silently when disarmed (no false alarms).
{
    const bool ct_armed = p4_ctl_ct_run_proof_armed(
        input_ct_clamp_enabled->value(), hw_rs485_ct_ok->value(),
        input_ct_run_proof_enabled->value());
    const float ct_amps = ct_clamp_current->state;
    const uint32_t ct_persist_ms =
        (uint32_t)(ctl_ct_run_proof_delay_s->value() * 1000.0f);
    const bool defrost_or_drip =
        ctl_defrost_active->value() || ctl_defrost_dripping->value();

    if (!ct_armed) {
        ctl_ct_fail_since_ms->value() = 0;
        ctl_ct_stuck_since_ms->value() = 0;
        ctl_ct_over_since_ms->value() = 0;
        ctl_ct_fail_to_start_active->value() = false;
        ctl_ct_stuck_on_active->value() = false;
        ctl_ct_overcurrent_active->value() = false;
    } else {
        const bool fail_cond = p4_ctl_ct_fail_to_start_condition(
            true, relay_compressor->state, ct_amps, ctl_ct_idle_max_a->value());
        if (fail_cond) {
            if (ctl_ct_fail_since_ms->value() == 0)
                ctl_ct_fail_since_ms->value() = now_ms;
            if (p4_ctl_alarm_persisted(ctl_ct_fail_since_ms->value(), ct_persist_ms))
                ctl_ct_fail_to_start_active->value() = true;
        } else {
            ctl_ct_fail_since_ms->value() = 0;
            ctl_ct_fail_to_start_active->value() = false;
        }

        const bool stuck_cond = p4_ctl_ct_stuck_on_condition(
            true, relay_compressor->state, defrost_or_drip, ct_amps,
            ctl_ct_run_min_a->value());
        if (stuck_cond) {
            if (ctl_ct_stuck_since_ms->value() == 0)
                ctl_ct_stuck_since_ms->value() = now_ms;
            if (p4_ctl_alarm_persisted(ctl_ct_stuck_since_ms->value(), ct_persist_ms))
                ctl_ct_stuck_on_active->value() = true;
        } else {
            ctl_ct_stuck_since_ms->value() = 0;
            ctl_ct_stuck_on_active->value() = false;
        }

        const bool over_cond = p4_ctl_ct_overcurrent_condition(
            true, ct_amps, ctl_ct_overcurrent_a->value());
        if (over_cond) {
            if (ctl_ct_over_since_ms->value() == 0)
                ctl_ct_over_since_ms->value() = now_ms;
            if (p4_ctl_alarm_persisted(ctl_ct_over_since_ms->value(), ct_persist_ms))
                ctl_ct_overcurrent_active->value() = true;
        } else {
            ctl_ct_over_since_ms->value() = 0;
            ctl_ct_overcurrent_active->value() = false;
        }
    }
}

{
    uint16_t mask = p4_ctl_alarm_mask(
        ctl_alarm_high_active->value(), ctl_alarm_low_active->value(),
        ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(),
        ctl_ice_alarm_active->value(), ctl_probe_fault->value(),
        ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(),
        ctl_ct_overcurrent_active->value());
    if (p4_ctl_alarm_silence_should_clear(
            ctl_alarm_silenced->value(), ctl_alarm_silenced_mask->value(), mask)) {
        const bool new_cond = mask != 0;
        ctl_alarm_silenced->value() = false;
        ctl_alarm_silenced_mask->value() = 0;
        if (new_cond)
            ESP_LOGI("ui", "Alarm silence lifted — new condition active");
    }
     
     
    const bool any_for_siren = mask != 0;
    if (input_siren_enabled->value()) {
        if (any_for_siren && !ctl_alarm_silenced->value() && !relay_siren->state)
            relay_siren->turn_on();
        if ((!any_for_siren || ctl_alarm_silenced->value()) && relay_siren->state)
            relay_siren->turn_off();
    }
}

 
 
 
 
 
 
{
    bool alarm_hi_now = ctl_alarm_high_active->value();
    if (alarm_hi_now && !ctl_alarm_hi_logged->value()) {
        ctl_alarm_hi_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "coolroom=%.1f setpoint=%.1f delta=%.1f",
                 t, ctl_setpoint->value(), ctl_alarm_high_delta->value());
        p4_sd_log_event("ALARM_HI", d);
        speak_alarm_high->execute();
    } else if (!alarm_hi_now && ctl_alarm_hi_logged->value()) {
        ctl_alarm_hi_logged->value() = false;
        char d[64];
        snprintf(d, sizeof(d), "coolroom=%.1f setpoint=%.1f", t, ctl_setpoint->value());
        p4_sd_log_event("ALARM_HI_CLEAR", d);
    }

    bool alarm_lo_now = ctl_alarm_low_active->value();
    if (alarm_lo_now && !ctl_alarm_lo_logged->value()) {
        ctl_alarm_lo_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "coolroom=%.1f setpoint=%.1f delta=%.1f",
                 t, ctl_setpoint->value(), ctl_alarm_low_delta->value());
        p4_sd_log_event("ALARM_LO", d);
        speak_alarm_low->execute();
    } else if (!alarm_lo_now && ctl_alarm_lo_logged->value()) {
        ctl_alarm_lo_logged->value() = false;
        char d[64];
        snprintf(d, sizeof(d), "coolroom=%.1f setpoint=%.1f", t, ctl_setpoint->value());
        p4_sd_log_event("ALARM_LO_CLEAR", d);
    }

    if (fault && !ctl_probe_fault_logged->value()) {
        ctl_probe_fault_logged->value() = true;
        p4_sd_log_event("PROBE_FAULT", "probe1 stale_or_invalid");
        speak_alarm_probe->execute();
    } else if (!fault && ctl_probe_fault_logged->value()) {
        ctl_probe_fault_logged->value() = false;
        p4_sd_log_event("PROBE_FAULT_CLEAR", "probe1 recovered");
    }

    bool door_now = ctl_door_alarm_active->value();
    if (door_now && !ctl_door_alarm_logged->value()) {
        ctl_door_alarm_logged->value() = true;
        char d[80];
        uint32_t open_s = ctl_door_open_since_ms->value() > 0 ?
            (now_ms - ctl_door_open_since_ms->value()) / 1000U : 0U;
        snprintf(d, sizeof(d), "open_for=%us delay=%.0fs", (unsigned) open_s, ctl_door_alarm_delay_s->value());
        p4_sd_log_event("DOOR_ALARM", d);
        speak_alarm_door->execute();
    } else if (!door_now && ctl_door_alarm_logged->value()) {
        ctl_door_alarm_logged->value() = false;
        p4_sd_log_event("DOOR_ALARM_CLEAR", "door closed");
    }

    bool nc_now = ctl_no_cool_alarm_active->value();
    if (nc_now && !ctl_no_cool_alarm_logged->value()) {
        ctl_no_cool_alarm_logged->value() = true;
        char d[112];
        snprintf(d, sizeof(d), "coolroom=%.1f setpoint=%.1f diff=%.1f compressor_on=%d",
                 t, ctl_setpoint->value(), ctl_comp_diff->value(), (int) relay_compressor->state);
        p4_sd_log_event("NO_COOL_ALARM", d);
        speak_alarm_no_cool->execute();
    } else if (!nc_now && ctl_no_cool_alarm_logged->value()) {
        ctl_no_cool_alarm_logged->value() = false;
        p4_sd_log_event("NO_COOL_ALARM_CLEAR", "room cooling normally");
    }

    bool ice_now = ctl_ice_alarm_active->value();
    if (ice_now && !ctl_ice_alarm_logged->value()) {
        ctl_ice_alarm_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "coolroom=%.1f evap=%.1f delta_threshold=%.1f", t, evap, ctl_ice_delta_c->value());
        p4_sd_log_event("ICE_ALARM", d);
        speak_alarm_ice->execute();
    } else if (!ice_now && ctl_ice_alarm_logged->value()) {
        ctl_ice_alarm_logged->value() = false;
        p4_sd_log_event("ICE_ALARM_CLEAR", "delta recovered");
    }

    bool ct_fail_now = ctl_ct_fail_to_start_active->value();
    if (ct_fail_now && !ctl_ct_fail_to_start_logged->value()) {
        ctl_ct_fail_to_start_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "amps=%.2f idle_max=%.2f compressor_on=1",
                 ct_clamp_current->state, ctl_ct_idle_max_a->value());
        p4_sd_log_event("CT_FAIL_TO_START", d);
    } else if (!ct_fail_now && ctl_ct_fail_to_start_logged->value()) {
        ctl_ct_fail_to_start_logged->value() = false;
        p4_sd_log_event("CT_FAIL_TO_START_CLEAR", "run current seen or disarmed");
    }

    bool ct_stuck_now = ctl_ct_stuck_on_active->value();
    if (ct_stuck_now && !ctl_ct_stuck_on_logged->value()) {
        ctl_ct_stuck_on_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "amps=%.2f run_min=%.2f compressor_on=0",
                 ct_clamp_current->state, ctl_ct_run_min_a->value());
        p4_sd_log_event("CT_STUCK_ON", d);
    } else if (!ct_stuck_now && ctl_ct_stuck_on_logged->value()) {
        ctl_ct_stuck_on_logged->value() = false;
        p4_sd_log_event("CT_STUCK_ON_CLEAR", "current dropped or disarmed");
    }

    bool ct_over_now = ctl_ct_overcurrent_active->value();
    if (ct_over_now && !ctl_ct_overcurrent_logged->value()) {
        ctl_ct_overcurrent_logged->value() = true;
        char d[96];
        snprintf(d, sizeof(d), "amps=%.2f limit=%.2f",
                 ct_clamp_current->state, ctl_ct_overcurrent_a->value());
        p4_sd_log_event("CT_OVERCURRENT", d);
    } else if (!ct_over_now && ctl_ct_overcurrent_logged->value()) {
        ctl_ct_overcurrent_logged->value() = false;
        p4_sd_log_event("CT_OVERCURRENT_CLEAR", "current below limit or disarmed");
    }

     
     
     
    if (!hw_rs485_relay_ok->value() && !ctl_relay_board_offline_logged->value()) {
        ctl_relay_board_offline_logged->value() = true;
        p4_sd_log_event("RELAY_BOARD_OFFLINE", "modbus relay no response");
        speak_alarm_relay_board->execute();
    } else if (hw_rs485_relay_ok->value() && ctl_relay_board_offline_logged->value()) {
        ctl_relay_board_offline_logged->value() = false;
        p4_sd_log_event("RELAY_BOARD_ONLINE", "modbus relay recovered");
    }

     
    const bool rtd_expected = true;
    const bool rtd_offline = rtd_expected && !hw_rs485_rtd1_ok->value();
    if (rtd_offline && !ctl_temp_board_offline_logged->value()) {
        ctl_temp_board_offline_logged->value() = true;
        p4_sd_log_event("TEMP_BOARD_OFFLINE", "modbus rtd no response");
        speak_alarm_temp_board->execute();
    } else if (!rtd_offline && ctl_temp_board_offline_logged->value()) {
        ctl_temp_board_offline_logged->value() = false;
        p4_sd_log_event("TEMP_BOARD_ONLINE", "modbus rtd recovered");
    }

    if (input_humidity_internal_enabled->value())
        p4_i2c_hum_try_recover(i2c_sht31);
    if (input_humidity_external_enabled->value())
        p4_i2c_hum_try_recover(i2c_sht20);

    const bool humidity_offline = input_humidity_internal_enabled->value() &&
        !p4_i2c_hum_online(
            true,
            i2c_sht31->is_failed(),
            i2c_sht31->status_has_warning(),
            sht31_internal_temp_raw->state);
    if (humidity_offline && !ctl_humidity_sensor_offline_logged->value()) {
        ctl_humidity_sensor_offline_logged->value() = true;
        p4_sd_log_event("HUMIDITY_SENSOR_OFFLINE", "sht31 no response");
        speak_alarm_humidity_sensor->execute();
    } else if (!humidity_offline && ctl_humidity_sensor_offline_logged->value()) {
        ctl_humidity_sensor_offline_logged->value() = false;
        p4_sd_log_event("HUMIDITY_SENSOR_ONLINE", "sht31 recovered");
    }

    const bool ambient_offline = input_humidity_external_enabled->value() &&
        !p4_i2c_hum_online(
            true,
            i2c_sht20->is_failed(),
            i2c_sht20->status_has_warning(),
            sht20_external_temp_raw->state);
    if (ambient_offline && !ctl_ambient_sensor_offline_logged->value()) {
        ctl_ambient_sensor_offline_logged->value() = true;
        p4_sd_log_event("AMBIENT_SENSOR_OFFLINE", "sht20 no response");
        speak_alarm_ambient_sensor->execute();
    } else if (!ambient_offline && ctl_ambient_sensor_offline_logged->value()) {
        ctl_ambient_sensor_offline_logged->value() = false;
        p4_sd_log_event("AMBIENT_SENSOR_ONLINE", "sht20 recovered");
    }

     
    const bool ct_offline = input_ct_clamp_enabled->value() && !hw_rs485_ct_ok->value();
    if (ct_offline && !ctl_ct_clamp_offline_logged->value()) {
        ctl_ct_clamp_offline_logged->value() = true;
        p4_sd_log_event("CT_CLAMP_OFFLINE", "modbus ct no response");
    } else if (!ct_offline && ctl_ct_clamp_offline_logged->value()) {
        ctl_ct_clamp_offline_logged->value() = false;
        if (input_ct_clamp_enabled->value())
            p4_sd_log_event("CT_CLAMP_ONLINE", "modbus ct recovered");
    }
}

 
const uint32_t log_every_ticks = (uint32_t)(log_interval_min) * 6U;
log_tick_count->value() = log_tick_count->value() + 1U;
if (sd_card_ok->value() && log_tick_count->value() >= log_every_ticks) {
    log_tick_count->value() = 0;
    p4_sd_log_temps(
        probe1_temp->state, probe2_temp->state, probe_external_temp->state,
        ctl_setpoint->value(), relay_compressor->state, ctl_defrost_active->value(),
        ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_probe_fault->value(),
        ct_clamp_current->state);
}

   
   
   
   
  if (ctl_wifi_connected->value() && input_ntfy_enabled->value()) {
    bool ntfy_hi = ctl_alarm_high_active->value();
    bool ntfy_lo = ctl_alarm_low_active->value();
    bool ntfy_pf = ctl_probe_fault->value();
    if (ntfy_hi && !ntfy_alarm_hi_sent->value()) {
      ntfy_alarm_hi_sent->value() = true;
      ntfy_high_alarm_request->trigger();
    } else if (!ntfy_hi && ntfy_alarm_hi_sent->value()) {
      ntfy_alarm_hi_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_lo && !ntfy_alarm_lo_sent->value()) {
      ntfy_alarm_lo_sent->value() = true;
      ntfy_low_alarm_request->trigger();
    } else if (!ntfy_lo && ntfy_alarm_lo_sent->value()) {
      ntfy_alarm_lo_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_pf && !ntfy_probe_fault_sent->value()) {
      ntfy_probe_fault_sent->value() = true;
      ntfy_probe_fault_request->trigger();
    } else if (!ntfy_pf && ntfy_probe_fault_sent->value()) {
      ntfy_probe_fault_sent->value() = false;
    }
     
     
    bool ntfy_door = ctl_door_alarm_active->value();
    bool ntfy_nc   = ctl_no_cool_alarm_active->value();
    bool ntfy_ice  = ctl_ice_alarm_active->value();
    if (ntfy_door && !ntfy_door_alarm_sent->value()) {
      ntfy_door_alarm_sent->value() = true;
      ntfy_door_alarm_request->trigger();
    } else if (!ntfy_door && ntfy_door_alarm_sent->value()) {
      ntfy_door_alarm_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_nc && !ntfy_no_cool_alarm_sent->value()) {
      ntfy_no_cool_alarm_sent->value() = true;
      ntfy_no_cool_alarm_request->trigger();
    } else if (!ntfy_nc && ntfy_no_cool_alarm_sent->value()) {
      ntfy_no_cool_alarm_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_ice && !ntfy_ice_alarm_sent->value()) {
      ntfy_ice_alarm_sent->value() = true;
      ntfy_ice_alarm_request->trigger();
    } else if (!ntfy_ice && ntfy_ice_alarm_sent->value()) {
      ntfy_ice_alarm_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    // CT run-proof — push only (no speak_* scripts).
    bool ntfy_ct_fail = ctl_ct_fail_to_start_active->value();
    bool ntfy_ct_stuck = ctl_ct_stuck_on_active->value();
    bool ntfy_ct_over = ctl_ct_overcurrent_active->value();
    if (ntfy_ct_fail && !ntfy_ct_fail_sent->value()) {
      ntfy_ct_fail_sent->value() = true;
      ntfy_ct_fail_request->trigger();
    } else if (!ntfy_ct_fail && ntfy_ct_fail_sent->value()) {
      ntfy_ct_fail_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_ct_stuck && !ntfy_ct_stuck_sent->value()) {
      ntfy_ct_stuck_sent->value() = true;
      ntfy_ct_stuck_request->trigger();
    } else if (!ntfy_ct_stuck && ntfy_ct_stuck_sent->value()) {
      ntfy_ct_stuck_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }
    if (ntfy_ct_over && !ntfy_ct_over_sent->value()) {
      ntfy_ct_over_sent->value() = true;
      ntfy_ct_over_request->trigger();
    } else if (!ntfy_ct_over && ntfy_ct_over_sent->value()) {
      ntfy_ct_over_sent->value() = false;
      ntfy_alarm_clear_request->trigger();
    }

    if (!sd_card_ok->value() && !ntfy_sd_failure_sent->value()) {
      ntfy_sd_failure_sent->value() = true;
      ntfy_sd_failure_request->trigger();
    } else if (sd_card_ok->value() && ntfy_sd_failure_sent->value()) {
      ntfy_sd_failure_sent->value() = false;
    }
     
     
    const bool ntfy_relay_off = !hw_rs485_relay_ok->value();
    const bool ntfy_rtd_expected = true;
    const bool ntfy_temp_off = ntfy_rtd_expected && !hw_rs485_rtd1_ok->value();
    const bool ntfy_hum_off = input_humidity_internal_enabled->value() &&
        !p4_i2c_hum_online(
            true,
            i2c_sht31->is_failed(),
            i2c_sht31->status_has_warning(),
            sht31_internal_temp_raw->state);
    const bool ntfy_amb_off = input_humidity_external_enabled->value() &&
        !p4_i2c_hum_online(
            true,
            i2c_sht20->is_failed(),
            i2c_sht20->status_has_warning(),
            sht20_external_temp_raw->state);
    if (ntfy_relay_off && !ntfy_relay_board_offline_sent->value()) {
      ntfy_relay_board_offline_sent->value() = true;
      ntfy_relay_board_offline_request->trigger();
    } else if (!ntfy_relay_off && ntfy_relay_board_offline_sent->value()) {
      ntfy_relay_board_offline_sent->value() = false;
      ntfy_relay_board_online_request->trigger();
    }
    if (ntfy_temp_off && !ntfy_temp_board_offline_sent->value()) {
      ntfy_temp_board_offline_sent->value() = true;
      ntfy_temp_board_offline_request->trigger();
    } else if (!ntfy_temp_off && ntfy_temp_board_offline_sent->value()) {
      ntfy_temp_board_offline_sent->value() = false;
      ntfy_temp_board_online_request->trigger();
    }
    if (ntfy_hum_off && !ntfy_humidity_sensor_offline_sent->value()) {
      ntfy_humidity_sensor_offline_sent->value() = true;
      ntfy_humidity_sensor_offline_request->trigger();
    } else if (!ntfy_hum_off && ntfy_humidity_sensor_offline_sent->value()) {
      ntfy_humidity_sensor_offline_sent->value() = false;
      ntfy_humidity_sensor_online_request->trigger();
    }
    if (ntfy_amb_off && !ntfy_ambient_sensor_offline_sent->value()) {
      ntfy_ambient_sensor_offline_sent->value() = true;
      ntfy_ambient_sensor_offline_request->trigger();
    } else if (!ntfy_amb_off && ntfy_ambient_sensor_offline_sent->value()) {
      ntfy_ambient_sensor_offline_sent->value() = false;
      ntfy_ambient_sensor_online_request->trigger();
    }
  } else {
     
    ntfy_alarm_hi_sent->value() = false;
    ntfy_alarm_lo_sent->value() = false;
    ntfy_probe_fault_sent->value() = false;
    ntfy_door_alarm_sent->value() = false;
    ntfy_no_cool_alarm_sent->value() = false;
    ntfy_ice_alarm_sent->value() = false;
    ntfy_ct_fail_sent->value() = false;
    ntfy_ct_stuck_sent->value() = false;
    ntfy_ct_over_sent->value() = false;
    ntfy_sd_failure_sent->value() = false;
    ntfy_relay_board_offline_sent->value() = false;
    ntfy_temp_board_offline_sent->value() = false;
    ntfy_humidity_sensor_offline_sent->value() = false;
    ntfy_ambient_sensor_offline_sent->value() = false;
  }

 
 
if (door_hold_comp && relay_compressor->state) {
    relay_compressor->turn_off();
    ctl_comp_last_off_ms->value() = now_ms;
    ctl_comp_on_since_ms->value() = 0;
}
 
 
 
 
 
 
 
 
 
if ((!ctl_relay_compressor_enabled->value() || !hw_rs485_relay_ok->value()) &&
    relay_compressor->state) {
    relay_compressor->turn_off();
    ctl_comp_last_off_ms->value() = now_ms;
    ctl_comp_on_since_ms->value() = 0;
}
 
 
{
    const bool fan_want =
        ctl_relay_fan_enabled->value() && hw_rs485_relay_ok->value() &&
        relay_compressor->state &&
        !ctl_defrost_active->value() && !ctl_defrost_dripping->value();
    if (fan_want && !relay_fan->state) relay_fan->turn_on();
    else if (!fan_want && relay_fan->state) relay_fan->turn_off();
}
if (!ctl_relay_light_enabled->value() && relay_light->state)
    relay_light->turn_off();
if (!ctl_relay_siren_enabled->value() && relay_siren->state)
    relay_siren->turn_off();

 
 
 
 
 
{
    bool comp_is_on = relay_compressor->state;
    if (comp_is_on != comp_was_on) {
        const char* reason = fault ? "sensor_fallback" :
                             (ctl_defrost_active->value() || ctl_defrost_dripping->value()) ? "defrost" :
                             locked_out ? "lockout_end" : "hysteresis";
        char d[112];
        snprintf(d, sizeof(d), "%s coolroom=%.1f setpoint=%.1f diff=%.1f",
                 reason, t, ctl_setpoint->value(), ctl_comp_diff->value());
        p4_sd_log_event(comp_is_on ? "COMPRESSOR_ON" : "COMPRESSOR_OFF", d);
        if (comp_is_on) speak_info_cooling_on->execute();
        else            speak_info_cooling_off->execute();
    }
}

 
 
 
 
 
 
{
    bool sd_is_ready = p4_sd_is_ready();
    if (sd_was_ready && !sd_is_ready) {
        sd_card_ok->value() = false;
        ESP_LOGE("ctl", "SD card failure detected during runtime — logging/backup now unavailable");
    }
}

}

// ─── CPU 0 worker ───────────────────────────────────────────────────────────
// YAML 10 s interval on loopTask (CPU 1) only gives the semaphore; the body
// above runs on this pinned task. See file header for sharing rules.

inline SemaphoreHandle_t &p4_ctl_tick_sem_ref() {
  static SemaphoreHandle_t sem = nullptr;
  return sem;
}
inline uint32_t &p4_ctl_tick_param_probe_stale_ms_ref() {
  static uint32_t v = 30000;
  return v;
}
inline uint32_t &p4_ctl_tick_param_startup_grace_max_min_ref() {
  static uint32_t v = 240;
  return v;
}
inline uint32_t &p4_ctl_tick_param_log_interval_min_ref() {
  static uint32_t v = 5;
  return v;
}

inline void p4_ctl_tick_worker_task_fn(void *arg) {
  (void) arg;
  for (;;) {
    if (p4_ctl_tick_sem_ref() != nullptr)
      xSemaphoreTake(p4_ctl_tick_sem_ref(), portMAX_DELAY);
    p4_ctl_tick(p4_ctl_tick_param_probe_stale_ms_ref(),
                p4_ctl_tick_param_startup_grace_max_min_ref(),
                p4_ctl_tick_param_log_interval_min_ref());
  }
}

/// Create the C0 control worker if needed. Safe to call repeatedly.
/// MUST NOT be called from on_boot / Component::setup() — task create + pin
/// to CPU 0 during hosted Wi-Fi bring-up starved loopTask until task_wdt.
/// Call only from post-setup intervals (1 s UI tick / 10 s signal).
inline void p4_ctl_tick_worker_start(uint32_t probe_stale_ms,
                                    uint32_t startup_grace_max_min,
                                    uint32_t log_interval_min) {
  p4_ctl_tick_param_probe_stale_ms_ref() = probe_stale_ms;
  p4_ctl_tick_param_startup_grace_max_min_ref() = startup_grace_max_min;
  p4_ctl_tick_param_log_interval_min_ref() = log_interval_min;
  if (p4_ctl_tick_sem_ref() == nullptr) {
    // xSemaphoreCreateBinary starts empty — worker blocks until first Give.
    p4_ctl_tick_sem_ref() = xSemaphoreCreateBinary();
    if (p4_ctl_tick_sem_ref() == nullptr) {
      ESP_LOGE("ctl", "Failed to create p4_ctl semaphore");
      return;
    }
  }
  static TaskHandle_t handle = nullptr;
  if (handle != nullptr) return;
  // 16 KiB: control body + SD append + event snprintf on C0.
  // Priority 5: below wifi/hosted, above idle — never run during setup().
  const BaseType_t ok = xTaskCreatePinnedToCore(
      p4_ctl_tick_worker_task_fn, "p4_ctl", 16384, nullptr, 5, &handle, 0);
  if (ok != pdPASS) {
    ESP_LOGE("ctl", "Failed to start p4_ctl worker on CPU 0");
    handle = nullptr;
  } else {
    ESP_LOGI("ctl", "p4_ctl worker started on CPU 0 (post-setup)");
  }
}

/// Wake the C0 worker (call from the 10 s YAML interval on CPU 1).
/// Lazily starts the worker on first call if setup deferred it.
inline void p4_ctl_tick_signal(uint32_t probe_stale_ms,
                               uint32_t startup_grace_max_min,
                               uint32_t log_interval_min) {
  p4_ctl_tick_worker_start(probe_stale_ms, startup_grace_max_min, log_interval_min);
  if (p4_ctl_tick_sem_ref() != nullptr)
    xSemaphoreGive(p4_ctl_tick_sem_ref());
}
