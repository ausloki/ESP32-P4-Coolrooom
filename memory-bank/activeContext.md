# Active Context — Current Session State

**Date:** 2026-07-25
**Session:** Help balloons guest-gated; Hardware Config became a real live diagnostics panel; three admin features scoped, awaiting user direction
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING (blocked on hardware)

## Current Focus

### What Was Confirmed This Session

- User gave feedback on the earlier same-day admin-balloon work: (1) help balloons should only be
  visible after admin login, not on the guest-visible main page; (2) Delete Logs/Download
  Logs/WiFi Settings/Hardware Config should be real functioning features, not just explanatory
  text — with specific behavior described for each.
- Researched before building: confirmed via the local ESPHome package source
  (`web_server_base.h`) that a custom SD file-manager endpoint (list/serve/delete) is feasible but
  requires a new `AsyncWebHandler` C++ component — nothing like it exists in ESPHome or this
  project today. Also identified that a safe WiFi test-then-commit switchover needs a new state
  machine running alongside ESPHome's own WiFi reconnect logic, carrying real risk to the device's
  primary connectivity if done carelessly — worth flagging given this controller is explicitly
  designed to stay autonomous and alert-reachable.
- Implemented the two low-risk, well-understood pieces immediately; presented findings and asked
  before starting the other three (Delete Logs, Download Logs, WiFi scan+switch), which all need
  genuinely new firmware infrastructure rather than wiring an already-existing entity.

### Latest Completed Work

- **Help balloons now guest-invisible.** `setSectionInteractive()` in both `assets/dashboard.html`
  and `assets/dashboard_virtual_preview.html` now hides every `.help-icon` in a section when that
  section is guest-disabled (`classList.toggle('hidden', !enabled)`), and force-closes any open
  popover on logout. Previously they were exempted from disabling so guests could still read them
  — reversed per this session's feedback.
- **Hardware Config is now a real live diagnostics panel**, not static help text. Every stat it
  needed was already a published entity — no new firmware work: `binary_sensor.controller_online`,
  `rs485_relay_online`, `rs485_rtd1_online`, `rtc_online`, `sht31_online`, `sht20_online`,
  `sd_card_online`; `sensor.chip_temp`, `free_heap_kb`, `free_psram_kb`, `wifi_rssi`;
  `text_sensor.wifi_ssid_text`, `ip_address`. New `updateHardwarePanel(data)` populates a
  `.hw-grid` every poll cycle from `updateDashboard()`. The button (`⚙️ Hardware Config`) now opens
  the panel directly via `toggleHelp(event, 'hw-panel')` — the separate "i" icon for this one item
  was redundant and removed, and the dead `hardwareSettings()` stub function was deleted. Dropped
  the `danger` (red) button styling too, since viewing diagnostics isn't a destructive action.
  Virtual preview mirrors the same panel with static mock values (no real device to poll there).
- Artifact republished at the same URL:
  `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`.

### Not Implemented — Awaiting User Direction

Presented to the user rather than built blind, given effort size and (for WiFi) real risk:

1. **Delete Logs** — "delete a selected log entry." Needs a new SD file-listing + delete HTTP
   endpoint. No such capability exists in ESPHome's `web_server` or this project.
2. **Download Logs** — "save a selected log to the local computer." Same missing infrastructure
   as #1 (file listing), plus a file-serving endpoint. Naturally paired with #1 — likely built
   together as one small custom component (a "SD log manager" `AsyncWebHandler`).
3. **WiFi Settings** — "show current WiFi credentials and stats, scan and connect to a new network
   without dropping the existing connection until confirmed working, then commit." Needs: (a) a
   scan-and-list-networks capability (not exposed by ESPHome's `wifi:` component today), (b) a new
   connect-test-confirm-or-rollback state machine that doesn't destabilize the existing connection
   if the new one fails. Also flagged: showing the *current* WiFi password back through the web UI
   is a security anti-pattern (same reasoning as why the touchscreen PIN never calls
   `publish_state()`) — recommended showing SSID/RSSI/IP/signal quality instead of the actual
   credential, with the new-network form being write-only.

Technical plan/feasibility notes recorded in `reference/session_recaps.md`'s matching entry and
`HANDOVER_NOTES_2026-07-18.md`'s addendum — read those before starting whichever of these gets
picked up next, rather than re-deriving the constraints from scratch.

### Build Status

No firmware/yaml touched — pure static HTML/CSS/JS asset change again. Compile re-run as a sanity
check, unchanged from the last several sessions:

```text
Compile: successful via ./tools/esphome_compile.sh, clean (no firmware changes)
RAM:   20.0% (115,440 / 576,464 bytes)
Flash: 20.7% (1,517,848 / 7,340,032 bytes)
```

### Immediate Next Actions

1. Awaiting user direction on Delete Logs / Download Logs / WiFi Settings — likely to become a
   dedicated implementation session given the new-component scope, once prioritized.
2. Two smaller, already-scoped stub-wiring follow-ups remain available if picked up first: Backup/
   Restore → `POST /button/btn_sd_backup|btn_sd_restore/press`; the four Operational Settings
   Update buttons → `POST /number/<id>/set?value=X`. Both have real entities already, pure
   frontend wiring, much lower effort than the three above.
3. Decide on the four open Carel-comparison divergences from an earlier session — still pending,
   unrelated to this session's work.
4. Hardware validation, once connected: confirm defrost-on-reboot and pulldown-aware alarm grace
   fixes behave as designed (Carel-alignment session, still unverified on real hardware).
5. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below).
6. Reflash the physical device once connected — every firmware change since credential rotation
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
8. Two well-scoped stub-button wiring follow-ups (Backup/Restore, Operational Settings Update
   buttons) — real backend entities already exist, pure frontend wiring.
9. Three admin features (Delete Logs, Download Logs, WiFi scan+safe-switch) need new firmware
   infrastructure — see "Not Implemented — Awaiting User Direction" above. Do not start without
   the user confirming scope/priority; the WiFi one in particular carries real connectivity risk
   if implemented carelessly.

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
- ESPHome source for reference (local venv): `.venv/lib/python3.12/site-packages/esphome/components/web_server_base/web_server_base.h` (custom endpoint feasibility)

---

**Ready for:** User direction on the three deferred admin features. Also awaiting a decision on
the four open Carel divergences, and ready to pick up either of the two small stub-wiring
follow-ups whenever asked. Hardware validation remains the standing blocker for everything
firmware-side; this session's work is pure UI and needs no hardware to verify.
