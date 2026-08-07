#pragma once
// ============================================================================
// p4_ui_second_tick.h — 1 s UI orchestrator (elevated from YAML).
//
// Must only be #included via ESPHome `esphome: includes:` into main.cpp AFTER
// entity / LVGL object pointer declarations. Uses those file-scope names
// directly (same as ESPHome id() rewrite).
//
// Runs on loopTask (CPU 1) only — may call LVGL. Control tick / SD heavy work
// lives on CPU 0 via p4_ctl_tick.h; this file must not start that work.
//
// Option C: page-gate expensive Home LVGL work when page_home_active.
// Info diagnostics: page-gated on page_info_active (full_refresh:true means
// dirtying hidden Info labels still forces a whole 1024×600 frame — that
// starved Settings touch responsiveness after the always-on Info fix).
// switch_to_page_info one-shots this tick for instant paint.
// Always-on (no LVGL): Wi-Fi poll, HA drop, Status text_sensor, system_time.
// NVS sync: skip while Settings is active (persist_config_to_nvs already
// stages+syncs after each change; periodic flash sync hitching C1 hurt UX).
//
// Do NOT early-return on !home2_view_active: that gate must stay nested under
// the Home block. A top-level return (B/A extraction bug) skipped Info updates.
// ============================================================================

#include <cstdio>
#include <cmath>
#include <string>
#include <lvgl.h>
#include "p4_ui.h"
#include "p4_helpers.h"
#include "p4_logging.h"
#include "p4_wifi.h"

inline void p4_ui_second_tick(float dial_min_c, float dial_max_c) {
  const bool on_home = page_home_active->value();
  const bool on_info = page_info_active->value();
  const bool on_settings = page_settings_active->value();

  // Flash sync blocks loopTask; Settings steppers already persist on change.
  if (!on_settings && esphome::global_preferences != nullptr)
    esphome::global_preferences->sync();

  p4_wifi::poll();

  if (!input_ha_api_enabled->value()) p4_ha_api_drop_clients();

  // Status string is shared with web/HA — compute every tick; LVGL paint only on home.
  float remaining_s = p4_ui_comp_lockout_remaining_s(
      ctl_comp_last_off_ms->value(), ctl_comp_lockout_min->value());
  const bool rtd_expected = true;
  P4UiHomeStatusIn st_in{};
  st_in.relay_ok = hw_rs485_relay_ok->value();
  st_in.rtd_ok = hw_rs485_rtd1_ok->value();
  st_in.rtd_expected = rtd_expected;
  st_in.sht31_enabled = input_humidity_internal_enabled->value();
  st_in.sht31_ok = p4_i2c_hum_online(
      st_in.sht31_enabled,
      i2c_sht31->is_failed(),
      i2c_sht31->status_has_warning(),
      sht31_internal_temp_raw->state);
  st_in.sht20_enabled = input_humidity_external_enabled->value();
  st_in.sht20_ok = p4_i2c_hum_online(
      st_in.sht20_enabled,
      i2c_sht20->is_failed(),
      i2c_sht20->status_has_warning(),
      sht20_external_temp_raw->state);
  st_in.probe_fault = ctl_probe_fault->value();
  st_in.alarm_hi = ctl_alarm_high_active->value();
  st_in.alarm_lo = ctl_alarm_low_active->value();
  st_in.alarm_door = ctl_door_alarm_active->value();
  st_in.alarm_no_cool = ctl_no_cool_alarm_active->value();
  st_in.alarm_ice = ctl_ice_alarm_active->value();
  st_in.alarm_ct_fail = ctl_ct_fail_to_start_active->value();
  st_in.alarm_ct_stuck = ctl_ct_stuck_on_active->value();
  st_in.alarm_ct_over = ctl_ct_overcurrent_active->value();
  st_in.defrost = ctl_defrost_active->value() || ctl_defrost_dripping->value();
  st_in.compressor_on = relay_compressor->state;
  st_in.lockout = remaining_s > 0.0f;
  st_in.rotate_period_ms = 2500;
  std::string st = p4_ui_home_status_text(
      st_in, ui_status_rotate_idx->value(), ui_status_rotate_ms->value());
  if (status_text->state != st) status_text->publish_state(st);

  system_time_valid->publish_state(p4_wall_clock_ok());

  // ESPHome htu21d/sht3xd leave sticky last readings on I2C fail — wipe raw
  // NaN when disabled or component unhealthy so online/UI cannot stay true.
  if (p4_i2c_hum_should_clear(input_humidity_internal_enabled->value(),
                              i2c_sht31->is_failed(),
                              i2c_sht31->status_has_warning())) {
    if (std::isfinite(sht31_internal_temp_raw->state))
      sht31_internal_temp_raw->publish_state(NAN);
    if (std::isfinite(sht31_internal_humidity_raw->state))
      sht31_internal_humidity_raw->publish_state(NAN);
  }
  if (p4_i2c_hum_should_clear(input_humidity_external_enabled->value(),
                              i2c_sht20->is_failed(),
                              i2c_sht20->status_has_warning())) {
    if (std::isfinite(sht20_external_temp_raw->state))
      sht20_external_temp_raw->publish_state(NAN);
    if (std::isfinite(sht20_external_humidity_raw->state))
      sht20_external_humidity_raw->publish_state(NAN);
  }

  if (on_home) {
    lv_label_set_text(lbl_date, (p4_ui_fmt_date_short()).c_str());
    lv_obj_send_event(lbl_date, lvgl::lv_update_event, nullptr);

    lv_label_set_text(lbl_clock, (p4_ui_fmt_clock_12h()).c_str());
    lv_obj_send_event(lbl_clock, lvgl::lv_update_event, nullptr);

    lv_label_set_text(lbl_wifi_hdr, ([]() -> std::string {
      if (!ctl_wifi_connected->value()) return std::string("\U000F092D");
      float rssi = wifi_rssi->state;
      if (isnan(rssi)) return std::string("\U000F0924");
      if (rssi >= -55) return std::string("\U000F0928");
      if (rssi >= -65) return std::string("\U000F0925");
      if (rssi >= -75) return std::string("\U000F0922");
      return std::string("\U000F091F");
    }()).c_str());
    lv_obj_send_event(lbl_wifi_hdr, lvgl::lv_update_event, nullptr);

    lv_obj_set_style_text_color(lbl_wifi_hdr, []() -> lv_color_t {
      if (!ctl_wifi_connected->value()) return col_grey;
      float rssi = wifi_rssi->state;
      if (!isnan(rssi) && rssi >= -65) return col_cyan;
      if (!isnan(rssi) && rssi >= -75) return col_orange;
      return col_subtext;
    }(), LV_PART_MAIN);

    lv_label_set_text(lbl_power_info, ([]() -> std::string {
      char buf[24];
      snprintf(buf, sizeof(buf), "Heap: %.0f KB", p4_free_heap_kb());
      return std::string(buf);
    }()).c_str());
    lv_obj_send_event(lbl_power_info, lvgl::lv_update_event, nullptr);

    // Home rail countdowns: compute from millis() every 1 s tick — do not
    // read the 3 s published sensors (comp_lockout_remaining_sec /
    // defrost_countdown_sec) or the amber/cyan labels would stutter.
    lv_label_set_text(lbl_home_lockout,
                      p4_ui_fmt_countdown_s(p4_ui_comp_lockout_remaining_s(
                          ctl_comp_last_off_ms->value(),
                          ctl_comp_lockout_min->value())).c_str());
    lv_obj_send_event(lbl_home_lockout, lvgl::lv_update_event, nullptr);

    lv_label_set_text(lbl_home_next_defrost,
                      p4_ui_fmt_countdown_s(p4_ui_next_defrost_remaining_s(
                          ctl_defrost_active->value(), ctl_defrost_dripping->value(),
                          ctl_defrost_last_end_ms->value(),
                          ctl_defrost_interval_min->value())).c_str());
    lv_obj_send_event(lbl_home_next_defrost, lvgl::lv_update_event, nullptr);

    lv_obj_set_style_text_color(ui_compressor_icon, []() -> lv_color_t {
      if (relay_compressor->state) return col_blue;
      if (!hw_rs485_relay_ok->value() || !ctl_relay_compressor_enabled->value())
        return col_red;
      float rem = p4_ui_comp_lockout_remaining_s(
          ctl_comp_last_off_ms->value(), ctl_comp_lockout_min->value());
      if (rem > 0.0f) return col_orange;
      return col_grey;
    }(), LV_PART_MAIN);

    const bool faultish = p4_ui_home_status_is_fault(st_in);
    lv_color_t col = col_green;
    if (faultish) col = col_red;
    else if (st_in.defrost || st_in.lockout) col = col_orange;
    else if (st_in.compressor_on) col = col_blue;
    lv_label_set_text(lbl_status_text, st.c_str());
    lv_label_set_text(lbl_status_text_v2, st.c_str());
    lv_obj_set_style_text_color(lbl_status_text, col, 0);
    lv_obj_set_style_text_color(lbl_status_text_v2, col, 0);

    lv_label_set_text(lbl_setpoint_status, ([]() -> std::string {
      const bool f = sel_temperature_display_unit->current_option() == "Fahrenheit";
      return "Set: " + p4_fmt_temp(ctl_setpoint->value(), f);
    }()).c_str());
    lv_obj_send_event(lbl_setpoint_status, lvgl::lv_update_event, nullptr);

    lv_label_set_text(lbl_setpoint_status_v2, ([]() -> std::string {
      const bool f = sel_temperature_display_unit->current_option() == "Fahrenheit";
      return "Set: " + p4_fmt_temp(ctl_setpoint->value(), f);
    }()).c_str());
    lv_obj_send_event(lbl_setpoint_status_v2, lvgl::lv_update_event, nullptr);

    lv_arc_set_value(home_setpoint_arc,
                     static_cast<int>(p4_ui_setpoint_to_arc_pct(
                         ctl_setpoint->value(), dial_min_c, dial_max_c)));
    lv_obj_send_event(home_setpoint_arc, lvgl::lv_update_event, nullptr);

    if (home2_view_active->value()) {
      p4_ui_update_home2_cooling(
          home2_set_arc, home2_temp_arc, home2_temp_delta_arc,
          home2_knob_set, home2_knob_cur,
          probe1_temp->state, ctl_setpoint->value(), dial_min_c, dial_max_c,
          home2_ambient_arc);
    }

    if (static_cast<bool>(!p4_ui_any_banner_alarm(
            ctl_alarm_high_active->value(), ctl_alarm_low_active->value(),
            ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(),
            ctl_ice_alarm_active->value(),
            ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(),
            ctl_ct_overcurrent_active->value()))) {
      lv_obj_add_flag(lbl_home_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(lbl_home_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(lbl_home_alarm_banner, ([]() -> std::string {
      std::string s;
      if (ctl_alarm_high_active->value()) s += "HIGH TEMPERATURE   ";
      if (ctl_alarm_low_active->value()) s += "LOW TEMPERATURE   ";
      if (ctl_door_alarm_active->value()) s += "DOOR OPEN   ";
      if (ctl_no_cool_alarm_active->value()) s += "NO COOLING   ";
      if (ctl_ice_alarm_active->value()) s += "ICE DETECTED   ";
      if (ctl_ct_fail_to_start_active->value()) s += "COMP FAIL TO START   ";
      if (ctl_ct_stuck_on_active->value()) s += "COMP STUCK ON   ";
      if (ctl_ct_overcurrent_active->value()) s += "CT OVERCURRENT   ";
      return s;
    }()).c_str());
    lv_obj_send_event(lbl_home_alarm_banner, lvgl::lv_update_event, nullptr);

    const bool motion_suppressed = p4_ui_any_alarm(ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(), ctl_ice_alarm_active->value(), ctl_probe_fault->value(), ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(), ctl_ct_overcurrent_active->value());
    const bool snow_active =
        !motion_suppressed && relay_compressor->state && !ctl_defrost_active->value();
    const bool flame_active = !motion_suppressed && ctl_defrost_active->value();
    if (snow_active || flame_active) {
      fx_anim_tick->value() = (fx_anim_tick->value() + 1U) % 240U;
    } else {
      fx_anim_tick->value() = 0U;
    }

    lv_obj_set_style_border_color(
        bg_fx_defrost_border,
        (fx_anim_tick->value() % 2U == 0U) ? col_orange : lv_color_hex(0xFFCF33),
        LV_PART_MAIN);
    lv_obj_set_style_border_opa(bg_fx_defrost_border, []() -> lv_opa_t {
      if (!p4_ui_fx_flame_visible(
              ctl_defrost_active->value(),
              p4_ui_any_alarm(ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(), ctl_ice_alarm_active->value(), ctl_probe_fault->value(), ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(), ctl_ct_overcurrent_active->value())))
        return 0;
      switch (fx_anim_tick->value() % 4U) {
        case 0: return 55;
        case 1: return 90;
        case 2: return 70;
        default: return 100;
      }
    }(), LV_PART_MAIN);
    lv_obj_set_style_border_width(bg_fx_defrost_border, []() -> int {
      if (!p4_ui_fx_flame_visible(
              ctl_defrost_active->value(),
              p4_ui_any_alarm(ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(), ctl_ice_alarm_active->value(), ctl_probe_fault->value(), ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(), ctl_ct_overcurrent_active->value())))
        return 0;
      switch (fx_anim_tick->value() % 4U) {
        case 0: return 3;
        case 1: return 6;
        case 2: return 4;
        default: return 7;
      }
    }(), LV_PART_MAIN);
    if (static_cast<bool>(!p4_ui_fx_flame_visible(
            ctl_defrost_active->value(),
            p4_ui_any_alarm(ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(), ctl_ice_alarm_active->value(), ctl_probe_fault->value(), ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(), ctl_ct_overcurrent_active->value())))) {
      lv_obj_add_flag(bg_fx_defrost_border, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(bg_fx_defrost_border, LV_OBJ_FLAG_HIDDEN);
    }

    auto snow_vis = []() -> bool {
      return p4_ui_fx_snow_visible(
          relay_compressor->state, ctl_defrost_active->value(),
          p4_ui_any_alarm(ctl_alarm_high_active->value(), ctl_alarm_low_active->value(), ctl_door_alarm_active->value(), ctl_no_cool_alarm_active->value(), ctl_ice_alarm_active->value(), ctl_probe_fault->value(), ctl_ct_fail_to_start_active->value(), ctl_ct_stuck_on_active->value(), ctl_ct_overcurrent_active->value()));
    };

    lv_obj_set_style_x(bg_fx_snow_1,
                       static_cast<lv_coord_t>(120 + (int)((fx_anim_tick->value() * 7U + 11U) % 760U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_1,
                       static_cast<lv_coord_t>(60 + (int)((fx_anim_tick->value() * 13U + 19U) % 470U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_1, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_1, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_x(bg_fx_snow_2,
                       static_cast<lv_coord_t>(250 + (int)((fx_anim_tick->value() * 9U + 29U) % 660U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_2,
                       static_cast<lv_coord_t>(80 + (int)((fx_anim_tick->value() * 11U + 37U) % 450U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_2, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_2, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_x(bg_fx_snow_3,
                       static_cast<lv_coord_t>(390 + (int)((fx_anim_tick->value() * 5U + 53U) % 560U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_3,
                       static_cast<lv_coord_t>(100 + (int)((fx_anim_tick->value() * 15U + 71U) % 430U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_3, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_3, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_x(bg_fx_snow_4,
                       static_cast<lv_coord_t>(530 + (int)((fx_anim_tick->value() * 8U + 97U) % 460U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_4,
                       static_cast<lv_coord_t>(70 + (int)((fx_anim_tick->value() * 17U + 23U) % 460U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_4, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_4, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_x(bg_fx_snow_5,
                       static_cast<lv_coord_t>(690 + (int)((fx_anim_tick->value() * 6U + 131U) % 360U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_5,
                       static_cast<lv_coord_t>(90 + (int)((fx_anim_tick->value() * 12U + 41U) % 440U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_5, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_5, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_x(bg_fx_snow_6,
                       static_cast<lv_coord_t>(840 + (int)((fx_anim_tick->value() * 10U + 173U) % 220U)),
                       LV_PART_MAIN);
    lv_obj_set_style_y(bg_fx_snow_6,
                       static_cast<lv_coord_t>(110 + (int)((fx_anim_tick->value() * 14U + 59U) % 420U)),
                       LV_PART_MAIN);
    if (!snow_vis()) lv_obj_add_flag(bg_fx_snow_6, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(bg_fx_snow_6, LV_OBJ_FLAG_HIDDEN);

    // Hidden diagnostic LEDs live on page_home.
    lv_led_set_brightness(
        led_rs485,
        static_cast<float>((hw_rs485_relay_ok->value() && hw_rs485_rtd1_ok->value()) ? 100.0f
                                                                                     : 0.0f));
    lv_led_set_brightness(led_rtc, static_cast<float>(p4_wall_clock_ok() ? 100.0f : 0.0f));
  }

  // Info page diagnostics — only while Info is visible. With full_refresh,
  // updating these labels off-page still forces a whole-frame redraw.
  if (!on_info) return;

  lv_label_set_text(lbl_lockout_timer_display, ([]() -> std::string {
    float rem = p4_ui_comp_lockout_remaining_s(
        ctl_comp_last_off_ms->value(), ctl_comp_lockout_min->value());
    if (rem <= 0.0f) return std::string("Ready");
    int total_s = (int) rem;
    char buf[16];
    snprintf(buf, sizeof(buf), "%dm %02ds", total_s / 60, total_s % 60);
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_lockout_timer_display, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_heap, ([]() -> std::string {
    char buf[64];
    p4_fmt_heap_mb(buf, sizeof(buf));
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_heap, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_cpu, ([]() -> std::string {
    char buf[32];
    p4_fmt_cpu_usage(buf, sizeof(buf));
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_cpu, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_psram, ([]() -> std::string {
    char buf[64];
    p4_fmt_psram_mb(buf, sizeof(buf));
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_psram, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_uptime, ([]() -> std::string {
    uint32_t uptime_s = millis() / 1000;
    uint32_t days = uptime_s / 86400;
    uint32_t hours = (uptime_s % 86400) / 3600;
    uint32_t mins = (uptime_s % 3600) / 60;
    char buf[48];
    snprintf(buf, sizeof(buf), "Uptime: %lu d %02lu h %02lu m",
             (unsigned long) days, (unsigned long) hours, (unsigned long) mins);
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_uptime, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_signal, ([]() -> std::string {
    char buf[48];
    int rssi = wifi_rssi->state;
    snprintf(buf, sizeof(buf), "Signal: %d dBm", rssi);
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_signal, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_ssid, ([]() -> std::string {
    return "SSID: " + wifi_ssid_text->state;
  }()).c_str());
  lv_obj_send_event(lbl_info_ssid, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_ip, ([]() -> std::string {
    return "IP: " + ip_address->state;
  }()).c_str());
  lv_obj_send_event(lbl_info_ip, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_rs485, ([]() -> std::string {
    char buf[64];
    bool relay_ok = hw_rs485_relay_ok->value();
    bool rtd_ok = hw_rs485_rtd1_ok->value();
    snprintf(buf, sizeof(buf), "Relay:%s RTD:%s",
             relay_ok ? "OK" : "NO", rtd_ok ? "OK" : "NO");
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_rs485, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_probes, ([]() -> std::string {
    char buf[80];
    bool rtd_ok = hw_rs485_rtd1_ok->value();
    bool sht31_ok = p4_i2c_hum_online(
        input_humidity_internal_enabled->value(),
        i2c_sht31->is_failed(),
        i2c_sht31->status_has_warning(),
        sht31_internal_temp_raw->state);
    bool sht20_ok = p4_i2c_hum_online(
        input_humidity_external_enabled->value(),
        i2c_sht20->is_failed(),
        i2c_sht20->status_has_warning(),
        sht20_external_temp_raw->state);
    snprintf(buf, sizeof(buf), "RTD:%s SHT31:%s SHT20:%s Fault:%s",
             rtd_ok ? "OK" : "NO", sht31_ok ? "OK" : "NO", sht20_ok ? "OK" : "NO",
             ctl_probe_fault->value() ? "YES" : "no");
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_probes, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_sd, ([]() -> std::string {
    if (!sd_card_ok->value()) return std::string("SD: not mounted");
    char buf[80];
    float free_mb = sd_free_mb->state;
    float total_mb = p4_sd_total_mb();
    if (total_mb > 0)
      snprintf(buf, sizeof(buf), "SD: mounted  %.0f MB total", total_mb);
    else
      snprintf(buf, sizeof(buf), "SD: mounted  %.0f MB free", free_mb);
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_sd, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_sd_detail, ([]() -> std::string {
    if (!sd_card_ok->value()) return std::string("Logs/files: use web SD tab");
    char buf[96];
    const float total_mb = p4_sd_total_mb();
    const float free_mb = sd_free_mb->state;
    if (total_mb > 0 && free_mb >= 0) {
      const float used_mb = total_mb - free_mb;
      snprintf(buf, sizeof(buf),
               "Used %.0f MB (%.2f%%)  Free %.0f MB (%.0f%%)  backup: %s",
               used_mb, used_mb * 100.0f / total_mb, free_mb,
               free_mb * 100.0f / total_mb, p4_sd_backup_present() ? "yes" : "no");
    } else {
      snprintf(buf, sizeof(buf), "Free %.0f MB  backup: %s", free_mb,
               p4_sd_backup_present() ? "yes" : "no");
    }
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_sd_detail, lvgl::lv_update_event, nullptr);

  lv_label_set_text(lbl_info_rtc, ([]() -> std::string {
    if (!p4_wall_clock_ok())
      return std::string("Time: pending — waiting for first NTP sync");
    const int32_t age_s = p4_ntp_synced() ? p4_ntp_sync_age_s() : -1;
    if (age_s < 0)
      return std::string("Time: valid — LP RTC held, no NTP sync yet");
    char buf[64];
    if (age_s < 3600)
      snprintf(buf, sizeof(buf), "Time: valid — NTP synced %d min ago", (int)(age_s / 60));
    else
      snprintf(buf, sizeof(buf), "Time: valid — NTP synced %.1f h ago", age_s / 3600.0f);
    return std::string(buf);
  }()).c_str());
  lv_obj_send_event(lbl_info_rtc, lvgl::lv_update_event, nullptr);
}
