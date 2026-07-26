# Active Context — Current Session State

**Date:** 2026-07-26
**Session:** Resolved 5 items from the outstanding-items list — 2 closed by user decision (no
code), 3 implemented: door-triggered light as its own feature (decoupling a bug), a second bug
fix found in the same code (door-alarm master switch didn't gate anything at runtime), all alarm
types now push to ntfy, and one entity renamed to remove a reserved URL character.
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User reviewed the previous "what's outstanding" list and gave 5 explicit instructions:
  (1) drop the 4 Carel-comparison divergences — current behavior is as wanted; (2) the door
  NC/NO switch's light-toggle coupling should become a proper "light enable" option linked
  to the door relay, not just removed; (3) fix the `Defrost Drip/Drain Phase` entity-name
  deprecation warning; (4) all alarm types should push to ntfy, not just high/low/probe-fault;
  (5) drop the "real RBAC" outstanding item — the current two-tier guest/operator model
  (main display visible without login, separate login elevates to settings) is exactly what's
  wanted, permanently, not a stopgap.
- **Found a second real bug while implementing item 2**: `input_door_sensor_enabled` (the
  "Door Sensor Enabled" master switch, previously documented as gating the door-open alarm)
  was read in backup/restore and the toggle UI, but **never actually checked in the control
  tick** — the door-open alarm timer ran unconditionally regardless of this switch's state.
  Fixed as a direct, well-scoped extension of the work already touching this exact code.

### Latest Completed Work

- **Items closed by decision, no code**: the four Carel-comparison divergences (symmetric vs.
  asymmetric hysteresis band, no min compressor ON-time, no fan-control confirmation, door
  switch not pausing compressor/alarm suppression) and the RBAC model (confirmed the existing
  two-tier design is permanent — already matched what the user described exactly, so nothing
  to change, only closed out the "revisit if needed" framing in `USER_MANUAL.md` §4.11).
- **Door-triggered light — new independent feature**: old `door_sensor_mode_light_control`
  switch (which set NC/NO wiring mode *and* toggled `relay_light` as a side effect) renamed to
  `door_sensor_mode_nc_no`, light-toggle stripped entirely. New `sw_door_light_enabled` switch
  (backed by `input_door_light_enabled`, default **on** to preserve prior unconditional
  behavior) — `door_reed_sensor`'s `on_state` now turns the light on door-open/off door-close
  only when this switch is on, fully independent of the door-open alarm switch.
- **Door-alarm-enable bug fix** (found, not requested, fixed as a direct extension): the reed
  sensor's on_state now only starts the door-open timer while `input_door_sensor_enabled` is
  true; the control tick's alarm-activation check re-verifies the switch as a defensive second
  gate; `sw_door_sensor_enabled`'s turn-off action now clears any in-progress open-timer/alarm
  state so a stale timestamp can't fire an alarm later after re-enabling.
- New `input_door_light_enabled` threaded through `p4_sd_backup_params()`/
  `p4_sd_restore_params()` (`p4_logging.h`, new parameter on both, seeded from caller's current
  value like other post-hoc settings so older `backup.json` files stay restorable) and all 3
  yaml call sites; added as a 4th row on the LVGL Door page (`page_settings_6`) and to
  `refresh_all_settings_labels`; added to both web dashboard files with updated help text
  clarifying the alarm-enable and light-enable switches are independent.
- **All alarm types now push to ntfy**: three new scripts (`ntfy_door_alarm_request`,
  `ntfy_no_cool_alarm_request` — urgent priority, `ntfy_ice_alarm_request`) matching the
  existing high/low/probe-fault pattern exactly (timestamp, matching Title/Priority/Tags
  style), all reusing the existing generic `ntfy_alarm_clear_request` on recovery — no new
  "cleared" message types needed. Three new edge-detection globals extend the existing
  WiFi-gated step-9 block; all reset in the offline branch so they still notify on reconnect.
  Also fixed a stale comment claiming "auto-remount isn't implemented" (it was, last session).
- **Entity rename**: `sw_defrost_drip`'s `name:` changed `"Defrost Drip/Drain Phase"` →
  `"Defrost Drip-Drain Phase"` — removes the reserved `/` (ESPHome warns now, hard-errors in
  2026.7.0). Updated everywhere the label appeared (LVGL page, both dashboard files).
  Confirmed the warning is gone from the build output.
- **Documentation updated per the new closeout rule** (`CLAUDE.md`, added last session — this
  is its first real test against actual code changes): `USER_MANUAL.md` §4.6 (removed the
  now-fixed "known quirk" callout, added the light-enable row + structogram, clarified Door
  Sensor Enabled's scope), §4.2 (renamed row), §4.8 (door/no-cool/ice added to ntfy table,
  removed the stale "not sent as push" caveat), §4.11 (added a permanent-design statement);
  `QUICK_START_GUIDE.md` (alerts table + plums worked example both updated).

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   21.4% (123,228 / 576,464 bytes)
Flash: 21.6% (1,583,256 / 7,340,032 bytes)
```

Only pre-existing, unrelated warnings remain (SD log manager `opendir`/`readdir`/`closedir`
linker warnings; ESP-IDF's own `periph_module_reset`/literal-suffix framework warnings).

### Immediate Next Actions

1. Reflash the physical device once connected — standing hardware blocker, unchanged.
2. Hardware validation, once connected, should now also cover: door-triggered light actually
   toggling on open/close when enabled and staying off when disabled; the door-alarm switch
   genuinely doing nothing when off (the fixed bug); all three new ntfy push types actually
   arriving for a real door/no-cool/ice condition.
3. Everything else in the hardware-validation queue from prior sessions remains open and
   unchanged by this session (see Outstanding Items).

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: this session's door-light + alarm-gating + ntfy
   additions (all new/untested), the LVGL 7-page settings flow + PIN gate + SD auto-remount
   from 2 sessions ago (still untested), offline-safe/SD-card behavior, RTC identity (0x51 vs
   0x68), SHT31/SHT20 sensors, the web dashboard status indicators + alarm banners, the
   defrost-on-reboot/pulldown-aware-alarm-grace fixes, the SD log manager + WiFi reconnect,
   the live countdown timers, dew-point defrost + calibration offsets, the defrost-flag
   switches, and the full event-logging + SD-failure-detection additions. All blocked.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding.
4. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — likely benign,
   unconfirmed without hardware.
5. Both new manuals (`USER_MANUAL.md`, `QUICK_START_GUIDE.md`) and the LVGL mockup artifact
   are screenshot-placeholder-only pending hardware connection.

**Resolved this session (removed from the list)**: the four Carel-comparison divergences
(closed by decision); the "real RBAC" question (closed by decision — current model is
permanent); the door-mode/light-relay coupling (fixed — now two independent features); the
`Defrost Drip/Drain Phase` deprecation warning (fixed — renamed); door/no-cool/ice alarms
having no ntfy push (fixed — all three now push).

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup — see
  `p4_sd_backup_params()`/`p4_sd_restore_params()` for the new `door_light_enabled` parameter)
- Door feature entities: `door_sensor_mode_nc_no` (NC/NO only), `sw_door_light_enabled` (new,
  light-on-open), `sw_door_sensor_enabled` (alarm master switch, now actually gates the alarm)
- New ntfy scripts: `ntfy_door_alarm_request`, `ntfy_no_cool_alarm_request`,
  `ntfy_ice_alarm_request` — same block as the pre-existing high/low/probe-fault/SD ones
- User-facing docs: `reference/USER_MANUAL.md`, `reference/QUICK_START_GUIDE.md` — kept in
  sync this session per the new `CLAUDE.md` closeout rule
- LVGL settings flow: `page_home` → `switch_to_page_settings` script → `page_pin_entry` →
  `page_settings_1` through `page_settings_7`
- Web dashboard: `assets/dashboard.html`, `assets/dashboard_virtual_preview.html`
- Old S3 reference project (read-only, for porting decisions):
  `/Volumes/Scratch/Documents/ESP32-Coolroom-Prescision/esp32-coolroom.yaml` +
  `esphome_includes.h`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`
- LVGL touchscreen mockup (Artifact): `https://claude.ai/code/artifact/9d09d4c2-8fbd-4ddc-b788-07c5afb2c8ab`

---

**Ready for:** Hardware validation remains the top blocker. This session's additions are all
well-scoped logic changes (a decoupled feature, a real bug fix, three new notification types, one
rename) layered on already-working paths — low risk, but genuinely need a real door open/close
cycle and a triggered no-cool/ice condition to confirm end-to-end.
