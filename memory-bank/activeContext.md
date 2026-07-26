# Active Context — Current Session State

**Date:** 2026-07-26
**Session:** Ntfy timestamps, full event-log sensor context for every control decision, SD-optional operation confirmed + runtime-failure detection and ntfy alert added
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- Four related asks: (1) every ntfy message needs a timestamp, (2) every control decision
  (defrost/compressor/alarm) must be event-logged with the sensor readings behind it, for
  troubleshooting, (3) the system must run fully without an SD card and suppress SD-dependent
  functions gracefully if none is detected at boot, (4) an ntfy alert must fire if the card fails.
- **Audited before changing anything** rather than assuming: found compressor on/off transitions
  were never logged at all; door/no-cool/ice alarms were never logged at all; existing
  ALARM_HI/LO/PROBE_FAULT logging was real but **only happened inside the WiFi-gated ntfy block**
  — an offline device logged nothing to SD for an alarm either, even though SD logging has nothing
  to do with WiFi. Also found the SD-optional requirement was largely already satisfied
  structurally (every `p4_sd_*` function already no-ops safely without a card; no control-critical
  logic reads `sd_card_ok`) — but there was **no detection of a runtime SD failure** (card removed
  after a successful boot mount) — `sd_card_ok` would silently stay "true" forever after that.

### Latest Completed Work

- **Ntfy timestamps**: all four existing scripts (`ntfy_high_alarm_request`,
  `ntfy_low_alarm_request`, `ntfy_alarm_clear_request`, `ntfy_probe_fault_request`) now prepend
  `[timestamp]` via the existing `p4_fmt_time()` helper.
- **New step 7b in the main control tick**: unconditional (WiFi-independent) edge-detection
  logging for `ALARM_HI`/`_LO`, `PROBE_FAULT`, and two brand-new coverage gaps —
  `DOOR_ALARM`, `NO_COOL_ALARM`, `ICE_ALARM` (+ matching `_CLEAR` events for all six), each with
  the actual sensor readings behind the decision. New tracking globals
  `ctl_alarm_hi_logged`/`_lo_logged`/`_probe_fault_logged`/`_door_alarm_logged`/
  `_no_cool_alarm_logged`/`_ice_alarm_logged` — deliberately separate from the pre-existing
  `ntfy_*_sent` flags, since SD-log edge-detection must be independent of WiFi/ntfy-delivery
  edge-detection.
- **New step 10**: `COMPRESSOR_ON`/`COMPRESSOR_OFF` event logging — didn't exist before.
  Implemented as one before/after check at the very end of the tick (comparing
  `relay_compressor.state` at tick-start vs. tick-end) rather than scattering log calls across
  the ~6 places the relay gets toggled — guarantees exactly one log entry per real transition,
  with the reason (`hysteresis`/`defrost`/`sensor_fallback`/`lockout_end`) inferred from which
  mode was active.
- `DEFROST_START`/`DEFROST_END`/`DEFROST_MANUAL_STOP` detail strings extended with actual
  coolroom/evap temps (+ setpoint or termination-temp) — previously just a bare reason string.
- **SD-optional + failure alert**: new `p4_sd_mark_failed()` in `p4_logging.h`, called from every
  write-path `fopen()` failure in `p4_sd_log_temps()`/`p4_sd_log_event()`/`p4_sd_backup_params()`
  (deliberately not the read-path in `p4_sd_restore_params()` — a missing `backup.json` on first
  boot is normal). New step 11 in the control tick mirrors a detected ready→not-ready transition
  into the `sd_card_ok` diagnostic immediately. New `ntfy_sd_failure_request` script + edge-
  triggered logic in step 9 fires once per failure episode, covering both "not detected at boot"
  and "failed mid-session" through the same `sd_card_ok` check — no separate boot-specific code
  path needed.
- **Not built**: auto-remount on card reinsertion (would need active polling for a newly-inserted
  card) — recovery is a manual reboot once storage is fixed; noted as a known limitation, wasn't
  asked for.
- **Not built**: new ntfy push notification *types* for door/no-cool/ice alarms — only event-log
  coverage was explicitly requested for those three; only high/low/probe-fault/SD-failure have
  ntfy pushes.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   20.6% (118,652 / 576,464 bytes)
Flash: 21.0% (1,541,560 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Hardware validation, once connected, should now also cover: every alarm type's event log entry
   actually contains sensible sensor readings during a real alarm condition; the compressor on/off
   log fires exactly once per real transition; and — carefully — an actual SD card
   removal-while-running test to confirm `p4_sd_mark_failed()` detects it and the ntfy alert fires.
2. Everything else queued from prior sessions remains open: the 14 newly-exposed settings, dew-
   point defrost + calibration offset validation, the `opendir`/`readdir`/`closedir` linker warning
   on the SD log manager, and the WiFi reconnect path (highest priority — affects reachability).
3. Decide on the four open Carel-comparison divergences from an earlier session — still pending.
4. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).
5. Reflash the physical device once connected — every firmware change since credential rotation
   is still un-flashed, same standing hardware blocker.
6. If ntfy push notifications for door/no-cool/ice alarms are ever wanted (only event-log coverage
   exists for them now), that's a small, well-scoped follow-up — three new scripts mirroring the
   existing high/low/probe-fault pattern.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior (including an actual card-
   removal test now), RTC identity (0x51 vs 0x68), SHT31/SHT20 sensors, the LVGL page-navigation +
   PIN gate flow, the web dashboard status indicators + alarm banners, the defrost-on-reboot/
   pulldown-aware-alarm-grace fixes, the SD log manager + WiFi reconnect, the live countdown
   timers, dew-point defrost + calibration offsets, the defrost-flag switches, the 14 newly-exposed
   settings, and now the full event-logging + SD-failure-detection additions. All blocked.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding, same
   hardware blocker.
4. Four Carel-comparison divergences left open from an earlier session — awaiting a decision.
5. Two dead LVGL widgets on the Settings 3 / Fallback page (`lbl_probe1_status`, two unlabeled
   compressor/defrost LED+label pairs) — noted, not fixed, unclear original intent.
6. Real server-side dashboard authorization — explicitly deferred (user chose skip), revisit only
   if a concrete multi-user need arises.
7. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — likely benign (known
   ESP-IDF FATFS VFS pattern) but unconfirmed without hardware; verify on first real SD test.
8. LVGL touchscreen has no equivalent controls for the 14 settings added last session (or the
   dew-point/calibration additions from before that) — web GUI only, by design.
9. No ntfy push notifications for door/no-cool/ice alarms (event-log only) — small follow-up if
   ever wanted.
10. No auto-remount when a failed/removed SD card is reinserted — manual reboot required to
    recover; a known, accepted limitation, not built since it wasn't requested.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup — see
  `p4_sd_mark_failed()` for the new runtime-failure detection), `p4_helpers.h`
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

**Ready for:** Hardware validation — the backlog keeps growing across sessions. This session's
additions (event logging, ntfy timestamps, SD-failure detection) are all logic/logging changes
layered on already-working control paths, low risk to core behavior, but genuinely need a real
alarm/defrost/compressor cycle — and ideally an actual SD card pull test — to confirm they behave
as designed.
