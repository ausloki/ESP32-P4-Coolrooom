# Active Context — Current Session State

**Date:** 2026-07-26
**Session:** New end-user documentation — a full User Manual and a condensed Quick Start Guide,
covering every device function by group with Nassi–Shneiderman (NS-style) structograms and a
worked recommended-settings example. No firmware changes this session.
**Status:** BUILDABLE (unchanged from last session), PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING
(blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- Ask: produce a user manual and a quick guide, organized by function group, each explaining
  what a setting/function does and how in plain language, with NS-style flow diagrams for
  the more involved control decisions. The quick guide additionally needed a worked
  recommended-settings example — cold storage of plums at ~15° Brix sugar level — plus
  screenshot placeholders (web dashboard + touchscreen) for both documents, to be filled in
  once hardware is connected.
- **Researched from the live firmware rather than from memory**: extracted the full 34-entity
  settings inventory (ids, names, units, min/max/step, defaults, `web_server` sorting groups)
  directly from `esp32-p4-coolroom.yaml`, and re-read `p4_control.h` in full to build accurate
  structograms for every control decision. Also re-read `RBAC_USER_GUIDE.md` and
  `AUTHENTICATION_GUIDE.md` to document the *current* two-tier guest/operator web model
  (post the 2026-07-24 rewrite), not any older three-tier description.

### Latest Completed Work

- **New `reference/USER_MANUAL.md`**: organized into the same groups the touchscreen (7
  pages) and web dashboard actually use — Compressor & Fallback, Defrost Schedule, Defrost
  Smart & Drip, Alarm Thresholds, Alarms Advanced, Door, Probes & Sensors — plus five
  web-only groups: Notifications (ntfy), Data & SD Card, Network & WiFi, Access Control &
  Security, System & Diagnostics. Every setting has a plain-language table row (what it
  does / range / default / when to change it). Structograms included for: compressor
  hysteresis decision, full defrost cycle (fixed interval + smart-delta + dew-point
  triggers), high/low alarm lifecycle (persist + hysteresis-clear), door alarm, probe fault
  safety behavior, SD auto-remount cycle. Includes a troubleshooting quick-reference table
  and a group ↔ touchscreen-page ↔ web-section cross-reference map.
- **New `reference/QUICK_START_GUIDE.md`**: first-time setup checklist, a one-screen "7
  groups at a glance" table, and the requested worked example — **cold storage of dessert
  plums at ~15° Brix** — mapping every relevant setting to a concrete recommended value with
  a one-line horticultural rationale (near-0°C setpoint without chill injury, tightened low
  alarm delta for freeze sensitivity, both optional defrost triggers turned on for a humid
  produce room, etc.), explicitly labeled as an illustrative starting point rather than a
  food-safety-authoritative prescription. Added a small bonus table for three other common
  cases (apples, leafy greens/general veg, dairy/general chiller) to address the "etc." in
  the request without overstating precision on produce not specifically asked about.
  Includes two condensed structograms and a plain-English ntfy-alert meaning table.
- **Screenshot placeholders**: every group/section in both documents has a clearly marked
  `📷 Screenshot placeholder` line for both web dashboard and touchscreen views.
- **One real discovery, flagged not fixed** (out of scope for a documentation-only session):
  the Door Sensor Mode (NC/NO) switch's `turn_on_action`/`turn_off_action` also toggles the
  cabinet light relay at the same time — almost certainly an unintentional coupling from
  whenever that switch was first implemented. Documented accurately as current behavior with
  an explicit "known quirk" callout in the User Manual's Door section (§4.6), rather than
  silently working around it during a session scoped as documentation-only.

### Build Status

No firmware/YAML files were touched this session — build metrics are unchanged from the
previous entry:

```text
Compile: successful via ./tools/esphome_compile.sh, clean (from last session, unchanged)
RAM:   21.3% (122,604 / 576,464 bytes)
Flash: 21.5% (1,579,656 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Fix the Door Sensor Mode / light-relay coupling flagged above, once confirmed
   unintentional with the user — small, well-scoped firmware fix, not yet done.
2. Once hardware is connected: capture the screenshots both new documents are placeholder-only
   for (web dashboard sections + all 7 touchscreen settings pages + Info/Home).
3. Hardware validation queue is unchanged from last session — see prior addendum and
   Outstanding Items below; nothing in this session added new hardware-validation surface
   (it was documentation only).
4. Decide on the four open Carel-comparison divergences from an earlier session — still
   pending.
5. Reflash the physical device once connected — standing hardware blocker, unchanged.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: the LVGL 7-page settings flow + PIN gate + SD
   auto-remount (added last session, still untested), offline-safe/SD-card behavior, RTC
   identity (0x51 vs 0x68), SHT31/SHT20 sensors, the web dashboard status indicators + alarm
   banners, the defrost-on-reboot/pulldown-aware-alarm-grace fixes, the SD log manager +
   WiFi reconnect, the live countdown timers, dew-point defrost + calibration offsets, the
   defrost-flag switches, and the full event-logging + SD-failure-detection additions. All
   blocked.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding.
4. Four Carel-comparison divergences left open from an earlier session — awaiting a decision.
5. Real server-side dashboard authorization — explicitly deferred (user chose skip).
6. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — likely benign,
   unconfirmed without hardware.
7. No ntfy push notifications for door/no-cool/ice alarms (event-log only) — small follow-up
   if ever wanted.
8. `Defrost Drip/Drain Phase` entity name contains `/` — deprecation warning now, will be a
   hard error in ESPHome 2026.7.0; small rename fix, not yet done.
9. **New**: Door Sensor Mode (NC/NO) switch also toggles the cabinet light relay — flagged
   this session in the User Manual, likely unintentional, not yet fixed pending confirmation.
10. **New**: both new documents are screenshot-placeholder-only pending hardware connection.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup)
- **New this session**: `reference/USER_MANUAL.md` (full function/setting reference + NS
  structograms), `reference/QUICK_START_GUIDE.md` (condensed + plums-at-15°-Brix worked
  example)
- Custom HTTP endpoint: `p4_log_manager.h` (`/logs` — list/download/delete SD log files)
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

---

**Ready for:** Hardware validation remains the top blocker across the whole project — this
session added no new hardware-validation surface (documentation only), but did surface one small
firmware discovery (the door-mode/light coupling) worth a quick fix once confirmed unintentional.
The two new manuals are ready to use as-is; only the screenshots are pending hardware.
