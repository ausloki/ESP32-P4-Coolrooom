# Progress Tracking

## 2026-07-25 Control Logic Benchmarked Against Carel IR33 Series; Two Fixes Applied

- Compared `p4_control.h` + the main control tick against a commercial Carel IR33-series
  controller's standard parameter set, per user request, before making any changes. Confirmed
  matching: probe-fault fallback duty cycling, dual defrost termination (time + evap-probe),
  post-defrost drip hold, compressor off-time lockout, alarm deltas/persist/hysteresis, door
  alarm delay, 8h defrost interval. No-cool/ice/smart-defrost are enhancements beyond baseline.
- Found six divergences; user approved fixing two:
  - Defrost no longer fires on every reboot — `ctl_defrost_last_end_ms` now seeded to boot time
    instead of triggering immediately (matches Carel's `d0`=off default).
  - Startup alarm grace is now pulldown-aware: holds the existing 15-min floor, then continues
    until the room first reaches the alarm-safe band, capped at a new 4h hard ceiling — instead
    of a flat 15-min timer that could let alarms fire before a slow warm-start pulldown finished.
- Four divergences left open pending a decision: symmetric vs Carel's asymmetric hysteresis band,
  no minimum compressor ON-time/anti-short-cycle delay, no fan control at all, door switch doesn't
  pause compressor regulation or suppress the high-temp alarm.
- Build: RAM 20.0%, Flash 20.7% (unchanged). Compile clean. Untested on physical hardware.

## 2026-07-25 Named Alarm Warning Banners + Web Dashboard Reading Gaps Closed

- User selected 2 of 4 proposed follow-ups from a visual-parity status check: named alarm warning
  banners on both surfaces, and closing the missing-readings gap on the web dashboard. A full LVGL
  main-screen redesign was explicitly not selected.
- **Found and fixed a pre-existing bug while investigating, not user-reported**:
  `assets/dashboard.html`'s `parseStates()` had been reading wrong entity IDs (internal `ctl_*`
  globals and guessed domains) since the dashboard was first built. Compressor/defrost badges,
  alarm bell, probe-fault alert, RS485/RTC health row, and settings input pre-fill have never
  reflected real device state on the live web dashboard until this fix. Full ID mapping in
  `reference/session_recaps.md`'s matching 2026-07-25 entry.
- Added pulsing `.alarm-banner` (web) and scrolling `lbl_home_alarm_banner` (LVGL `page_home`),
  both reading the same five alarm globals: HIGH TEMPERATURE / LOW TEMPERATURE / DOOR OPEN / NO
  COOLING / ICE DETECTED. Probe fault intentionally excluded (already has its own indicator).
- Added Evaporator reading pill to web dashboard (`sensor.probe2_temp`). Lockout/defrost/drip
  countdowns dropped — no backing live-countdown entities exist.
- Build: RAM 20.0%, Flash 20.7%. Compile clean, no flake. Untested on physical hardware (not
  connected).

## 2026-07-25 Touchscreen PIN Gate + LVGL Page-Navigation Fix

- Added a 4-digit PIN lock on the LVGL settings screens (djb2 hash, default `0000`, changeable
  from the touchscreen keypad or the web dashboard's admin section) — same design as the earlier
  S3 project.
- **Correction to this file's own history**: while wiring the gate, found that the LVGL tab-bar
  buttons never called `lvgl.page.show` — only diagnostic globals were updated. The "Phase 9 ...
  Complete" note in `reference/session_recaps.md` (2026-07-18) did not reflect actual on-device
  behavior; the Settings/Info tabs have never worked by touch. Fixed as part of this session
  (required for the PIN gate to redirect anywhere real). See the 2026-07-25 "Touchscreen PIN
  Gate" recap entry for the full explanation — not editing the 2026-07-18 entry itself, per this
  project's convention of correcting history with a new dated entry rather than rewriting old
  ones.
- Build: RAM 20.0%, Flash 20.7%. Compile clean. Untested on physical hardware (not connected).

## 2026-07-25 Second RTD Board Removed, Ambient Remapped to SHT20

- Removed the dedicated ambient RTD board (RS485 slave 101, `probe3_temp`) — SHT20 already
  covers ambient and adds humidity. Remapped every consumer (LVGL ambient arc, web dashboard
  inner ring, SD logging, backup/restore) onto `probe_external_temp`.
- Found and fixed a mislabeling: LVGL info page + web dashboard "Probe Status" showed a "P2" flag
  that actually meant "RTD board 2 online," not evaporator probe status. Replaced with
  `sht31_online`/`sht20_online`.
- Updated hardware docs, phase-status tables ("3x RTD" → "2x RTD"), and control-flow diagrams to
  match. See `reference/session_recaps.md` (2026-07-25 entry) for the full change list.
- Build: RAM 19.5%, Flash 20.4% (both down slightly — one fewer RS485 board).
- Follow-up: also removed `select_probe1_source`/`select_probe2_source` (same non-functional
  decorative-dropdown issue as `select_probe3_source` — nothing ever read their value; probes
  are hard-mapped). Removed the entire now-empty `select:` block. Build: RAM 19.5%, Flash 20.3%.

## 2026-07-25 I2C Humidity/Temp Sensors

- Added SHT31 (internal, 0x44) + SHT20 (external, 0x40) on the existing shared I2C bus — no GPIO
  conflicts, no new connector (both share the item-19 4-pin header with the RTC). Enable-gated
  sensors, LVGL right-panel readouts, web dashboard reading pills, and backup/restore all wired.
  Confirmed placement via clarifying question (SHT31=internal, SHT20=external).
- **Found and fixed a real bug**: `p4_sd_restore_params()`'s 256-byte read buffer was too small
  for the actual ~840-byte backup.json — most bool toggles were silently never restored. Grown
  to 1536 bytes; see `reference/session_recaps.md` (2026-07-25 entry) for detail.
- Did not port the old project's calibration/dew-point/primary-probe-override features —
  out of scope. Hardware validation still outstanding (no physical sensor tested yet).
- Build: RAM 19.6%, Flash 20.4%.

## 2026-07-24 Security Fix

- Found and fixed a leaked credential: `assets/dashboard.html` (and the same-day
  `assets/dashboard_virtual_preview.html`) hardcoded the real device password as its
  "admin"/"superadmin" demo login, committed to git since Phase 12. Rotated
  `ota_password`/`web_server_password` in `secrets.yaml`; device reflash to apply the new
  credentials on hardware is **blocked — the ESP32-P4 board is not currently connected**.
- Collapsed the fake three-tier guest/admin/superadmin dashboard model (never backed by any
  server-side role support) to the real two tiers: guest and operator, with login now verified
  against the live device instead of a hardcoded value.
- Rewrote `reference/RBAC_USER_GUIDE.md` and `reference/AUTHENTICATION_GUIDE.md` to match.
- Build re-verified after cleanup: RAM 19.5%, Flash 20.3%. See
  `reference/session_recaps.md` (2026-07-24 entry) for full detail.

## 2026-07-24 Web Dashboard Central Gauge

- Reworked `assets/dashboard.html`'s central display into a 3-arc SVG horseshoe gauge matching
  the LVGL touchscreen (coolroom/setpoint/ambient temp arcs, same colors and percent formula).
  No firmware change (RAM/Flash unchanged). Updated `assets/dashboard_virtual_preview.html` with
  the same gauge and mock data, then published it via Artifact for visual sign-off — renders
  correctly. See `reference/session_recaps.md` (2026-07-24 entries) for detail.

## 2026-07-24 Dashboard Visual Rework

- `assets/dashboard.html` restyled as Home Assistant Lovelace cards (token-based flat surfaces,
  HA Tile-card metric tiles, gauge card header). Removed the guest lock/blur overlay — settings/
  admin sections always visible now, guest gets disabled controls instead. No functional access
  change (per-handler role checks remain the real gate). Preview file and Artifact synced.

## 2026-07-24 Gauge Panel Icon Rail

- `assets/dashboard.html` gauge card now has a left icon rail (compressor/defrost/light/alarm,
  same set/order as LVGL's left sidebar) and top-right Wi-Fi/uptime, replacing the separate tile
  row entirely. Light toggle is a real `/switch/relay_light/toggle` call, gated to operator.
  Alarm stays display-only (no exposed reset endpoint in firmware). Preview + Artifact synced.

## 2026-07-25 Gauge Styled After ESP32-Coolroom-Prescision

- `assets/dashboard.html` gauge restyled after the earlier S3 project's web dashboard
  (`ESP32-Coolroom-Prescision/web/tooltips.js`): hue-matched faded tracks, tick marks, in-SVG
  CURRENT/SET/AMB ring labels, thinner center numeral, dynamic outer-arc recoloring. Kept this
  project's real 3-state (red/blue/green) alarm-relative color logic rather than the old
  project's different 4-state scheme. Preview + Artifact synced.

## Phase Completion Status

| Phase | Description | Status | Completion Date | Build Test | Device Test |
| ----- | ----------- | ------ | --------------- | ---------- | ----------- |
| 1 | WiFi, HA API, Web Server, OTA | ✅ | Previous | ✅ | ✅ |
| 2 | RS485 Modbus (relays, RTD, RTC) | ✅ | Previous | ✅ | ✅ |
| 3 | Control Logic (hysteresis, alarms, defrost) | ✅ | Previous | ✅ | ✅ |
| 4 | LVGL Touchscreen Dashboard | ✅ | 2026-07-18 | ✅ | ⏳ |
| 5 | SD Card, ntfy, Backup/Restore | 🔄 In Progress | Ongoing | ⏳ | ⏳ |

## 2026-07-23 Resume Status Refresh

### Repository State

- Latest completed commit: `bf2547b` (`feat(offline): keep control autonomous without wifi and add offline prep`)
- Main handover and active context were refreshed to match current `HEAD`
- Working tree is now intentionally dirty with repo-owned compile-helper and documentation updates pending commit

### Since The Original Phase 4 Snapshot

- Added animated state visuals for compressor and defrost states
- Added compile environment helpers and ARM64 recovery path
- Added repo-managed dependency-check hooks and Windows bootstrap flow
- Hardened offline autonomous control so Wi-Fi/API loss does not reboot the controller
- Added offline package-prep assets under `tools/offline/`

### Current Resume Position

- The firmware header still marks Phase 5 as the active feature area
- Build metrics are now revalidated: RAM 19.5%, flash 20.2% of the 7 MB OTA slot
- Flash policy has been corrected from `< 20%` to an OTA-slot policy appropriate for this ESP32-P4 partition layout
- Hardware validation remains outstanding for the newest offline/autonomy and SD-card changes

### New Work Completed This Session

- Set explicit `web_server.auth.type: basic` in firmware configuration
- Cleaned local logging warnings in `p4_logging.h`
- Hardened `tools/esphome_compile.sh` to auto-recover the native-IDF reconfigure `src` `REQUIRES` omission for `esp_ringbuf` / `esp_http_server`
- Updated instructions, recap, and handover docs to reflect the corrected ESP32-P4 flash policy

### Recommended Next Check

- Confirm remaining Phase 5 implementation gaps before new scope is added
- Run hardware validation for offline operation and SD-card behavior on the ESP32-P4 target board

## Phase 4 Completion Details (2026-07-18)

### Tasks Completed ✅

**Display Architecture:**

- [x] Horseshoe arc gauge (3 concentric colored arcs)
- [x] Temperature-to-arc conversion formula
- [x] Center temperature display (64pt blue font)
- [x] Setpoint and status labels
- [x] Left sidebar icons (4 total)
- [x] Right sidebar secondary readings
- [x] Color palette (dark theme, 10 colors)

**Icon Visual Feedback:**

- [x] Compressor icon (green when relay ON)
- [x] Light icon (orange when relay ON, touchable)
- [x] Defrost icon (orange when control logic active, supports passive cycles)
- [x] Alarm icon (red when any alarm active, touchable)
- [x] Light icon toggle functionality
- [x] Alarm icon soft reset functionality

**Code & Architecture:**

- [x] Icon color updates via relay handlers (relay-bound)
- [x] Icon color updates via binary sensors (control logic-bound)
- [x] Temperature sensor handlers (probe1 2s, probe3 10s, setpoint on-change)
- [x] Arc value updates with conversion formula

**Documentation:**

- [x] Handover notes with architecture
- [x] Icon visual feedback loop diagram
- [x] Display architecture visual reference
- [x] Session recap entry
- [x] Copilot instructions with post-task protocol
- [x] Code-review graph (mermaid.js)

**Build & Quality:**

- [x] Zero compilation errors/warnings
- [x] RAM 19.4% (healthy)
- [x] Flash 20.2% (comfortable)
- [x] All firmware binaries generated

### Pending Tasks ⏳

**Device Testing:**

- [ ] Flash firmware.factory.bin to ESP32-P4
- [ ] Verify home page layout and rendering
- [ ] Test arc gauge animations
- [ ] Verify icon color changes
- [ ] Test light icon touch toggle
- [ ] Test alarm icon touch reset
- [ ] Extended operation stability test

**Potential Enhancements:**

- [ ] Arc animation/easing effects
- [ ] Setpoint needle (alternative pointer widget)
- [ ] Icon pulse animations on alarm
- [ ] Secondary pages (settings, diagnostics)
- [ ] Nighttime theme

## Phase 5 Roadmap

**Planned Features:**

- SD card CSV logging with event export
- ntfy push notifications for alarms/critical events
- Backup/restore of control logic settings to SD
- Secondary UI pages (settings panel, diagnostics, event log)
- OTA firmware update from web dashboard

### 2026-07-23 Static Audit Notes

- Implemented now: SD mount, daily CSV logging, event logging, SD free-space reporting, SD backup/restore buttons, boot-time restore attempt, ntfy high/low/clear/probe-fault notifications.
- Updated now: backup/restore expanded to all configurable settings (core setpoints, extended control floats, and feature toggles).
- Not present now: web-based log export workflow and OTA firmware update flow from the dashboard.

**Estimated Complexity:** Medium  
**Estimated Timeline:** 2-3 weeks

## Key Metrics Trending

| Metric | Phase 3 | Phase 4 | Target | Status |
| ------ | ------- | ------- | ------ | ------ |
| RAM Usage | 18.9% | 19.4% | < 25% | ✅ |
| Flash Usage | 18.2% | 20.2% | < 6.0 MB app image soft target | ✅ |
| Compilation Time | ~6s | ~7s | < 10s | ✅ |
| Code Quality | Clean | Clean | 0 warnings | ✅ |

**Note:** Flash usage is well within the actual 7 MB OTA-slot budget for this ESP32-P4 partition layout. Future growth should be reviewed once the image exceeds about 5.5 MB, not at an arbitrary 20% threshold.

## Session Execution Quality

**2026-07-18 Session:**

- Post-task protocol steps: 4/4 completed ✅
- Completion checklist: 8/8 items verified ✅
- Documentation completeness: Comprehensive ✅
- Build metrics: Verified and healthy ✅
- Git commits: 3 clear, conventional format ✅

**Code-Review Graph:** Generated (icon control flow diagram)

---

**Last Updated:** 2026-07-23  
**By:** Copilot Agent  
**Status:** Compile flow hardened; Phase 5 still active
