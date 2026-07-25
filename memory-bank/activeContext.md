# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Control logic benchmarked against Carel IR33 series; two divergences fixed (defrost-on-reboot, flat startup alarm grace)
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User asked for a comparison of the cooling/compressor/defrost/alarm logic against a commercial
  Carel IR33-series controller, to confirm the basics follow the same control path before any
  changes — reviewed and presented first, no changes made until the user confirmed which to fix.
- Reviewed `p4_control.h` + the main 10s control tick against Carel's standard `dIn` parameter
  set. **Confirmed matching**: probe-fault fallback duty cycling (`c.CY`-equivalent), dual defrost
  termination (time + evap-probe, matches `Md`/`dtE`), post-defrost drip hold (`dP`), compressor
  off-time lockout (`c2`), high/low alarm deltas (`AH`/`AL`), alarm persist delay (`Pab`), alarm
  recovery hysteresis (`rE`), door alarm delay (`dAd`), 8h defrost interval. No-cool alarm,
  ice/evap-delta alarm, and smart delta-defrost are enhancements beyond a base IR33 — not gaps.
- Found six divergences from Carel's baseline; user approved fixing two (#3 defrost-on-reboot,
  #4 flat startup alarm grace) this session. **Four left open, not fixed, pending a decision**:
  1. Compressor hysteresis band is symmetric (±0.5°C around setpoint) vs Carel's asymmetric (ON
     at setpoint+diff, OFF at exactly setpoint) — the one real "basics" divergence, needs a call.
  2. No minimum compressor ON-time / anti-short-cycle start delay (Carel's `c1`/`c0`).
  3. No fan control anywhere in the project — needs confirming whether the evaporator fan is
     wired independently (own thermostat/always-on) or if that's a real gap.
  4. Door switch doesn't pause compressor regulation or suppress the high-temp alarm while open.

### Latest Completed Work

- **Fix #3 — defrost no longer forced on every reboot.** `p4_ctl_defrost_due()` treated
  `ctl_defrost_last_end_ms == 0` ("never run") as "due immediately" after 10 min uptime, so any
  reboot (WiFi hiccup, OTA) of an already-cold room triggered an unwanted defrost. Fixed by
  seeding `ctl_defrost_last_end_ms = ctl_boot_ms` in the `on_boot` priority-600 lambda — the
  interval clock now starts from power-on. Carel's `d0` (defrost-at-startup) defaults off; this
  now matches.
- **Fix #4 — startup alarm grace is now pulldown-aware, not a flat timer.** Old 15-min flat
  window was too short for a genuine warm-start pulldown (first commissioning, long outage),
  letting the high-temp alarm fire before the room ever reached setpoint once. New: grace holds
  unconditionally for 15 min (floor, unchanged for the common case), then continues until the
  room first reaches the alarm-safe band, capped at a new hard 4h ceiling
  (`startup_grace_max_min` substitution constant — not a tunable entity, same treatment as
  `probe_stale_ms`). New pure functions `p4_ctl_pulldown_reached()` /
  `p4_ctl_startup_grace_active()` in `p4_control.h`; new runtime-only global
  `ctl_startup_pulldown_done` (resets every boot by design, not persisted/backed up).
- Full Carel comparison table and parameter mapping recorded in
  `reference/session_recaps.md`'s 2026-07-25 "Control Logic Reviewed Against Carel IR33 Series"
  entry.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   20.0% (115,440 / 576,464 bytes)
Flash: 20.7% (1,517,848 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Decide on the four open Carel divergences (symmetric vs asymmetric hysteresis band especially
   — needs a call either way, not obviously a bug) — ask if/when revisiting this topic.
2. Hardware validation, once connected, should specifically check: a reboot on an already-cold
   room does *not* trigger defrost, and a cold start from a warm room holds off high-temp alarms
   until setpoint is genuinely reached (or the 4h ceiling).
3. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below) —
   unchanged from prior sessions, still blocked.
4. Reflash the physical device once connected — still pending from credential rotation and every
   firmware change since, same standing hardware blocker.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation required for: offline-safe/SD-card behavior, RTC identity (0x51 vs 0x68),
   SHT31/SHT20 sensors (address confirmation, physical wiring), the LVGL page-navigation + PIN
   gate flow, the web dashboard status indicators + both alarm banners (prior session), and now
   the defrost-on-reboot fix + pulldown-aware alarm grace (this session). All blocked — device
   not currently connected.
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
7. Four Carel-comparison divergences left open (see "What Was Confirmed This Session" above) —
   awaiting a decision on each, not to be changed without the user weighing in.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Control logic: `p4_control.h` (pure functions), `p4_logging.h` (SD/backup), `p4_helpers.h`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware/I2C reference: `reference/hardware_pins.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`

---

**Ready for:** Hardware validation — two prior sessions' worth of unvalidated changes (web
dashboard fixes, alarm banners, and now the Carel-alignment defrost/alarm-grace fixes) are all
stacked up waiting on the device being connected. Also awaiting a user decision on the four open
Carel divergences before touching that area further.
