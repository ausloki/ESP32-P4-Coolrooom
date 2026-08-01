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

// ─── Left-rail icon animation (50 ms tick) ─────────────────────────────────

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

/// Drive snowflake / flame / light / bell motion for one 50 ms frame.
/// Behavior matches the former inline YAML lambda exactly.
inline void p4_ui_home_icon_anim_tick(uint32_t tick, lv_obj_t *compressor, lv_obj_t *defrost,
                                      lv_obj_t *light, lv_obj_t *alarm,
                                      const P4UiHomeIconAnimIn &in) {
    // Snowflake (compressor): slow continuous spin + gentle flicker while ON.
    if (compressor != nullptr) {
        if (in.compressor_on) {
            p4_ui_spin_pivot_(compressor);
            lv_obj_set_style_transform_rotation(compressor, (int32_t)((tick * 22) % 3600), 0);
            float flicker = sinf(tick * 0.3f) * 0.5f + 0.5f;
            lv_obj_set_style_opa(compressor, (lv_opa_t)(160 + flicker * 95), 0);
        } else {
            lv_obj_set_style_transform_rotation(compressor, 0, 0);
            lv_obj_set_style_opa(compressor, 255, 0);
        }
    }

    // Flame (defrost): irregular dual-sine flicker while control tick says active.
    if (defrost != nullptr) {
        if (in.defrost_active) {
            float flicker = sinf(tick * 0.5f) * 0.3f + sinf(tick * 1.3f + 1.0f) * 0.2f + 0.5f;
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
            float glow = sinf(tick * 0.15f) * 0.5f + 0.5f;
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
                float jiggle = sinf(tick * 1.3f) * 150.0f;  // ±15 degrees
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
