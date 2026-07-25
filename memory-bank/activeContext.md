# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Dew-point-triggered early defrost ported from the old S3 project; manual per-probe calibration offset added (auto-calibration routine deliberately not ported)
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- Follow-up to explaining what the old S3 project's calibration-offset/dew-point/primary-probe-
  override features actually did (read directly from `esp32-coolroom.yaml` and
  `esphome_includes.h` in the old `ESP32-Coolroom-Prescision` project, not from memory). User
  decision: **scrap primary-probe-override entirely**, **port dew-point early defrost as-is**,
  **add only the manual offset entry** from calibration — explicitly not the old project's
  automated 15-20-sample "Start Calibration" averaging routine.
- Confirmed the internal SHT31 in this project is the correct analog to the old project's dew-point
  reference sensor: this project's SHT31 is the *internal/coolroom-air* sensor, SHT20 is
  *external/ambient* — same pairing role the old project needed, just different physical sensor
  models. This resolves the exact ambiguity that got these features deferred in an earlier session.

### Latest Completed Work

- **Dew-point early defrost**: new pure functions in `p4_control.h` — `p4_calc_dew_point_c()`
  (Magnus formula) and `p4_ctl_dew_point_defrost_ready()` (evap colder than both freezing and the
  dew point → frost is actually forming right now). New globals `ctl_last_dew_point_c`,
  `ctl_dew_point_defrost_triggered` (reset on every defrost end — automatic end-by-temp,
  end-by-timeout, or manual stop via `btn_manual_defrost_stop` — so it can fire again next frost
  cycle), `input_dew_point_trigger_enabled`. Added as a fourth defrost-start trigger
  (`dew_point_start`) alongside manual/smart/interval in the control tick's "not defrosting"
  branch — layered on top of the existing triggers, not replacing any of them. **Exposed as a real
  switch entity** `sw_dew_point_trigger` (`platform: template`, `group_defrost`) so it's actually
  reachable — unlike its conceptual siblings.
- **Found, not fixed, while wiring that switch**: `input_smart_defrost_enabled`,
  `input_defrost_drip_enabled`, `input_defrost_term_temp_enabled` have **no way to be toggled at
  runtime at all** — no `switch:` entity, no LVGL control, nothing. Permanently stuck at their
  YAML compile-time defaults unless hand-edited into a backup.json. Deliberately didn't replicate
  this gap for the new dew-point trigger (an unreachable toggle would defeat implementing the
  feature) but didn't fix the three pre-existing ones either — flagged for a future session.
- **Manual per-probe calibration offset**: new `number:` entities `probe1_offset_c` /
  `probe2_offset_c` (±10°C, 0.1°C step, `entity_category: config`, `group_probes`), backed by new
  globals `ctl_probe1_offset_c`/`ctl_probe2_offset_c`. Applied via a `filters: - lambda:` on
  `probe1_temp`/`probe2_temp` (after unit conversion, NaN-safe) so every downstream consumer
  (control, alarms, logs, LVGL display) sees the calibrated value transparently — same effect as
  the old project's offsets, just without the auto-averaging-against-SHT31 sequence behind them.
- **Consistency follow-through**: extended `p4_sd_backup_params()`/`p4_sd_restore_params()`
  (`p4_logging.h`) and all three call sites (`on_boot` restore, `btn_sd_backup`, `btn_sd_restore`)
  to round-trip the two new offsets and the trigger flag through SD backup/restore, matching every
  sibling parameter. Made restore backward-compatible on purpose: the three new fields seed from
  the caller's current value (not `NAN`) before parsing and are excluded from the all-or-nothing
  `isfinite` gate, so an **older backup.json without these keys still restores successfully**
  instead of failing the whole restore over fields that didn't exist when it was written.
- **Noticed, unrelated, not fixed**: this build surfaced `opendir`/`readdir`/`closedir is not
  implemented and will always fail` linker warnings, stemming from the SD log manager
  (`p4_log_manager.h`, added two sessions ago) using `dirent.h`. Very likely benign — a known
  ESP-IDF pattern where the default newlib stub triggers this warning at link time, but real
  directory operations get dispatched through the mounted FATFS VFS at runtime instead. Cannot
  confirm without hardware. Added to the priority list for the log manager's first real test:
  confirm `/logs` actually lists files, not just that it compiles.
- **Not done, deliberately**: no custom web dashboard UI for either feature. The new `number:`/
  `switch:` entities are reachable via ESPHome's own auto-generated web_server UI and the API —
  that satisfies "allowing an offset value to be entered," which is what was asked, without
  dashboard.html work that wasn't part of this request.

### Build Status

Compiled clean at every incremental step (control functions → globals/entities → control-tick
wiring → backup/restore threading) rather than writing everything and debugging one large failure:

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   20.2% (116,532 / 576,464 bytes)
Flash: 20.8% (1,528,664 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Hardware validation, once connected, should now also cover: dew-point trigger firing correctly
   during a real frost cycle, and the calibration offsets actually shifting the published reading
   as expected against a reference thermometer.
2. Specifically check whether the `opendir`/`readdir`/`closedir` linker warning is actually benign
   at runtime — test `/logs` (list/download/delete) against the real SD card early.
3. Two well-scoped stub-wiring follow-ups remain available if picked up: Backup/Restore →
   already wired (done two sessions ago); the sibling enable-flag gap
   (`input_smart_defrost_enabled` etc. having no runtime toggle) is a new, larger follow-up if
   ever wanted — would need three new switch entities plus LVGL controls, not just one.
4. Decide on the four open Carel-comparison divergences from an earlier session — still pending.
5. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).
6. Reflash the physical device once connected — every firmware change since credential rotation
   is still un-flashed, same standing hardware blocker.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior, RTC identity (0x51 vs 0x68),
   SHT31/SHT20 sensors, the LVGL page-navigation + PIN gate flow, the web dashboard status
   indicators + alarm banners, the defrost-on-reboot/pulldown-aware-alarm-grace fixes, the SD log
   manager + WiFi reconnect, the live countdown timers, and now dew-point defrost + calibration
   offsets. All blocked — device not currently connected.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding, same
   hardware blocker.
4. Decide whether the old S3 project's remaining ported-vs-not decisions need revisiting — primary-
   probe-override was explicitly scrapped this session, not deferred; no further action needed
   there unless the user changes their mind.
5. Four Carel-comparison divergences left open from an earlier session — awaiting a decision.
6. Two dead LVGL widgets on the Settings 3 / Fallback page (`lbl_probe1_status`, two unlabeled
   compressor/defrost LED+label pairs) — noted, not fixed, unclear original intent.
7. Real server-side dashboard authorization — explicitly deferred (user chose skip), revisit only
   if a concrete multi-user need arises.
8. `input_smart_defrost_enabled`/`input_defrost_drip_enabled`/`input_defrost_term_temp_enabled`
   have no runtime toggle at all (no switch entity, no LVGL control) — found this session, not
   fixed, flagged for later if ever wanted.
9. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — likely benign (known
   ESP-IDF FATFS VFS pattern) but unconfirmed without hardware; verify on first real SD test.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup), `p4_helpers.h`
- Custom HTTP endpoint: `p4_log_manager.h` (`/logs` — list/download/delete SD log files)
- Web dashboard: `assets/dashboard.html` (live), `assets/dashboard_virtual_preview.html` (static
  mock, kept in sync, published as the Artifact)
- Old S3 reference project (read-only, for porting decisions):
  `/Volumes/Scratch/Documents/ESP32-Coolroom-Prescision/esp32-coolroom.yaml` +
  `esphome_includes.h`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`

---

**Ready for:** Hardware validation — the backlog of untested firmware changes keeps growing
(WiFi reconnect, SD log manager, countdown timers, alarm banners, defrost/alarm-grace fixes, and
now dew-point defrost + calibration offsets). WiFi reconnect and the SD log manager's dirent
warning should be checked first since they touch core reachability and a totally new code path
respectively; dew-point/calibration are lower-risk (pure control-logic additions layered on
already-working paths) but still worth a real frost-cycle observation before trusting them.
