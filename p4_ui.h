#pragma once
// ============================================================================
// p4_ui.h — ESP32-P4 Coolroom: home-screen UI helpers
//
// PRINCIPLE: Hot-path LVGL / display math lives here. YAML intervals call these;
// they must not change control or alarm decisions — only presentation.
//
// Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
// ============================================================================

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/time.h>
#include <lvgl.h>
#include "p4_helpers.h"

// ─── Dial scale (shared by coolroom / setpoint / ambient arcs) ─────────────
// Maps °C onto LVGL arc value 0..100 across the configured dial range. The
// same range drives the tick ring, so ticks and fills always agree.

inline int p4_ui_scale_pct_(float t_c, float lo_c, float hi_c) {
    const float span = hi_c - lo_c;
    if (span <= 0.0f) return 0;
    int v = (int)((t_c - lo_c) / span * 100.0f);
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

/// Probe / measured reading: invalid → 0 (empty arc).
inline int p4_ui_temp_to_arc_pct(float t_c, float lo_c, float hi_c) {
    if (!p4_rtd_valid(t_c)) return 0;
    return p4_ui_scale_pct_(t_c, lo_c, hi_c);
}

/// Setpoint dial (always a finite stored value — no probe-validity gate).
inline int p4_ui_setpoint_to_arc_pct(float sp_c, float lo_c, float hi_c) {
    return p4_ui_scale_pct_(sp_c, lo_c, hi_c);
}

// Horseshoe geometry matches home arcs: start_angle 150 → end_angle 30 (240°).
inline float p4_ui_arc_pct_to_angle_(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    float a = 150.0f + (pct / 100.0f) * 240.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
}

/// Pin a marker to an LVGL arc's indicator tip using the arc's own geometry
/// (same radius/centre as that widget — never the ambient ring).
inline void p4_ui_place_dot_on_arc_(lv_obj_t *arc, lv_obj_t *dot, int pct) {
    if (arc == nullptr || dot == nullptr) return;
    const int32_t prev = lv_arc_get_value(arc);
    lv_arc_set_value(arc, pct);
    lv_arc_align_obj_to_angle(arc, dot, 0);
    lv_arc_set_value(arc, prev);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dot);
}

/// Active segment between current and setpoint (HA thermostat gap on the ring).
inline void p4_ui_arc_set_delta_overlay(lv_obj_t *arc, int pct_temp, int pct_set) {
    if (arc == nullptr) return;
    int lo = pct_temp < pct_set ? pct_temp : pct_set;
    int hi = pct_temp < pct_set ? pct_set : pct_temp;
    if (hi - lo < 1) {
        lv_arc_set_angles(arc, 0, 0);
        return;
    }
    lv_arc_set_angles(arc, p4_ui_arc_pct_to_angle_(lo), p4_ui_arc_pct_to_angle_(hi));
}

/// Hide LVGL's built-in arc knob so ambient (or any ring) cannot show a fake tip dot.
inline void p4_ui_hide_arc_knob_(lv_obj_t *arc) {
    if (arc == nullptr) return;
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_width(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_height(arc, 0, LV_PART_KNOB);
}

/// Seconds left in compressor off-delay (0 = ready / not counting).
/// Used by the 1 s home LVGL path so the amber countdown stays snappy even when
/// the published countdown sensors run slower (diagnostic / web).
inline float p4_ui_comp_lockout_remaining_s(uint32_t last_off_ms, float lockout_min) {
    if (last_off_ms == 0) return 0.0f;
    const uint32_t lockout_ms = (uint32_t)(lockout_min * 60000.0f);
    const uint32_t elapsed = millis() - last_off_ms;
    if (elapsed >= lockout_ms) return 0.0f;
    return (lockout_ms - elapsed) / 1000.0f;
}

/// Seconds until the next scheduled defrost (0 = due / active / not counting).
/// Same math as the published `defrost_countdown_sec` sensor; home LVGL calls
/// this every 1 s so the rail countdown is not tied to the 3 s sensor publish.
inline float p4_ui_next_defrost_remaining_s(bool defrost_active, bool defrost_dripping,
                                           uint32_t last_end_ms, float interval_min) {
    if (defrost_active || defrost_dripping) return 0.0f;
    if (last_end_ms == 0) return 0.0f;
    const uint32_t interval_ms = (uint32_t)(interval_min * 60000.0f);
    const uint32_t elapsed = millis() - last_end_ms;
    if (elapsed >= interval_ms) return 0.0f;
    return (interval_ms - elapsed) / 1000.0f;
}

/// Compact countdown for home rail labels. Uses H:MM:SS once past an hour
/// (defrost intervals are often multi-hour); otherwise M:SS like lockout.
inline std::string p4_ui_fmt_countdown_s(float rem_s) {
    if (rem_s <= 0.0f) return std::string("");
    int total_s = (int) rem_s;
    char buf[16];
    if (total_s >= 3600) {
        snprintf(buf, sizeof(buf), "%d:%02d:%02d",
                 total_s / 3600, (total_s % 3600) / 60, total_s % 60);
    } else {
        snprintf(buf, sizeof(buf), "%d:%02d", total_s / 60, total_s % 60);
    }
    return std::string(buf);
}

/// Alt home gauge (HA thermostat-style, display-only — no drag-to-set):
///   - set_arc: dim track + soft cyan fill to setpoint (target path)
///   - cur_arc: invisible fill; used only as the coolroom radius reference for the current pip
///   - delta_arc: blue band between current and setpoint (active gap)
///   - knob_set / knob_cur: aligned to set_arc / cur_arc via lv_arc_align_obj_to_angle
/// Call only when home2 is scrolled into view (see home2_view_active).
inline void p4_ui_update_home2_cooling(lv_obj_t *set_arc, lv_obj_t *cur_arc,
                                      lv_obj_t *delta_arc, lv_obj_t *knob_set,
                                      lv_obj_t *knob_cur, float temp_c, float sp_c,
                                      float lo_c, float hi_c,
                                      lv_obj_t *ambient_arc = nullptr) {
    const int pct_sp = p4_ui_setpoint_to_arc_pct(sp_c, lo_c, hi_c);
    const bool temp_ok = p4_rtd_valid(temp_c);
    const int pct_t = temp_ok ? p4_ui_temp_to_arc_pct(temp_c, lo_c, hi_c) : 0;

    // Ambient must never show LVGL's native tip knob (reads as "dot on pink ring").
    p4_ui_hide_arc_knob_(ambient_arc);
    p4_ui_hide_arc_knob_(set_arc);
    p4_ui_hide_arc_knob_(cur_arc);
    p4_ui_hide_arc_knob_(delta_arc);

    if (set_arc != nullptr) lv_arc_set_value(set_arc, pct_sp);
    // Keep indicator invisible; value is set only while placing the current pip.
    if (cur_arc != nullptr) lv_arc_set_value(cur_arc, 0);

    // Bind setpoint thumb to the OUTER coolroom set_arc (440), not ambient.
    if (set_arc != nullptr && knob_set != nullptr) {
        lv_arc_align_obj_to_angle(set_arc, knob_set, 0);
        lv_obj_clear_flag(knob_set, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(knob_set);
    }

    if (!temp_ok) {
        if (delta_arc != nullptr) lv_arc_set_angles(delta_arc, 0, 0);
        if (knob_cur != nullptr) lv_obj_add_flag(knob_cur, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    p4_ui_arc_set_delta_overlay(delta_arc, pct_t, pct_sp);
    // Bind current pip to cur_arc (same 440 coolroom geometry as set_arc).
    p4_ui_place_dot_on_arc_(cur_arc, knob_cur, pct_t);
}

// ─── Header clock / date ───────────────────────────────────────────────────

/// Header clock, 12-hour with seconds (e.g. "01:05:09 PM").
/// Falls back to the uptime string until the wall clock is set.
inline std::string p4_ui_fmt_clock_12h() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    char buf[24];
    if (tv.tv_sec < 86400L) {
        p4_fmt_time(buf, sizeof(buf));
        return std::string(buf);
    }
    struct tm t{};
    localtime_r(&tv.tv_sec, &t);
    strftime(buf, sizeof(buf), "%I:%M:%S %p", &t);
    return std::string(buf);
}

/// Header date as day + short month, no leading zero (e.g. "10 Apr").
inline std::string p4_ui_fmt_date_short() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 86400L) return std::string("--");
    struct tm t{};
    localtime_r(&tv.tv_sec, &t);
    char mon[8];
    strftime(mon, sizeof(mon), "%b", &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, mon);
    return std::string(buf);
}

// ─── Door reed polarity ────────────────────────────────────────────────────

/// True when the cabinet door is open given NC/NO mode and the raw reed level.
inline bool p4_door_is_open(bool nc_mode, bool reed_level) {
    return nc_mode ? reed_level : !reed_level;
}

// ─── Alarm / FX predicates ─────────────────────────────────────────────────

/// Any alarm or probe-fault condition active (home bell, snow/flame suppress).
inline bool p4_ui_any_alarm(bool hi, bool lo, bool door, bool no_cool, bool ice, bool probe) {
    return hi || lo || door || no_cool || ice || probe;
}

/// Banner conditions (probe fault has its own status path — not listed here).
inline bool p4_ui_any_banner_alarm(bool hi, bool lo, bool door, bool no_cool, bool ice) {
    return hi || lo || door || no_cool || ice;
}

// ─── Home centre status (rotating multi-fault) ─────────────────────────────

/// Inputs for the home-gauge centre status line. Hardware offline bits are only
/// raised when that path is actually in use (RTD expected / SHT enabled).
struct P4UiHomeStatusIn {
    bool     relay_ok;
    bool     rtd_ok;
    bool     rtd_expected;
    bool     sht31_enabled;
    bool     sht31_ok;
    bool     sht20_enabled;
    bool     sht20_ok;
    bool     probe_fault;
    bool     alarm_hi;
    bool     alarm_lo;
    bool     alarm_door;
    bool     alarm_no_cool;
    bool     alarm_ice;
    bool     defrost;
    bool     compressor_on;
    bool     lockout;
    uint32_t rotate_period_ms;  // 0 → 2500 ms
};

inline size_t p4_ui_home_status_collect_(const P4UiHomeStatusIn &in,
                                         const char **out, size_t cap) {
    size_t n = 0;
    auto push = [&](const char *s) {
        if (n < cap) out[n++] = s;
    };
    if (!in.relay_ok)                              push("RELAY BOARD OFFLINE");
    if (in.rtd_expected && !in.rtd_ok)              push("TEMP BOARD OFFLINE");
    if (in.sht31_enabled && !in.sht31_ok)           push("HUMIDITY SENSOR OFFLINE");
    if (in.sht20_enabled && !in.sht20_ok)           push("AMBIENT SENSOR OFFLINE");
    if (in.probe_fault)                            push("COOLROOM PROBE BAD");
    if (in.alarm_hi)                               push("HIGH TEMP");
    if (in.alarm_lo)                               push("LOW TEMP");
    if (in.alarm_door)                             push("DOOR OPEN");
    if (in.alarm_no_cool)                          push("NOT COOLING");
    if (in.alarm_ice)                              push("ICE ON COIL");
    return n;
}

/// True when at least one fault/offline/alarm slot is active (centre text red).
inline bool p4_ui_home_status_is_fault(const P4UiHomeStatusIn &in) {
    const char *slots[12];
    return p4_ui_home_status_collect_(in, slots, 12) > 0;
}

/// Centre status string. With multiple faults, advances rotate_idx every
/// rotate_period_ms so the operator sees each one in turn. With none, falls
/// through to DEFROST / COOLING / LOCKOUT / OK.
inline std::string p4_ui_home_status_text(const P4UiHomeStatusIn &in,
                                          uint32_t &rotate_idx,
                                          uint32_t &rotate_last_ms) {
    const char *slots[12];
    const size_t n = p4_ui_home_status_collect_(in, slots, 12);
    if (n == 0) {
        rotate_idx = 0;
        rotate_last_ms = 0;
        if (in.defrost) return std::string("DEFROST");
        if (in.compressor_on) return std::string("COOLING");
        if (in.lockout) return std::string("LOCKOUT");
        return std::string("OK");
    }
    const uint32_t now = millis();
    const uint32_t period = in.rotate_period_ms ? in.rotate_period_ms : 2500U;
    if (rotate_last_ms == 0U) {
        rotate_last_ms = now;
        if (rotate_idx >= n) rotate_idx = 0;
    } else if ((now - rotate_last_ms) >= period) {
        rotate_last_ms = now;
        rotate_idx = (rotate_idx + 1U) % static_cast<uint32_t>(n);
    }
    if (rotate_idx >= n) rotate_idx = 0;
    return std::string(slots[rotate_idx]);
}

// ─── Left-rail icon animation (100 ms tick on Home; skipped off-Home) ──────
// YAML interval is 100 ms. Motion coefficients are 2× the original 50 ms
// values so visual spin/flicker speed stays the same at half the wakeups.

/// Background FX: falling snow while compressor runs (suppressed on alarm/defrost).
inline bool p4_ui_fx_snow_visible(bool compressor_on, bool defrost_active, bool any_alarm) {
    return compressor_on && !defrost_active && !any_alarm;
}

/// Background FX: defrost border flicker (suppressed on alarm).
inline bool p4_ui_fx_flame_visible(bool defrost_active, bool any_alarm) {
    return defrost_active && !any_alarm;
}

struct P4UiHomeIconAnimIn {
    bool compressor_on;
    bool defrost_active;
    bool light_on;
    bool any_alarm;
    bool alarm_silenced;
    lv_color_t col_blue;
    lv_color_t col_red;
    lv_color_t col_grey;
    lv_color_t col_orange;
};

inline void p4_ui_spin_pivot_(lv_obj_t *obj) {
    lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), 0);
}

/// Drive snowflake / flame / light / bell motion for one 100 ms frame.
inline void p4_ui_home_icon_anim_tick(uint32_t tick, lv_obj_t *compressor, lv_obj_t *defrost,
                                      lv_obj_t *light, lv_obj_t *alarm,
                                      const P4UiHomeIconAnimIn &in) {
    // Snowflake (compressor): slow continuous spin + gentle flicker while ON.
    if (compressor != nullptr) {
        if (in.compressor_on) {
            p4_ui_spin_pivot_(compressor);
            lv_obj_set_style_transform_rotation(compressor, (int32_t)((tick * 44) % 3600), 0);
            float flicker = sinf(tick * 0.6f) * 0.5f + 0.5f;
            lv_obj_set_style_opa(compressor, (lv_opa_t)(160 + flicker * 95), 0);
        } else {
            lv_obj_set_style_transform_rotation(compressor, 0, 0);
            lv_obj_set_style_opa(compressor, 255, 0);
        }
    }

    // Flame (defrost): irregular dual-sine flicker while control tick says active.
    if (defrost != nullptr) {
        if (in.defrost_active) {
            float flicker = sinf(tick * 1.0f) * 0.3f + sinf(tick * 2.6f + 1.0f) * 0.2f + 0.5f;
            int32_t opa = (int32_t)(140 + flicker * 115);
            if (opa < 100) opa = 100;
            if (opa > 255) opa = 255;
            lv_obj_set_style_opa(defrost, (lv_opa_t)opa, 0);
            lv_obj_set_style_text_color(defrost, in.col_red, 0);
        } else {
            lv_obj_set_style_opa(defrost, 255, 0);
            lv_obj_set_style_text_color(defrost, in.col_grey, 0);
        }
    }

    // Light globe: slow breathing glow while relay is on.
    if (light != nullptr) {
        if (in.light_on) {
            float glow = sinf(tick * 0.3f) * 0.5f + 0.5f;
            lv_obj_set_style_opa(light, (lv_opa_t)(90 + glow * 165), 0);
            lv_obj_set_style_text_color(light, in.col_orange, 0);
        } else {
            lv_obj_set_style_opa(light, 255, 0);
            lv_obj_set_style_text_color(light, in.col_grey, 0);
        }
    }

    // Bell: red while any alarm; jiggle until soft-muted.
    if (alarm != nullptr) {
        if (in.any_alarm) {
            lv_obj_set_style_text_color(alarm, in.col_red, 0);
            if (!in.alarm_silenced) {
                p4_ui_spin_pivot_(alarm);
                float jiggle = sinf(tick * 2.6f) * 150.0f;  // ±15 degrees
                lv_obj_set_style_transform_rotation(alarm, (int32_t)jiggle, 0);
            } else {
                lv_obj_set_style_transform_rotation(alarm, 0, 0);
            }
        } else {
            lv_obj_set_style_text_color(alarm, in.col_grey, 0);
            lv_obj_set_style_transform_rotation(alarm, 0, 0);
        }
    }
}
