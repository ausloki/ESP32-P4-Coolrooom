# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Setting help balloons added to the web dashboard's admin settings
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User asked for each admin setting on the web dashboard to have a clickable help balloon
  explaining what it does in plain English and how it correlates to a sensor — a direct
  implementation request, no confirmation step needed first.
- Applied to the five actual tunable settings: Setpoint, Alarm High Delta, Alarm Low Delta,
  Compressor Hysteresis (all in Operational Settings), and Touchscreen Access PIN (System
  Administration). Deliberately **not** applied to the one-shot admin action buttons
  (Backup/Restore/Logs/WiFi/Hardware Config) — those are actions, not values a user tunes.

### Latest Completed Work

- New `.help-icon` (small circular "i" button) + `.help-popover` (card-style balloon) UI pattern
  in both `assets/dashboard.html` and `assets/dashboard_virtual_preview.html`, styled from the
  existing `--accent`/`--accent-soft` design tokens. Click-to-toggle (not hover-only, so it works
  on touch), only one balloon open at a time, closes on click-away or Escape.
- Balloon content is grounded in the real control logic in `p4_control.h`, not generic text:
  Setpoint's explains the ON/OFF thresholds relative to Compressor Hysteresis and names the
  Coolroom Temperature (Primary Control) probe; the alarm deltas name the same probe, the 5-min
  persist requirement, and what they drive (the named alarm banner, 🔔 icon); Compressor
  Hysteresis correctly notes it also sets the No-Cooling alarm's threshold
  (`setpoint + diff/2 + 0.5°C`, confirmed from `p4_ctl_no_cool_alarm()`) — independent of the
  Alarm High Delta, which is a separate threshold; the PIN field notes it has no sensor
  correlation, it only gates the touchscreen's Settings screens.
- **Found while wiring this up (not user-reported)**: the guest-mode `setSectionInteractive()`
  function disables every `input, button` in the Settings/Admin sections — this would have
  disabled the new help icons for guests too, even though explaining a setting isn't a
  privileged action. Fixed in both files to exempt `.help-icon`-classed buttons.
- **Noticed, not fixed (flagged only)**: the four Operational Settings "Update" buttons
  (`updateSetpoint()` etc.) are stubs — only show an info alert, never actually POST to the
  device — despite a code comment claiming no backing endpoint exists. That's stale: ESPHome's
  `web_server` auto-generates `POST /number/<id>/set?value=X` for every `number:` entity by
  default, the same mechanism `changeTouchscreenPin()`/`toggleLight()` already use successfully
  elsewhere on the page. Worth a small separate follow-up session; out of scope for a
  help-balloon request.
- Artifact republished at the same URL:
  `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`.

### Build Status

No firmware/yaml touched this session — pure static HTML/CSS/JS asset change. Compile re-run as
a sanity check anyway (unchanged from last session):

```text
Compile: successful via ./tools/esphome_compile.sh, clean
RAM:   20.0% (115,440 / 576,464 bytes)
Flash: 20.7% (1,517,848 / 7,340,032 bytes)
```

### Immediate Next Actions

1. If asked to continue dashboard admin work: wire the four stub Update buttons to real
   `POST /number/<id>/set` calls (flagged above, not done this session).
2. Decide on the four open Carel-comparison divergences from the prior session (symmetric vs
   asymmetric hysteresis band especially) — still pending, unrelated to this session's work.
3. Hardware validation, once connected: confirm a reboot on an already-cold room doesn't trigger
   defrost, and a warm-start pulldown holds off high-temp alarms appropriately (prior session's
   fixes, still unverified on real hardware).
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
7. Four Carel-comparison divergences left open from the prior session — awaiting a decision on
   each, not to be changed without the user weighing in.
8. The four Operational Settings "Update" buttons are non-functional stubs — real
   `POST /number/<id>/set` endpoints already exist in the firmware and are unused. Small,
   well-scoped follow-up if/when requested.

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
decision on the four open Carel divergences and the stub-button follow-up before touching either
area further.
