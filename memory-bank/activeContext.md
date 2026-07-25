# Active Context — Current Session State

**Date:** 2026-07-25  
**Session:** Removed second RTD board, remapped ambient temp to SHT20 (follow-up to earlier I2C humidity/temp addition)  
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- The dedicated ambient RTD board (RS485 slave 101, `probe3_temp`) is no longer needed — SHT20
  (added earlier this session as the external humidity sensor) already covers the same ambient
  role and adds humidity, which RTD never could. Removed the board entirely.
- Every consumer of the old ambient reading was remapped onto `probe_external_temp` (SHT20):
  `home_ambient_arc` (LVGL inner gauge ring, now 30s interval instead of 10s), the web
  dashboard's inner gauge ring, SD CSV logging (`ambient_c` column — same schema, new source),
  and backup/restore (`probe3_enabled` removed from the parameter list entirely).
- **Found and fixed a mislabeling** while updating diagnostics: the LVGL info page and web
  dashboard's "Probe Status" panel both checked `hw_rs485_rtd2_ok` under a "P2" label that
  actually meant "RTD board 2 online," not "probe 2 (evaporator)" — mislabeled since it was
  written. Replaced with `sht31_online`/`sht20_online` (already existed from the earlier
  addition) on both surfaces — more accurate and more useful than what it replaced.

### Latest Completed Work

- `esp32-p4-coolroom.yaml`: removed `rtd_board_2` modbus_controller, `rtd2_ch1_raw`,
  `probe3_temp`, `rtd2_ch1_age_s`, `rs485_rtd2_online`, `select_probe3_source` (a non-functional
  decorative dropdown — never actually switched anything, doubly stale once RTD Channel 3 no
  longer existed), and the `hw_rs485_rtd2_ok`/`rtd2_ch1_last_ms`/`input_probe3_enabled` globals.
  Removed `modbus_rtd2_address`/`rtd2_temp_reg_ch1` substitutions.
- `p4_logging.h`: removed `probe3_enabled` from both `p4_sd_backup_params()`/
  `p4_sd_restore_params()` signatures; updated all 3 yaml call sites (boot restore, manual
  backup button, manual restore button).
- LVGL right panel: dropped the redundant standalone "Ambient" label (SHT20 temp+RH was already
  shown by `lbl_ext_humidity_large`) — 4 items now, re-spaced at 116px steps.
- Header comment block, phase-status table ("3x RTD" → "2x RTD" — also fixed in both
  `copilot-instructions.md` files), `reference/hardware_pins.md`, `README.md`,
  `reference/DISPLAY_ARCHITECTURE_VISUAL.md`, `reference/program_control_logic_flowchart.md`,
  `reference/control_logic_ns_diagram.md`, and `assets/dashboard.html` all updated to match.
- Left untouched (out of scope): `select_probe1_source`/`select_probe2_source` are the same kind
  of non-functional dropdown as the removed one, but weren't part of this request.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh
RAM:   19.5% (112,320 / 576,464 bytes)
Flash: 20.4% (1,496,104 / 7,340,032 bytes) — both down slightly (one fewer RS485 board)
Warning level: only generic ESP-IDF experimental-features warning remains
```

### Immediate Next Actions

1. **Hardware validation** — no physical SHT31/SHT20 has been tested against this firmware yet,
   and the RTD board 2 removal means the physical second RTD board (if still wired) should be
   disconnected/repurposed. Verify SHT31 ADDR pin strapping (0x44 vs 0x45) before flashing.
   Blocked — device not connected this session.
2. **Reflash the physical device** (`esphome upload`) — still pending from an earlier session's
   credential rotation, same standing hardware blocker.
3. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation still required for: offline-safe/SD-card behavior, RTC identity (0x51 vs
   0x68), the SHT31/SHT20 sensors (address confirmation, physical wiring, cable-run integrity for
   the external sensor), and physically removing/disconnecting the old RTD board 2 if still
   wired. All blocked — device not currently connected.
3. Device reflash for the rotated web/OTA credentials is still outstanding, same hardware
   blocker.
4. If real server-side dashboard authorization is ever wanted, it requires either ESPHome gaining
   multi-account/role support or a proxy in front of `web_server` — don't reintroduce fake role
   tiers in the dashboard in the meantime.
5. Decide whether the old S3 project's calibration-offset/dew-point/primary-probe-override
   features are worth porting later — deliberately left out of both humidity-sensor sessions.
6. `select_probe1_source`/`select_probe2_source` are non-functional decorative dropdowns (same
   issue as the removed `select_probe3_source`) — worth cleaning up or actually implementing
   later, but out of scope so far.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`

---

**Ready for:** Hardware validation (new sensors + old RTD board 2 removal) plus the still-pending
credential reflash (both blocked — hardware not connected this session), then Phase 5 hardware
validation on the ESP32-P4 target.
