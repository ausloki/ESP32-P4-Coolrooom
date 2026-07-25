# Active Context — Current Session State

**Date:** 2026-07-25  
**Session:** I2C humidity/temp sensors (SHT31 internal + SHT20 external) — sensing, display, backup/restore  
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- Added SHT31 (0x44, internal/coolroom) and SHT20 (0x40, external/ambient) humidity+temp sensors
  on the existing shared I2C bus (GPIO7/8, item-19 4-pin header) — no GPIO conflicts, no new
  connector. RTC (0x51) and GT911 touch (0x5D) already share this bus at distinct addresses.
  Confirmed sensor placement via a clarifying question: SHT31 = internal, SHT20 = external.
- `esp32-p4-coolroom.yaml`'s `i2c:` block had a Phase 2 comment reserving the bus for RTC only
  ("NOT for temperature sensors") — judged humidity as new capability RS485 RTD can't provide at
  all, not a reopening of that decision, and updated the comment to explain both.
- **Found and fixed a real pre-existing bug**: `p4_logging.h`'s `p4_sd_restore_params()` read
  `backup.json` into `char buf[256]`, but the file is already ~840 bytes — most bool toggles were
  silently never parsed by `strstr`, meaning restore was quietly keeping in-memory defaults for
  much of the existing feature-toggle set (not just the new humidity ones). Grown to 1536 bytes.

### Latest Completed Work

- Added `sht3xd`/`htu21d` raw sensors + enable-gated `probe_internal_temp/humidity` and
  `probe_external_temp/humidity` template sensors (mirrors the probe2/probe3 pattern), plus
  `input_humidity_internal_enabled`/`input_humidity_external_enabled` NVS globals and
  `sht31_online`/`sht20_online` diagnostics.
- LVGL right panel: added `lbl_int_humidity_large`/`lbl_ext_humidity_large`, retightened all 5
  right-panel items from 100px to 88px spacing to fit the 496px container.
- `p4_sd_backup_params()`/`p4_sd_restore_params()`: added the two new bool params, threaded
  through all three call sites (boot restore, manual backup button, manual restore button).
- `assets/dashboard.html` + `dashboard_virtual_preview.html`: added an Internal/External
  reading-pill row under the gauge panel; preview republished to the same Artifact URL.
- `reference/hardware_pins.md`: documented the I2C address table, shared-bus wiring, and a
  cable-length caution for the external sensor's run outside the enclosure.
- Did **not** port the old project's calibration-offset/dew-point/primary-probe-override
  features — out of scope for this request.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh
RAM:   19.6% (112,992 / 576,464 bytes)
Flash: 20.4% (1,498,584 / 7,340,032 bytes; about 1.50 MB of a 7 MB OTA slot)
Warning level: only generic ESP-IDF experimental-features warning remains
```

### Immediate Next Actions

1. **Hardware validation for the new sensors** — no physical SHT31/SHT20 has been tested against
   this firmware yet. Verify actual I2C addresses on the physical breakouts before flashing (some
   SHT31 boards ship with ADDR pulled to 0x45 instead of 0x44) — blocked, device not connected.
2. **Reflash the physical device** (`esphome upload`) — still pending from the prior session's
   credential rotation, same standing hardware blocker.
3. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation still required for: offline-safe/SD-card behavior, RTC identity (0x51 vs
   0x68), and now the new SHT31/SHT20 sensors (address confirmation, physical wiring, cable-run
   integrity check for the external sensor). All blocked — device not currently connected.
3. Device reflash for the rotated web/OTA credentials (from the 2026-07-24 security-fix session)
   is still outstanding, same hardware blocker.
4. If real server-side dashboard authorization is ever wanted, it requires either ESPHome gaining
   multi-account/role support or a proxy in front of `web_server` — don't reintroduce fake role
   tiers in the dashboard in the meantime.
5. Decide whether the old project's calibration-offset/dew-point/primary-probe-override features
   are worth porting later — deliberately left out of this pass.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`

---

**Ready for:** Hardware validation of the new humidity/temp sensors plus the still-pending
credential reflash (both blocked — hardware not connected this session), then Phase 5 hardware
validation on the ESP32-P4 target.
