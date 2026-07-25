# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Help balloons extended to all System Administration action buttons
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- Follow-up to the earlier same-day setting-help-balloons work: user asked for the same treatment
  on the six System Administration action buttons (Backup, Restore, Delete Logs, Download Logs,
  WiFi Settings, Hardware Config) — one balloon per button, not one per group, since Backup and
  Restore in particular do very different things.
- Checked each button's actual backend before writing content, rather than assuming: found
  `btn_sd_backup`/`btn_sd_restore` are real ESPHome `button:` entities that call
  `p4_sd_backup_params()`/`p4_sd_restore_params()` ([esp32-p4-coolroom.yaml:1688,1717](../esp32-p4-coolroom.yaml#L1688)) —
  but the web dashboard's `backupSettings()`/`restoreSettings()` JS handlers are still stubs
  (`showAlert('info', ...)` only) that never call them, same pattern as the Operational Settings
  Update buttons flagged in the earlier addendum. Delete Logs, Download Logs, WiFi Settings, and
  Hardware Config have **no backend at all** — genuinely undefined placeholders, not just unwired.

### Latest Completed Work

- Added one `.help-icon`/`.help-popover` pair per admin action button in both
  `assets/dashboard.html` (grouped grid layout, icon after each button inline) and
  `assets/dashboard_virtual_preview.html` (flat button row layout, same inline pattern).
- Balloon content says what each button is *meant* to do, and honestly notes whether it's
  actually wired up — treating "this doesn't do anything yet" as useful information for the user
  reading it before clicking, not just a code-comment detail. Backup/Restore balloons name the
  real entity IDs (`btn_sd_backup`/`btn_sd_restore`) they'd need to be wired to if ever completed.
- Artifact republished at the same URL:
  `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`.
- No firmware/yaml touched — pure static asset edit, same as the prior same-day session. Compile
  re-run as a sanity check anyway, unchanged.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean (no firmware changes)
RAM:   20.0% (115,440 / 576,464 bytes)
Flash: 20.7% (1,517,848 / 7,340,032 bytes)
```

### Immediate Next Actions

1. If asked to continue dashboard admin work: two well-scoped follow-ups are now flagged and
   ready to pick up —
   (a) wire `backupSettings()`/`restoreSettings()` to `POST /button/btn_sd_backup/press` and
       `POST /button/btn_sd_restore/press` (real entities already exist),
   (b) wire the four Operational Settings "Update" buttons to `POST /number/<id>/set?value=X`
       (real entities already exist for all four).
   Delete Logs / Download Logs / WiFi Settings / Hardware Config would need new firmware-side
   work first (no backend exists at all) — bigger scope, not just a wiring fix.
2. Decide on the four open Carel-comparison divergences from an earlier session (symmetric vs
   asymmetric hysteresis band especially) — still pending, unrelated to this session's work.
3. Hardware validation, once connected: confirm a reboot on an already-cold room doesn't trigger
   defrost, and a warm-start pulldown holds off high-temp alarms appropriately (Carel-alignment
   session's fixes, still unverified on real hardware).
4. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).
5. Reflash the physical device once connected — every firmware change since credential rotation
   is still un-flashed, same standing hardware blocker.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior, RTC identity (0x51 vs 0x68),
   SHT31/SHT20 sensors, the LVGL page-navigation + PIN gate flow, the web dashboard status
   indicators + alarm banners, and the defrost-on-reboot/pulldown-aware-alarm-grace fixes (all
   prior sessions). All blocked — device not currently connected.
3. Device reflash for rotated credentials + all firmware changes since is still outstanding, same
   hardware blocker.
4. If real server-side dashboard authorization is ever wanted, it requires either ESPHome gaining
   multi-account/role support or a proxy in front of `web_server` — don't reintroduce fake role
   tiers in the dashboard in the meantime.
5. Decide whether the old S3 project's calibration-offset/dew-point/primary-probe-override
   features are worth porting later — deliberately left out of the humidity-sensor sessions.
6. No live-countdown entities exist for compressor lockout/defrost/drip — only LVGL-only labels
   and configured-duration `number:` entities. Would need new backend entities if ever wanted on
   the web dashboard.
7. Four Carel-comparison divergences left open from an earlier session — awaiting a decision on
   each, not to be changed without the user weighing in.
8. Two well-scoped stub-button wiring follow-ups (Backup/Restore, the four Operational Settings
   Update buttons — see Immediate Next Actions above) — real backend entities already exist for
   all of them, this is pure frontend wiring.
9. Delete Logs / Download Logs / WiFi Settings / Hardware Config admin buttons have no backend
   at all yet — bigger scope than a wiring fix if ever requested; currently honest placeholders
   with help balloons explaining as much.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup), `p4_helpers.h`
- Web dashboard: `assets/dashboard.html` (live), `assets/dashboard_virtual_preview.html` (static
  mock, kept in sync, published as the Artifact)
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`

---

**Ready for:** Hardware validation — several sessions' worth of unvalidated firmware changes are
stacked up waiting on the device being connected. This session's work (help balloons) is a pure
UI change and needs no hardware to verify, just a visual check in a browser. Also awaiting a user
decision on the four open Carel divergences, and ready to pick up either stub-button wiring
follow-up whenever asked.
