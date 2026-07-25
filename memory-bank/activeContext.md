# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Named alarm warning banners (LVGL + web dashboard) + fixed a wide-reaching pre-existing entity-ID bug in the web dashboard
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User was asked a status-check question ("have we achieved LVGL/webgui visual parity with the
  old project?") and selected exactly two of four proposed follow-ups: add named alarm warning
  banners on both surfaces, and close the missing-readings gap on the web dashboard. A full LVGL
  main-screen redesign to match the old project's meter/needle layout was explicitly **not**
  selected — do not revisit that without the user asking again.
- **Major discovery while investigating (not user-reported)**: `assets/dashboard.html`'s
  `parseStates()` had been reading wrong entity IDs since the dashboard was first built —
  internal `ctl_*` global variable names and guessed domains instead of the real published `id:`
  fields. This affected: alarm high/low/ice/probe-fault binary sensors (`ctl_*` → real
  `alarm_high_active`/`alarm_low_active`/`ice_alarm_sensor`/`probe_fault_active`), compressor/
  defrost (wrong domain — should be `switch.*` not `binary_sensor.*`), RS485/RTC health sensors
  (`hw_*_ok` globals → real `rs485_relay_online`/`rs485_rtd1_online`/`rtc_online`), the Wi-Fi
  indicator (`wifi_connected` doesn't exist → `binary_sensor.controller_online`), free heap
  (`free_heap` → `free_heap_kb`), and every `number.ctl_*` settings readback (→ `setpoint`,
  `alarm_high_delta`, `alarm_low_delta`, `compressor_differential`). **The compressor/defrost
  status badges, alarm bell, probe-fault alert, RS485/RTC health row, and settings input pre-fill
  have never reflected real device state on the live web dashboard.** All fixed this session.

### Latest Completed Work

- `assets/dashboard.html`: fixed every wrong entity ID in `parseStates()` (see above), added
  parsing for `binary_sensor.door_alarm_active`/`no_cool_alarm_sensor` (existing entities the
  dashboard never read) and `sensor.probe2_temp` (evaporator).
- New pulsing `.alarm-banner` on the web dashboard — shows whichever of HIGH TEMPERATURE / LOW
  TEMPERATURE / DOOR OPEN / NO COOLING / ICE DETECTED are active. Kept separate from the existing
  plain `.alert-danger`/`.alert-warning` boxes (probe fault, Wi-Fi disconnect).
- New Evaporator reading pill on the web dashboard (`sensor.probe2_temp`), third pill alongside
  Internal (SHT31) / External (SHT20). Lockout/defrost/drip countdowns considered and dropped —
  no backing live-countdown entities exist yet; out of scope for a reading-gap close-up.
- `esp32-p4-coolroom.yaml`: new LVGL `page_home` widget `lbl_home_alarm_banner` — scrolling
  (`long_mode: SCROLL_CIRCULAR`) label in the 32px gap between the left icon column and the tab
  bar (`x:96, y:520, width:912, height:28`), hidden by default. Driven from the existing "Phase
  4: 1s LVGL display updates" `interval:` block via `lvgl.widget.update` (hidden) +
  `lvgl.label.update` (text), reading the same five alarm globals the web banner uses — both
  surfaces show the identical named-alarm set. Deliberately excludes probe fault, which already
  has its own indicator (`lbl_status_text` + `led_probe_fault`).
- `assets/dashboard_virtual_preview.html`: synced (evaporator pill + a demo active alarm banner
  so the static preview visibly demonstrates the new feature). Artifact republished at the same
  URL: `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean on the first attempt (no flake)
RAM:   20.0% (115,424 / 576,464 bytes)
Flash: 20.7% (1,517,672 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Hardware validation, once the device is connected, should specifically check: the corrected
   web dashboard status badges (compressor/defrost/alarm bell/probe-fault/RS485/RTC — none of
   these were verified against a real device before, since the bug predates this session) and the
   new LVGL banner's scroll behavior on the touchscreen.
2. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below) —
   unchanged from prior sessions, still blocked.
3. Reflash the physical device once connected — still pending from credential rotation and the
   PIN-gate/page-navigation fix, same standing hardware blocker.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior, RTC identity (0x51 vs 0x68),
   SHT31/SHT20 sensors (address confirmation, physical wiring), the LVGL page-navigation + PIN
   gate flow, and now the corrected web dashboard status indicators + both new alarm banners.
   All blocked — device not currently connected.
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

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`

---

**Ready for:** Hardware validation — this session both fixed a wide-reaching pre-existing web
dashboard bug and added two new visible UI features, none of it checked against a real device
yet. Blocked on hardware not being connected this session.
