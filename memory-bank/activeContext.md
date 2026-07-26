# Active Context — Current Session State

**Date:** 2026-07-26
**Session:** Full settings audit — every configurable parameter is now web-settable, NVS-persistent, and included in backup/restore (found this was a much wider gap than the prior session's fix)
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User asked to ensure *every* setting can be set/enabled from the web GUI, has a help balloon,
  is included in backup/restore, and is saved to NVS for power-fail recovery. Rather than assume
  the prior session's three-switch fix covered this, ran a systematic audit: listed every
  `input_*`/`ctl_*` global, checked each against an actual `number:`/`switch:` entity reading/
  writing it, and separately checked `restore_value:` and presence in
  `p4_sd_backup_params()`/`p4_sd_restore_params()`.
- **Found 14 settings with no entity at all** (not 3) — a much wider gap than previously fixed.
- **Found one real NVS-persistence bug**: `ctl_startup_grace_min` had `restore_value: no`, the
  only setting global in the whole project marked that way.
- **Found one real backup/restore bug**: `ctl_startup_grace_min` was also completely absent from
  the backup/restore functions and all three call sites.

### Latest Completed Work

- **7 new switch entities** in `esp32-p4-coolroom.yaml`: `sw_probe2_enabled`,
  `sw_humidity_internal_enabled`, `sw_humidity_external_enabled`, `sw_door_sensor_enabled`,
  `sw_siren_enabled`, `sw_defrost_enabled` (the defrost **master** enable — previously completely
  unreachable), `sw_fallback_enabled`. All follow the same `platform: template` +
  `turn_on_action`/`turn_off_action` + NVS-sync pattern as the switches added last session.
- **7 new number entities**: `startup_grace_min`, `defrost_grace_min`, `alarm_hysteresis_c`,
  `fallback_on_min`, `fallback_off_min`, `smart_delta_c`, `smart_dwell_min` — same
  `platform: template` pattern as the existing operational-settings numbers.
- **Fixed**: `ctl_startup_grace_min`'s `restore_value: no` → `yes`.
- **Fixed**: extended `p4_sd_backup_params()`/`p4_sd_restore_params()` (`p4_logging.h`) with a new
  `startup_grace_min` parameter, updated all three call sites (`on_boot` restore, `btn_sd_backup`,
  `btn_sd_restore`). Backward-compatible: seeded from the caller's current value and excluded from
  the all-or-nothing `isfinite` gate, so existing backup.json files without this key still restore.
- **New "🛠️ Advanced Settings" section** in `assets/dashboard.html`: a data-driven
  `ADVANCED_SETTINGS_GROUPS` config array (5 groups — Compressor & Fallback, Defrost, Alarms, Door,
  Probe Calibration & Enable) covering all ~30 settings beyond the original 4 headline ones,
  rendered by `renderAdvancedSettings()` rather than hand-authored (30 near-identical blocks would
  otherwise be a lot of repetition/drift risk). Every help balloon is grounded in the real control
  logic. Numbers reuse the existing `submitNumberSetting()` helper; switches get a new
  `toggleAdvancedSwitch()`. `parseStates()` gained a generic entity-ID-matched loop instead of 30
  more manual `if` lines. Same guest/operator gating as the rest of the page.
- Mirrored in `assets/dashboard_virtual_preview.html` with a duplicated config array (`def` demo
  values added, no live device to poll) rendered as static disabled inputs/status badges.
- Artifact republished at the same URL:
  `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`.
- **Not done, deliberately**: no LVGL touchscreen controls for any of the 14 new settings — web
  GUI reachability was the explicit ask; touchscreen parity would be a separate, larger LVGL
  layout task.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   20.5% (118,428 / 576,464 bytes, +1.9 KB for 14 new entities)
Flash: 21.0% (1,538,056 / 7,340,032 bytes, +9.4 KB)
```

### Immediate Next Actions

1. Hardware validation, once connected, should now also cover: every one of the 14 newly-exposed
   settings actually changing device behavior when toggled/set via the web GUI, and confirming
   `ctl_startup_grace_min` survives a power cycle (the bug just fixed).
2. Everything else queued from prior sessions remains open: dew-point defrost + calibration
   offset validation, the `opendir`/`readdir`/`closedir` linker-warning check on the SD log
   manager, and the WiFi reconnect path (highest priority — affects reachability itself).
3. Decide on the four open Carel-comparison divergences from an earlier session — still pending.
4. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).
5. Reflash the physical device once connected — every firmware change since credential rotation
   is still un-flashed, same standing hardware blocker.
6. If LVGL touchscreen parity for these settings is ever wanted, it's a distinct, larger task
   (new keypad/list screens, not a quick add) — scope it separately when/if requested.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior, RTC identity (0x51 vs 0x68),
   SHT31/SHT20 sensors, the LVGL page-navigation + PIN gate flow, the web dashboard status
   indicators + alarm banners, the defrost-on-reboot/pulldown-aware-alarm-grace fixes, the SD log
   manager + WiFi reconnect, the live countdown timers, dew-point defrost + calibration offsets,
   the three defrost-flag switches, and now the 14 newly-exposed settings + NVS/backup fixes.
   All blocked — device not currently connected.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding, same
   hardware blocker.
4. Four Carel-comparison divergences left open from an earlier session — awaiting a decision.
5. Two dead LVGL widgets on the Settings 3 / Fallback page (`lbl_probe1_status`, two unlabeled
   compressor/defrost LED+label pairs) — noted, not fixed, unclear original intent.
6. Real server-side dashboard authorization — explicitly deferred (user chose skip), revisit only
   if a concrete multi-user need arises.
7. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — likely benign (known
   ESP-IDF FATFS VFS pattern) but unconfirmed without hardware; verify on first real SD test.
8. LVGL touchscreen has no equivalent controls for the 14 settings added this session (or the
   dew-point/calibration additions from two sessions ago) — web GUI only, by design of this
   session's scope; a future ask if touchscreen parity is wanted.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup), `p4_helpers.h`
- Custom HTTP endpoint: `p4_log_manager.h` (`/logs` — list/download/delete SD log files)
- Web dashboard: `assets/dashboard.html` (live — see `ADVANCED_SETTINGS_GROUPS` for the full
  settings config), `assets/dashboard_virtual_preview.html` (static mock, kept in sync, published
  as the Artifact)
- Old S3 reference project (read-only, for porting decisions):
  `/Volumes/Scratch/Documents/ESP32-Coolroom-Prescision/esp32-coolroom.yaml` +
  `esphome_includes.h`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`

---

**Ready for:** Hardware validation — the backlog keeps growing across sessions (WiFi reconnect,
SD log manager, countdown timers, alarm banners, defrost/alarm-grace fixes, dew-point defrost +
calibration offsets, defrost-flag switches, and now 14 more settings + two real persistence bugs
fixed). WiFi reconnect and the SD log manager's dirent warning remain top priority since they
touch core reachability and a totally new code path; everything else is lower-risk (entity/
control-logic additions on already-working paths) but still unverified against real hardware.
