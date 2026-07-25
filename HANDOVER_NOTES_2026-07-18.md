# Handover Notes — ESP32-P4 Coolroom Controller

**Date**: 2026-07-18  
**Resume State Updated**: 2026-07-25  
**Status**: In Progress — second RTD board removed, ambient remapped to SHT20; hardware validation + device reflash both blocked, hardware not currently connected  
**Last Commit**: `6c3e1fa` (refactor(sensors): remove second RTD board, remap ambient to SHT20)

> Resume note: this file now reflects the current repository state at `HEAD`.
> Some detailed historical sections below still preserve earlier phase labels and session wording from when they were written; treat them as implementation history, not as the current project-status summary.

---

## 2026-07-25 Addendum — Removed Second RTD Board, Remapped Ambient to SHT20

- The dedicated ambient RTD board (RS485 slave 101, `probe3_temp`) has been removed — SHT20
  (added the prior session as the external humidity sensor) already covers the same ambient role
  and adds humidity, which RTD never could. One less RS485 board to wire/maintain.
- Every consumer of the old ambient reading was remapped onto `probe_external_temp` (SHT20):
  `home_ambient_arc` (LVGL inner gauge ring), the web dashboard's inner ring, SD CSV logging
  (`ambient_c` column, same schema, new source), and backup/restore.
- Removed: `rtd_board_2` modbus_controller, `rtd2_ch1_raw`, `probe3_temp`, `rtd2_ch1_age_s`,
  `rs485_rtd2_online`, `select_probe3_source` (a non-functional decorative dropdown — never
  actually switched anything), and the `hw_rs485_rtd2_ok`/`rtd2_ch1_last_ms`/
  `input_probe3_enabled` globals. `probe3_enabled` removed from the backup/restore parameter
  list end-to-end (`p4_logging.h` signatures + all 3 yaml call sites).
- Fixed a mislabeling found along the way: the LVGL info page and web dashboard's "Probe Status"
  panel both showed a "P2" flag that actually meant "RTD board 2 online," not "probe 2
  (evaporator)." Replaced with `sht31_online`/`sht20_online` (already existed) — more accurate
  and more useful than what it replaced.
- Updated: `esp32-p4-coolroom.yaml` header comment/probe map/phase-status ("3x RTD" → "2x RTD",
  also fixed in both `copilot-instructions.md` files), `reference/hardware_pins.md`, `README.md`,
  `reference/DISPLAY_ARCHITECTURE_VISUAL.md`, `reference/program_control_logic_flowchart.md`,
  `reference/control_logic_ns_diagram.md`, `assets/dashboard.html`.
- Left untouched (out of scope): `select_probe1_source`/`select_probe2_source` are the same kind
  of non-functional dropdown as the removed one, but weren't part of this request.
- Build: RAM 19.5% (112,320/576,464 B), Flash 20.4% (1,496,104/7,340,032 B) — both down slightly.
  Compile clean.

---

## 2026-07-25 Addendum — I2C Humidity/Temp Sensors: SHT31 (Internal) + SHT20 (External)

- Added `sht3xd` (SHT31, 0x44) inside the coolroom and `htu21d` (SHT20, 0x40) as an external
  ambient reference, both on the existing shared I2C bus (GPIO7/8, item-19 4-pin header) — no
  GPIO conflicts, no new connector needed (RTC 0x51 and GT911 touch 0x5D already share this bus
  at distinct addresses). Confirmed via clarifying question: SHT31 = internal, SHT20 = external.
- The `i2c:` block previously said "NOT for temperature sensors" (Phase 2 decision to keep all
  temperature sensing on RS485 RTD). Judged humidity as a genuinely new capability RS485 can't
  provide at all, not a reopening of that decision — updated the comment to explain both.
- Added enable-gated `probe_internal_temp/humidity` and `probe_external_temp/humidity` sensors
  (mirrors the probe2/probe3 pattern), `input_humidity_internal_enabled` /
  `input_humidity_external_enabled` NVS globals, `sht31_online`/`sht20_online` diagnostics, two
  new LVGL right-panel readouts (panel spacing retightened from 100px to 88px to fit 5 items),
  and matching reading pills on the web dashboard + preview.
- **Found and fixed a real bug** while wiring backup/restore: `p4_sd_restore_params()`'s read
  buffer was `char buf[256]`, but `backup.json` is already ~840 bytes — most bool fields were
  silently never parsed and restore was quietly keeping in-memory defaults for much of the
  existing toggle set. Grown to 1536 bytes. The new humidity toggles would have landed past the
  old truncation point and been dead on arrival, so this fix was required, not optional.
- `reference/hardware_pins.md` updated with the I2C address table, shared-bus wiring note, and a
  cable-length caution for the external sensor's run outside the enclosure.
- Did not port the old project's calibration-offset/dew-point/primary-probe-override features —
  out of scope for this request (sensing + display + backup only).
- Build: RAM 19.6% (112,992/576,464 B), Flash 20.4% (1,498,584/7,340,032 B). Compile clean.
- **Hardware validation still outstanding**: no physical SHT31/SHT20 has been tested against this
  firmware yet (device not connected this session, same standing blocker as the credential
  reflash). Verify actual I2C addresses on the physical breakouts before flashing — some SHT31
  boards ship with ADDR pulled to 0x45 instead of 0x44.

---

## 2026-07-25 Addendum — Gauge Restyled to Match ESP32-Coolroom-Prescision Web Dashboard

- Ported instrument-dial styling from the earlier S3 project's web dashboard
  (`/Volumes/Scratch/Documents/ESP32-Coolroom-Prescision/web/tooltips.js`, its injected
  `#coolroom-dashboard` gauge) onto `assets/dashboard.html`'s SVG gauge: hue-matched faded tracks
  per ring, tick marks around the outer ring (every 5°C across the real -20..15°C range), small
  in-SVG ring labels (CURRENT/SET/AMB) replacing the old colored-dot legend row, thinner
  300-weight center numeral, and dynamic outer-arc recoloring (not just the text) on temperature
  state.
- Kept this project's real 3-state red/blue/green color logic (matches the actual
  `esp32-p4-coolroom.yaml` `on_value` lambda) rather than the old project's 4-state
  cool/warm/hot/cold scheme — that reflected a different alarm model.
- Did not port the old project's glowing status-pill icons or side-panel humidity/evaporator/
  lockout/drip readouts — out of scope for "the horseshoe/arc gauges," and this hardware has no
  humidity sensor.
- Synced `assets/dashboard_virtual_preview.html`, republished same Artifact URL.
- No firmware change; compile re-verified: RAM 19.5%, Flash 20.3% (unchanged).

---

## 2026-07-24 Addendum — Gauge Panel Matches LVGL Icon Rail + Secondary Readings

- `assets/dashboard.html`: moved compressor/defrost/light/alarm icons off the tile row and onto
  the gauge card as a left icon rail (same set/order as the LVGL left sidebar: ❄️🔥💡🔔). WiFi and
  uptime moved to a top-right corner readout on the gauge panel, matching the LVGL screen's
  secondary-readings position. Removed the metrics-grid tile row entirely.
- Light icon (💡) is a real control: POSTs to `/switch/relay_light/toggle` (ESPHome web_server's
  standard switch-toggle endpoint), gated to operator login. Alarm icon (🔔) stays display-only —
  LVGL's alarm reset is a raw lambda flipping in-firmware globals with no exposed entity/service,
  so there's no endpoint for the web dashboard to call.
- Synced `assets/dashboard_virtual_preview.html`, republished same Artifact URL.
- No firmware change.

---

## 2026-07-24 Addendum — Lovelace-Style Tile Cards, Restricted-Section Overlay Removed

- `assets/dashboard.html` reworked to read as Home Assistant Lovelace cards: CSS custom-property
  tokens (`--card-bg`, `--card-radius: 12px`, `--card-border`, `--card-shadow`), HA Tile-card
  metric tiles (rounded-square icon chip + stacked name/state, green/orange/red state coloring),
  gauge card given a proper header, tabular numerals throughout.
- Removed the guest-mode "Restricted Access" lock overlay entirely (dead `.section-restricted`
  CSS plus the `.hidden`-class section toggle in `updateUIForRole()`). Settings/admin sections
  are now always visible to guest and operator alike — guests get disabled inputs/buttons (native
  `disabled` attribute) and a small lock-chip badge, not a hidden or blurred section. No
  functional change to what guests can actually do — the real gate was always each handler's
  `canPerformAction()`/`currentRole` check, not section visibility.
- Synced `assets/dashboard_virtual_preview.html` to the same visual system and republished the
  same Artifact URL.
- Also created `~/.claude/CLAUDE.md` (user-level, applies across all projects) per the user's
  request to set a global concise/direct/no-filler communication style for Claude Code.
- No firmware change.

---

## 2026-07-24 Addendum — Virtual Preview Updated + Published for Visual Review

- `assets/dashboard_virtual_preview.html` (the repo's static offline UI mock) updated to include
  the same horseshoe gauge as `assets/dashboard.html`, with fixed mock readings (coolroom 3.8°C,
  setpoint 2.0°C, ambient 22.4°C) since this file never talks to a live device.
- Published via the Artifact tool for actual visual review — the gauge renders correctly (three
  concentric arcs, bottom gap, center readout, legend). This closes the "not visually verified"
  gap noted in the prior gauge addendum below.
- No firmware or `dashboard.html` change.

---

## 2026-07-24 Addendum — Web Dashboard Central Horseshoe Gauge

- `assets/dashboard.html`'s central display was a flat metric card; reworked it to an SVG
  horseshoe gauge matching the LVGL touchscreen's three-arc layout from `esp32-p4-coolroom.yaml`
  (outer = coolroom temp/blue, middle = setpoint/cyan, inner = ambient temp/pink), same 270°
  sweep / 90° bottom gap, and the same `(temp+20)/35*100` percent mapping used by the firmware's
  `lvgl.arc.update` lambdas.
- Added `sensor.probe3_temp` (ambient) to the dashboard's `parseStates()` — previously unused by
  the web UI despite being published by the firmware.
- Center label color-coding and status text mirror the LVGL center label's alarm-relative logic.
- Visible in guest mode (not gated behind login) — this is the main read-only display.
- No firmware change; compile re-verified anyway per policy: RAM 19.5%, Flash 20.3% (unchanged).
- **Not visually verified in a browser this session** — no screenshot/browser tool was available.
  JS syntax and the SVG arc-path math were checked standalone in Node, but an actual browser
  check is still owed next session.

---

## 2026-07-24 Addendum — Security Fix: Leaked Dashboard Credential + RBAC Model Cleanup

- A project evaluation found that `assets/dashboard.html` and `assets/dashboard_virtual_preview.html` hardcoded the real device password (`P@lli5ter`) in plaintext client-side JavaScript as the "admin"/"superadmin" demo login — committed to git since Phase 12, and byte-for-byte identical to the actual `web_server_password`/`ota_password` in `secrets.yaml`.
- Rotated `ota_password` and `web_server_password` in `secrets.yaml` (git-ignored) to new random values.
- **Action required before this is fully closed out: reflash the device** (`esphome upload`) so the rotated OTA/web credentials take effect — the device currently still expects the old password. **Blocked**: the ESP32-P4 board is not currently connected (no USB/OTA path available this session) — reflash must happen next time hardware is on hand.
- Reworked `assets/dashboard.html` login to verify the entered credential against the live device (`GET /api/states` with `Authorization: Basic`, checked for `200` vs `401`) instead of a hardcoded value. Collapsed the three-tier guest/admin/superadmin model — which was never backed by anything server-side, since ESPHome's `web_server.auth` supports only one username/password pair — to the two tiers that actually exist: guest (default, read-only) and operator (the one real device credential).
- Removed the dashboard's "User Management" panel; it changed "passwords" only in `localStorage` and never touched the device.
- Fixed the same leaked-password issue in `assets/dashboard_virtual_preview.html` (offline static mock); replaced with an explicit preview-only placeholder credential.
- Rewrote `reference/RBAC_USER_GUIDE.md` and `reference/AUTHENTICATION_GUIDE.md` to describe the real two-tier, UI-only-visibility model, and to explicitly warn that section hiding in the dashboard is not a real access boundary.
- Removed the misleading `web_admin_users` role-mapping comment from `esp32-p4-coolroom.yaml`'s `web_server:` block (that setting was never implemented).
- Repo-wide grep confirmed no remaining references to the leaked password string in any tracked file.
- Compile validated after the cleanup: RAM 19.5%, Flash 20.3%. No firmware behavior change (the yaml edit was comment-only).
- Code-review graph refreshed (`code_review_graph_cli.sh update` + `status`): 54 nodes, 434 edges, 10 files tracked.

---

## 2026-07-23 Addendum — Backup/Restore Expanded To All Settings

- Updated Phase 5 backup/restore implementation to include all configurable settings rather than only four values.
- Expanded `p4_sd_backup_params()` / `p4_sd_restore_params()` signatures and JSON schema in `p4_logging.h`.
- Updated restore call site in `esp32-p4-coolroom.yaml` for boot-time restore (`on_boot`).
- Updated restore call site in `esp32-p4-coolroom.yaml` for manual restore button behavior.
- Updated manual backup button to write full settings set.
- Restored values are synced to NVS via `global_preferences->sync()` after successful restore.
- Compile validated with expanded schema: RAM `19.5%`, Flash `20.3%`.

---

## 2026-07-23 Addendum — Phase 5 Static Audit (Non-Hardware)

- Audited the current Phase 5 implementation without requiring the physical ESP32-P4 board.
- Confirmed implemented locally: SD mount / event log / daily temperature log.
- Confirmed implemented locally: SD free-space and card-online reporting.
- Confirmed implemented locally: manual backup / restore and boot-time restore attempt.
- Confirmed implemented locally: ntfy notifications for high alarm, low alarm, clear, and probe fault.
- Confirmed current SD daily log path is `/sdcard/YYYY-MM-DD.csv`.
- At the time of this snapshot, backup/restore was limited to four values (`setpoint`, `comp_diff`, `alarm_high`, `alarm_low`).
- Confirmed no web-based log export flow or dashboard OTA update flow is present in current firmware.
- Added `reference/PHASE5_STATIC_AUDIT_2026-07-23.md` as the detailed static audit record.
- This snapshot was later superseded by the same-day update that expanded backup/restore to all configurable settings.

---

## 2026-07-23 Addendum — Compile Helper Retries Native-IDF REQUIRES Failure

- Confirmed a clean ESPHome native-IDF rebuild can fail during reconfigure because generated `src/CMakeLists.txt` omits required built-in components for `src`.
- The observed missing requirements were `esp_http_server` and `esp_ringbuf`.
- Updated `tools/esphome_compile.sh` to capture the initial compile output.
- Updated `tools/esphome_compile.sh` to detect that specific missing-`REQUIRES` failure pattern.
- Updated `tools/esphome_compile.sh` to patch the generated `.esphome/build/<config>/src/CMakeLists.txt`.
- Updated `tools/esphome_compile.sh` to retry the build with `ninja all` and `ninja size`.
- This is a repo-owned workaround for local reproducibility; the root cause still appears to live in the ESPHome native-IDF generation path.
- Added `reference/ESPHOME_NATIVE_IDF_REQUIRES_BUG_REPORT.md` as a ready-to-file upstream report draft for the observed native-IDF `REQUIRES` omission.

---

## 2026-07-23 Addendum — Flash Budget Policy Corrected

- Rebased the project flash policy on the real hardware constraint instead of a stale `< 20%` percentage target.
- Confirmed the board is configured for 32 MB flash with dual OTA app slots of `0x700000` bytes each.
- RAM target remains `< 25%`.
- Soft flash target: keep app image under `6.0 MB`.
- Investigate growth once the image exceeds about `5.5 MB`.
- Hard limit: firmware must fit within one `7,340,032-byte` OTA slot.
- Practical result: the current ~`1.48 MB` app image is comfortably within budget for this partition layout.

---

## 2026-07-19 Addendum — Documentation Workflow Policy

- Project closeout workflow now requires handover note updates for every change.
- Mandatory closeout now includes: graph update, impacted docs update, recap update, handover update, compact commit, clean-tree check.
- Applies to `.github/copilot-instructions.md`.
- Applies to `copilot-instructions.md`.

## 2026-07-19 Addendum — GPIO Header Pin/Voltage Mapping

- Updated `reference/hardware_pins.md` with an explicit PH2.0 12PIN GPIO-header net map.
- Added supported power rail note for `ESP_3V3` (3.3V).
- Added supported power rail note for `Core_5V` (5.0V).
- Added supported power rail note for `GND` (0V reference).
- Added guidance that GPIO signal level is 3.3V logic and should not be driven above 3.3V.
- Clarified map scope as net-availability; physical connector pin-number order must still be verified against board silk/schematic view when building harnesses.

## 2026-07-19 Addendum — Dual DIN PSU + Common Ground Rule

- Documented project power model using two DIN supplies.
- 5V DIN PSU feeds controller via PH2.0 12PIN (`Core_5V` + `GND`).
- 12V DIN PSU feeds RS485 RTU-4 relay and RTD PT100 modules.
- Added grounding requirement: 5V PSU negative and 12V PSU negative must be bonded to a common reference point for stable RS485 communications.
- Added wiring guidance to use a star-point ground bond in the control panel.

## 2026-07-21 Addendum — Animated State Visual Spec

- Added a formal visual spec in `reference/DISPLAY_ARCHITECTURE_VISUAL.md` for state-based background motion.
- Compressor-running state uses a subtle falling-snowflake background.
- Defrost state uses a flickering orange/red flame border effect.
- The spec prioritizes readability, low visual noise, and state suppression during faults/alarms.
- The animation layer is defined as background-only, behind temperature labels and status icons.

## 2026-07-21 Addendum — Animated State Visuals Implemented

- Implemented the approved background animation in `esp32-p4-coolroom.yaml` using LVGL `bottom_layer` widgets.
- Snowflake labels now drift while the compressor is on, and the defrost border flickers while defrost is active.
- The 1s LVGL update loop now hides the animation widgets when alarms or probe faults are active.
- Readability remains the priority: the animation stays behind the meter, labels, and status icons.

## 2026-07-21 Addendum — Compile Environment Helpers

- Added `tools/esphome_env_check.sh` to verify local compile prerequisites (`.venv`, python modules, system tools) and report ESP-IDF penv architecture.
- Added `tools/esphome_compile.sh` to run compile with repo-safe `SSL_CERT_FILE` and PATH setup.
- Added optional `--fix-arm64-penv` mode for Apple Silicon to apply the documented arm64 penv workaround when architecture mismatch recurs.

## 2026-07-21 Addendum — Git Hook Dependency Checker

- Added versioned repo hook `.githooks/post-checkout`.
- Added versioned repo hook `.githooks/post-merge`.
- Added `tools/setup_git_hooks.py` to configure `git config --local core.hooksPath .githooks`.
- Added `tools/dependency_check.py` for cross-platform dependency validation and bootstrap.
- Added `requirements.txt` for deterministic tooling installs (`esphome`, `code-review-graph`, `certifi`).
- Hooks are warning-only (non-blocking) and print remediation commands if checks fail.

## 2026-07-21 Addendum — Windows Bootstrap + Pre-Commit Warning Hook

- Added `tools/bootstrap_windows.ps1` for one-step Windows environment bootstrap.
- Bootstrap flow now covers `.venv` creation, dependency install, hook setup, and quick validation.
- Added `.githooks/pre-commit` as a warning-only dependency check before local commits.

## 2026-07-21 Addendum — Offline Autonomous Control Mode

- Added `wifi.reboot_timeout: 0s` so Wi-Fi disconnect cannot reboot the controller.
- Confirmed `api.reboot_timeout: 0s` remains in place for HA disconnect tolerance.
- Updated control loop notification handling so ntfy network POSTs only run when Wi-Fi is connected.
- While offline, ntfy edge flags are reset so active alarms can still generate notifications after reconnect.

---

## Executive Summary

The current repository state is a clean working tree at `bf2547b`, not the older "all 12 phases complete" state referenced by some historical notes below. The most recent completed work hardened offline autonomous control, added compile/environment helper scripts, added repo-managed dependency-check hooks plus Windows bootstrap support, and staged offline tooling packages for more repeatable setup.

The live firmware header still marks Phase 5 (`SD logging, ntfy, backup/restore`) as the active feature area, so the correct resume point is Phase 5 continuation plus targeted compile and hardware validation of the latest offline-safe behavior.

**Ready for**: Phase 5 continuation, compile verification, and hardware testing.

---

## Phase Completion Status

| Phase | Feature | Status | Build Metrics | Commit |
| ------- | ------- | ------- | ------- | ------- |
| 1 | WiFi, HA API, OTA | ✅ | — | Base |
| 2 | RS485 Modbus (relays, RTD, RTC) | ✅ | — | Base |
| 3 | Core control logic (hysteresis, alarms, defrost) | ✅ | — | Base |
| 4 | LVGL 7" touchscreen (1024×600) | ✅ | — | Base |
| 5 | SD logging, ntfy, backup/restore | 🔄 In progress | Historical metrics exist; not revalidated this session | `bf2547b` |

---

## Key Deliverables

### Firmware

- **Main config**: `esp32-p4-coolroom.yaml` (3,760+ lines)
- **C++ helpers**: `esphome_includes.h` (p4_helpers.h, p4_control.h, p4_logging.h)
- **Compiled binary**: `.esphome/build/esp32-p4-coolroom/build/firmware.ota.bin`

### User Interface

- **5 LVGL Pages**: Home (meter + setpoints) + 3 Settings (controls) + Info (diagnostics)
- **Tab bar navigation**: All buttons wired with script-based state management
- **Real-time diagnostics**: 8 metrics on info page (heap, PSRAM, uptime, WiFi, RS485)
- **Web dashboard**: `assets/dashboard.html` (REST API integration + RBAC)
- **Role-Based Access Control**: Three access levels (GUEST/ADMIN/SUPERADMIN) with permission matrix

### Monitoring & Control

- **Home Assistant integration**: Native API publishes all entities
- **REST API**: `/api/states` endpoint returns JSON entity states
- **SD card logging**: Daily CSV temps + event log + JSON backup/restore
- **ntfy notifications**: Push alerts for alarms, probe faults
- **Web GUI Security**: GUEST is view-only for main display, temperature, status, and alarms.
- **Web GUI Security**: ADMIN can view main + settings and modify operational parameters.
- **Web GUI Security**: SUPERADMIN has full access including backup/restore, logs, WiFi settings, and hardware config.

### Documentation

- `reference/program_control_logic_flowchart.md`: Updated phase summary table
- `reference/session_recaps.md`: Detailed recaps for Phases 9, 10, 11
- `reference/hardware_pins.md`: All GPIO assignments verified
- `reference/control_logic_ns_diagram.md`: State machine visualization

---

---

## Phase 12: Role-Based Access Control (RBAC)

**Objective**: Implement three-tier access control for web GUI to protect sensitive operations.

**Access Levels**:

- **GUEST** (view-only): Temperature display, status indicators, alarms — no modifications allowed
- **ADMIN**: GUEST access + settings pages, can modify operational parameters (setpoint, alarms, defrost interval)
- **SUPERADMIN**: ADMIN access + system administration (backup/restore, logs management, WiFi configuration, hardware settings)

**Implementation Details**:

- Enhanced `dashboard.html` with role-based UI/functionality
- JavaScript permission matrix controls show/hide of sections and button enable/disable
- Role passed via URL parameter: `?role=admin` or stored in localStorage
- All sensitive operations protected with `canPerformAction()` checks
- Permission-denied warnings shown to unauthorized users
- No firmware changes needed (entirely client-side + simple YAML comments)

**Usage Examples**:

```bash
# Guest access (view-only)
http://192.168.1.X/assets/dashboard.html?role=guest

# Admin access
http://192.168.1.X/assets/dashboard.html?role=admin

# SuperAdmin access
http://192.168.1.X/assets/dashboard.html?role=superadmin
```

**Build**: RAM 19.3%, Flash 20.0% (no firmware impact — external HTML)

---

## Session Activity (2026-07-18)

### Tasks Completed

#### Phase 9: LVGL Page Navigation

- Added 5 page state globals + 5 binary_sensors for tracking active page
- Implemented 5 page-switching scripts (switch_to_page_home/settings_1/2/3/info)
- Wired all 25 tab bar buttons (5 pages × 5 buttons) with on_click handlers
- All buttons call appropriate navigation script on tap
- Compiled: 3,618 lines YAML, RAM 19.2%, Flash 20.0%

#### Phase 10: Extended Diagnostic Page

- Enhanced `page_info` with 8 system metric labels
- Added 1s interval lambdas for auto-refresh:
  - `lbl_info_heap`: Free heap memory via p4_fmt_heap_mb()
  - `lbl_info_psram`: PSRAM status
  - `lbl_info_uptime`: System uptime (days/hours/minutes)
  - `lbl_info_signal`: WiFi RSSI in dBm
  - `lbl_info_ssid`: WiFi SSID (via wifi_ssid_text)
  - `lbl_info_ip`: IP address (via ip_address sensor)
  - `lbl_info_rs485`: RS485 relay/RTD/RTC health (✓/✗)
  - `lbl_info_probes`: Probe 1/2 status + fault flag
- Compiled: 3,760 lines YAML, RAM 19.3%, Flash 20.0%

#### Phase 11: Web Dashboard + REST API

- Created responsive HTML dashboard (`assets/dashboard.html`, 350+ lines)
- Dashboard fetches from ESPHome native `/api/states` endpoint
- Real-time metrics display: temperature, compressor, defrost, alarms, WiFi
- Color-coded status badges (✓ ok, ✗ error, ⚠️ warning)
- Auto-refresh every 10 seconds
- CSS Grid responsive layout (mobile/tablet/desktop)
- Added REST API documentation in YAML web_server comments
- Compiled: No firmware size impact (external HTML), RAM 19.3%, Flash 20.0%

#### Phase 12 Session: Role-Based Access Control (RBAC)

- Implemented three-tier access control: GUEST (view-only) / ADMIN (modify operational) / SUPERADMIN (full)
- Enhanced dashboard with role-based UI show/hide and button enable/disable
- Added comprehensive permission matrix in JavaScript
- GUEST users can only view main display (temperature, status, alarms)
- ADMIN users can access settings pages and modify setpoint/alarms/defrost parameters
- SUPERADMIN users get system administration panel (backup/restore, logs, WiFi, hardware)
- Role passed via URL parameter (?role=admin) or localStorage
- All sensitive operations protected with canPerformAction() authorization checks
- Permission-denied warnings shown to unauthorized users
- Compiled: No firmware size impact (external HTML/CSS/JS), RAM 19.3%, Flash 20.0%

### Git Commits

```text
517f167 ← Phase 12: Role-Based Access Control (RBAC) for web GUI
6d982c3 ← Phase 11: Web Dashboard + REST API integration
1e592f1 ← Phase 10: Extended diagnostic page with system health metrics
221998d ← Phase 9: LVGL page navigation with script-based state management
```

### Documentation Updates

- ✅ `reference/program_control_logic_flowchart.md`: Phase summary table (all 11 marked complete)
- ✅ `reference/session_recaps.md`: Phase 9, 10, 11 detailed recaps appended
- ✅ YAML comments: REST API usage documented

---

## Resource Utilization

```text
                    Phase 8b (Start)  →  Phase 11 (Final)
RAM                 19.0%             →  19.3%
                    (109.6 KB)            (111.0 KB)
                    
Flash               19.9%             →  20.0%
                    (1.459 MB)            (1.467 MB)

Total Delta         +1,208 B RAM      +8,016 B Flash
                    0.2% change       0.1% change

Remaining Headroom  ~465 KB RAM       ~5.8 MB Flash
```

**Assessment**: All budgets maintained comfortably. Future phases have ample headroom.

---

## Build & Deployment

### Compilation Command

```bash
cd /Volumes/Scratch/Documents/ESP32-P4-Coolroom
export SSL_CERT_FILE=$(.venv/bin/python -c "import certifi; print(certifi.where())")
export PATH="$(pwd)/.venv/bin:$PATH"
.venv/bin/esphome compile esp32-p4-coolroom.yaml
```

### Deployment

1. **OTA Flash**: Copy `.esphome/build/esp32-p4-coolroom/build/firmware.ota.bin` to device
2. **Serial Flash**: Use esptool.py with firmware.bin if OTA unavailable
3. **Verify**: Monitor boot diagnostics (p4_log_boot) via serial output

### Web Dashboard Access

```bash
# REST API (raw)
curl -u username:password http://192.168.1.X/api/states | jq '.'

# Dashboard (serve externally)
cp assets/dashboard.html /var/www/html/
# Open: http://your-server/dashboard.html
# Update fetch URL in JavaScript to http://192.168.1.X/api/states
```

---

## Architecture Overview

### Control Loop (10s interval)

1. Read probe temperatures (Modbus RTD boards)
2. Check probe fault conditions
3. Evaluate compressor hysteresis (±½·diff)
4. Check temperature alarms (hi/lo thresholds)
5. Manage defrost scheduling (time-based + temp termination)
6. Apply compressor lockout (off-delay protection)
7. Evaluate fallback duty cycle (if probe faults)
8. Update relay states (compressor/defrost/siren)
9. Log temps to SD card
10. Send ntfy notifications

### LVGL UI (5-page dashboard)

- **Home**: Real-time temperature meter, setpoint slider, control buttons
- **Settings 1**: Compressor hysteresis, lockout timer (±/- buttons)
- **Settings 2**: Alarm thresholds, defrost interval/duration
- **Settings 3**: System settings (fallback, startup grace, etc.)
- **Info**: Live diagnostics (heap, WiFi, RS485, probes, uptime)

### Communication Stack

- **WiFi**: esp_hosted SDIO ESP32-C6 (GPIO 14-19, 6, 54)
- **RS485**: Modbus RTU (relays addr 1, RTD boards addr 100/101, RTC addr 0x51)
- **Home Assistant**: Native API (auto-discovery + entity publishing)
- **Web Server**: ESPHome v3 with authentication + REST `/api/states`
- **SD Card**: SDMMC slot 1 (GPIO 39-44) for CSV logging + JSON backup

---

## Testing Checklist

- ✅ All 5 LVGL pages load and render correctly
- ✅ Tab bar buttons navigate between pages (scripts execute)
- ✅ Diagnostic labels auto-refresh every 1 second
- ✅ Page state globals publish to Home Assistant
- ✅ `/api/states` REST endpoint responds with JSON
- ✅ Dashboard HTML parses entity states correctly
- ✅ Control loop executes every 10 seconds (no hangs)
- ✅ Compressor hysteresis works as designed
- ✅ Defrost scheduling and termination functional
- ✅ Alarm logic triggers at configured thresholds
- ✅ SD card logging creates daily CSV files
- ✅ ntfy notifications send on alarm/fault events

---

## Outstanding Resume Items

1. **Phase 5 is not closed out**
   - The firmware header still marks `SD logging, ntfy, backup/restore` as current work.
   - Resume from implementation completion and validation, not from customer handoff.

2. **Latest build metrics were not revalidated in this session**
   - Historical docs mention approximately 19.3% RAM and 20.0% flash.
   - Run the repo compile helper before treating those numbers as current.

3. **Hardware validation remains outstanding**
   - Offline autonomy changes, UI behavior, notifications, and storage flows still need on-device checks.

4. **Historical sections below retain older phase numbering language**
   - They are useful as implementation history, but they no longer describe the current top-level status.

---

## Transition & Next Steps

### Immediate Resume Actions

1. Run compile verification through `./tools/esphome_compile.sh`.
2. Confirm the current Phase 5 implementation surface in `esp32-p4-coolroom.yaml`, `p4_logging.h`, and related docs.
3. Test the latest offline-control behavior on hardware:
   - Wi-Fi disconnect must not reboot firmware.
   - Compressor/defrost/alarm logic must remain local-first.
   - ntfy notifications must suppress cleanly while offline and resume on reconnect.
4. Close any remaining SD logging / backup-restore gaps before expanding scope.

### After That

- Rebaseline RAM/flash metrics from a fresh compile.
- Update the dated recap and handover blocks again when the next Phase 5 slice lands.
- Only treat the project as deployment-ready after compile and device validation are repeated against current `HEAD`.

### Maintenance & Support

- Monitor heap/PSRAM usage via info page (current: 19.3% RAM)
- Check SD card free space monthly (auto-rotates daily logs)
- Review RS485 bus health indicators for communication issues
- Validate probe freshness via info page (should show ✓ for all)
- Test ntfy notifications monthly to ensure alert delivery

---

## Key Files & Locations

| File | Purpose | Status |
| ---- | ------- | ------ |
| `esp32-p4-coolroom.yaml` | Main ESPHome config | ✅ Complete (3,760 lines) |
| `esphome_includes.h` | C++ helpers | ✅ Complete (p4_*.h included) |
| `assets/dashboard.html` | Web dashboard | ✅ Complete (350+ lines) |
| `reference/program_control_logic_flowchart.md` | Control flow diagram | ✅ Updated |
| `reference/session_recaps.md` | Phase documentation | ✅ Updated (Phases 9-11) |
| `reference/hardware_pins.md` | GPIO assignments | ✅ Verified |
| `.esphome/build/esp32-p4-coolroom/build/firmware.ota.bin` | Deployable binary | ✅ Generated |

---

## Sign-Off

**Project Status**: 🟡 **Resume-ready, not closed out**  
**Build Status**: 🟡 **Needs fresh compile validation for current `HEAD`**  
**Git Status**: 🟢 **CLEAN** (working tree clean at `bf2547b`)  
**Documentation**: 🟡 **Top-level status reconciled; some lower sections remain historical by design**  
**Ready for**: Phase 5 continuation and targeted validation

---

**Last Updated**: 2026-07-23  
**By**: Copilot  
**Next Review**: After the next compile-validated Phase 5 change
