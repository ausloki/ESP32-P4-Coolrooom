# Progress Tracking

## 2026-08-01 — Per-phrase audio preview on LVGL + web

- `preview_*` scripts / `btn_preview_*` buttons play a single clip regardless of master
  enable, per-phrase toggle, or soft-mute.
- LVGL Settings 8/8: Toggle 240 → 150 px, plus a 56 px mdi-volume-high speaker button per
  Speak row (`\U000F057E` added to `font_mdi`). Web/preview use a 40 px 🔊 button.
- Manuals updated; coverage, embed, compile, NVS-safe flash done.

## 2026-08-01 — Closeout: probes fixed, 2CH RTD sheets, log recommend tools

- Removed Probe Source toggles; CH1/CH2 roles fixed in firmware + manuals.
- Digested 2CH PT100 Modbus sheets into `hardware_pins.md` + `reference/PT100-RS485-2CH-*`.
- Added LAN log pull/analyze/recommend scripts and Win/mac double-click launchers.
- Closeout: coverage, embed, compile, NVS-safe flash, graph, push.

## 2026-08-01 — Fixed Probe 1/2 RTD roles (no source swaps)

- CH1 = coolroom room air, CH2 = evaporator — documented in `hardware_pins.md`.
- Removed Probe Source selects from LVGL/web/preview and firmware entities/globals.
- Probe 2 Enabled remains. Coverage OK; NVS-safe flash done.

## 2026-08-01 — Evening closeout (glass confirm + commit)

- Cory confirmed LVGL Settings **1/8–8/8** menu present on glass.
- Bundled closeout: °C/°F, LVGL settings parity, Info glyphs/SD%/NTP latch, manuals,
  coverage + compile + NVS-safe flash already done earlier this evening.

## 2026-08-01 — Info page glyphs / SD percent / NTP latch

- Added `|` and `—` to `font_small/medium/large` (were rendering as tofu boxes).
- Info SD line: total + used/free MB with percentages + backup presence (used % to
  two decimals so nearly-empty cards show e.g. `0.04%`).
- Fixed self-clearing `sntp_get_sync_status()` race by latching sync state in
  `p4_helpers.h`; Info shows last-sync age. Poll cadence: 60 s → weekly after sync.

## 2026-08-01 — LVGL settings parity (8 pages)

- Fixed page-1 Fan Relay / Fallback OFF overlap; added Comp Min Run.
- Added Skip-If-Cold / Force-Max (p2), Frost Rate (p3), Ice enable+dwell (p5),
  Probe sources + Calibrate (p7), full Audio page 8/8.
- Info: SD detail + Backup/Restore; full SD directory stays web-only.
- Web-only by design: Wireless change, Events, ntfy text/priorities.
- Coverage OK; compile clean; NVS-safe flash `/dev/cu.usbmodem213401`.

## 2026-08-01 — Celsius / Fahrenheit display preference restored

- Added NVS-restored **Temperature Display Unit** select (Celsius default / Fahrenheit).
- Published to HA; added to LVGL Settings 7/8 and web Probes & Sensors.
- LVGL/web temperature readouts convert without changing Celsius control math, logs,
  stored thresholds, or native-API sensor payloads.

## 2026-08-01 — Outstanding-items triage (log download / auth / mic)

- Closed: web SD log download (confirmed working); multi-user server auth (not required).
- On hold: microphone / voice input (no mic; may not ship).
- Still deferred: LVGL audio toggles, dashboard OTA UI.
- Still open: RS485+RTD bench bring-up, manual screenshot placeholders.

## 2026-08-01 — Can-do-now punch list closed

- Synced `assets/dashboard_virtual_preview.html` to live Coolroom Status (no pink strip,
  rotating fault labels, full Audio + Alarms & Notify ADVANCED_SETTINGS incl. Hardware
  Offline Priority).
- Confirmed SD `opendir`/`readdir` already fixed via `disable_vfs_support_dir: false`.
- Refreshed `HANDOVER_NOTES_2026-07-31.md` + `activeContext.md`; device spot-check OK
  (Status rotating humidity offline; no guest gate / alarm strip in embedded page).
- Manual screenshots still placeholders. Carel A/B/C/D closed 2026-08-01 (`reference/CAREL_CONTROL_DECISIONS.md`).

## 2026-08-01 — Hardware-offline ntfy + afternoon closeout

- ntfy now covers RELAY/TEMP BOARD and HUMIDITY/AMBIENT SENSOR OFFLINE (rising edge)
  plus matching ONLINE recoveries; shared **Hardware Offline Priority** (default urgent).
- Same conditions as rotating centre status, spoken alerts, and SD event log.
- Manuals (§4.8 + Quick Start alert table) and dashboard Alarms & Notify updated.
- Closeout: coverage OK, compile `0x8a9c1893`, NVS-safe USB flash, graph update, compact
  commit. DIRAM ~27.2% / app ~2.77 MB. Persistence smoke OK on retry; hardened
  `wait_published` against transient post-boot GET errors (false FAIL).

## 2026-07-31 — HA Gate, Probe Live UI, Header Clock, Bell Soft-Mute

- Opt-in **Home Assistant API Enabled** (default OFF) drops native API clients when off.
- Probes tab live Raw/Offset/Corrected; `probe1/2_temp_raw` published.
- LVGL header: date left, 12h time right; web pink alarm strip removed (gauge-only).
- Bell soft-mute: stay red / stop jiggle; new alarm type re-animates (`ctl_alarm_silenced_mask`).
- Local schematic/manual PDFs mandated in hardware cursor rule + CLAUDE.md.
- Closeout: compile OK (`0xfa32a15e`), flashed USB, graph update, compact commit.
- RAM ~26.6% DIRAM / Flash ~2.52 MB app — under OTA slot ceiling.

## 2026-07-30 (Late Evening) SD Power Fix, Touch Fix, Icon Animations

- User installed a physical SD card and reported touch not registering input. Both root-caused
  by cross-referencing Espressif's official esp32_p4_function_ev_board BSP source rather than
  guessing (same method that found the three boot bugs on 2026-07-29).
- SD card was never actually powered: this board's high-speed SDMMC pins draw card I/O power
  from on-chip LDO channel 4, per Waveshare's own SD example — our mount code never initialized
  it. Fixed with a lazily-initialized sd_pwr_ctrl_handle_t set before every mount.
- Touch was missing its own address-strap reset pulse: wrongly assumed only one component could
  safely drive the shared display/touch reset pin. Espressif's BSP has the touch driver do its
  own reset (after the display's) specifically to strap the GT911 I2C address correctly. Fixed
  by re-adding reset_pin to the touchscreen config + allow_other_uses: true on both usages
  (ESPHome validates pin exclusivity by default) + matching mirror_x/mirror_y transform.
- Added procedural icon animations (snowflake spin+flicker, flame flicker, light glow, bell
  jiggle), each gated on real relay/alarm state. Checked PPA/32-bit color hardware acceleration
  first — hardware and raw ESP-IDF support it, but ESPHome's lvgl: component currently
  hard-blocks both for ESP32-P4 (not needed for this ask; software rendering is sufficient).
- Two real compile bugs found and fixed: lambda code can't see LVGL's complete private struct
  (lv_obj_get_width/height failed; switched to lv_pct(50) for pivot instead), and these icon
  labels are raw lv_obj_t* globals, not wrapped in ESPHome's usual LvCompound helper — id()
  already returns the pointer directly, no .obj suffix needed.
- Build: RAM 22.1%, Flash 21.6%, config hash 0x9cf2e4ce. Flashed and hash-verified. Nothing in
  this pass confirmed on hardware yet (touch, display integrity, SD status, animations all
  pending visual check).

## 2026-07-30 (Evening) Arc Orientation Bug Fixed, Live-Hardware Gauge Tuning

- Resumed the interrupted afternoon session (below) — it never completed closeout, so this entry
  also reconstructs/documents that work for the first time (see `reference/session_recaps.md` and
  `HANDOVER_NOTES_2026-07-18.md` for full narrative).
- Root-caused the horseshoe orientation bug the afternoon session kept flip-flopping on: LVGL
  angles are clockwise from 0°=3 o'clock, so `330/210` (believed "bottom-opening") actually
  centers the gap at 12 o'clock — mathematically wrong, not just a preference call. Fixed to
  `150/30`, which also resolved a second latent bug: the tick marks had been correctly positioned
  for bottom-opening the whole time, silently fighting the wrongly-oriented arcs. Ticks later
  removed entirely per operator feedback.
- Five live flash-and-observe rounds tuned gauge diameter/spacing directly against the physical
  screen: zero-gap → real gaps → ~3.5mm smaller → ~2mm smaller (twice) → outer+middle grown back
  +1mm. One round required holding the innermost ring back from a requested reduction to avoid
  hiding it behind the fixed center circle (corrected a wrong assumption about how LVGL draws
  `border_width` — inside the box, not outside).
- Left icon rail resized twice (doubled, then reduced 25%) with even spacing recomputed each time.
- Confirmed (not yet fixed) a real bug: zero app-level log output ever reaches the USB console
  across three independent, properly-reset captures, despite the device reliably booting to a
  working UI.
- Fixed a structural bug in `HANDOVER_NOTES_2026-07-18.md` where the afternoon session's
  insertions had severed a prior addendum's heading from its own body text.
- Build stable throughout: RAM 22.1%, Flash 21.6%.

## 2026-07-30 Emergency Recovery (No-UI Cyan Flash State)

- User reported near-total UI loss with occasional cyan flashes.
- Live serial logs captured and confirmed hosted Wi-Fi path assert/reset loop (`H_API link not yet up`, queue semaphore assert) as active failure mode.
- Removed delayed boot action that re-enabled Wi-Fi after 30s, keeping hosted stack disabled for runtime stability.
- Recovery firmware flashed successfully (`config_hash=0xb6918970`; RAM 22.1%, Flash 21.6%).

## 2026-07-30 UI Refinement Pass 4 (Bell Icon, Uniform Ring-Constrained Arcs, Dense Ticks)

- Increased left icon size again and preserved uniform vertical spacing.
- Switched alarm icon to bell glyph (not alarm-ringer variant).
- Rebuilt arc geometry to uniform 12px widths and evenly spaced diameters within grey donut bounds.
- Restored bottom-open orientation (`330 -> 210`) and replaced sparse ticks with dense incremental radial ticks along horseshoe sweep.
- Reinforced no-scroll behavior at page and center-container levels.
- Added LVGL refresh tuning (`full_refresh`, `update_when_display_idle`) targeting intermittent cyan flashing.
- Build/flash successful (`config_hash=0x4a9b2d6a`; RAM 22.1%, Flash 21.6%).

## 2026-07-30 UI Regression Recovery Hotfix (after Pass 3)

- User reported pass-3 regressions: no visible MDI icons, upside-down horseshoe, right slider still present.
- Root cause: center arc container footprint overlapped left icon rail after full-width expansion.
- Hotfix: constrained center container (`x:80`, `width:864`), disabled center container scroll/scrollbar, restored arc direction (`150 -> 30`), and moved right-side labels inward.
- Build/flash successful (`config_hash=0xf2bbfb49`; RAM 22.1%, Flash 21.6%).

## 2026-07-30 UI Correction Pass 3 (Icon-Only, Centered Equal-Width Arcs, Ticks, No-Scroll)

- Left rail converted to icon-only touch controls: removed visible boxes and removed text labels.
- Increased left icon size (`font_mdi_large` set to 42).
- Centered the arc zone on full width (`x:0`, `width:1024`) and aligned right panel labels for this geometry.
- Set all three reading arcs to equal width (`12`) and restored bottom-opening horseshoe (`330 -> 210`).
- Added perimeter clock-style tick marks around the horseshoe.
- Forced `page_home` scrollbar off and retained in-bounds layout to eliminate right-side slider.
- Disabled background motion FX updates to reduce intermittent cyan flashing.
- Build/flash successful (`config_hash=0xaff3307d`; RAM 22.1%, Flash 21.6%).

## 2026-07-30 UI Correction Pass 2 (Duplicate MDI Icons Removed, Arc Direction Fixed)

- Removed the duplicated inner MDI icon set from the center gauge area.
- Kept single left icon rail and increased icon size via new `font_mdi_large`.
- Moved `ui_*_icon` IDs to the left rail so existing runtime color-state updates target the visible icons.
- Updated left rail border colors (compressor blue, defrost orange, light grey, alarm red).
- Flipped horseshoe arc geometry to `start_angle: 150` / `end_angle: 30`.
- Changed `Int` reading color from cyan to white to reduce cyan emphasis/flicker perception.
- Validated, compiled, and flashed successfully (`config_hash=0x2b5e9132`; RAM 22.1%, Flash 21.6%).

## 2026-07-30 Home Screen Full-Screen Fit (No Scroll) + Screenshot Alignment

- Followed hardware feedback and adjusted the LVGL home screen to be fixed-view, no-scroll on the 1024x600 panel while keeping bottom `Home / Settings / Info` tabs unchanged.
- Found and corrected layout extents likely causing scroll: right-column labels and hidden status object were out-of-bounds; left status icon offsets were tightened in-bounds.
- Set explicit `page_home` dimensions (`width: 1024`, `height: 600`).
- Preserved previous visual fixes (MDI icon font, compressor/defrost order swap, bottom-opening centered horseshoe arcs).
- Config validated and firmware flashed successfully.
- Build stats after pass: RAM 22.1%, Flash 21.6%, config hash `0x7935bd9d`.

## 2026-07-30 Hosted-Link Isolation And Boot-Loop Boundary

- Ran controlled hosted-link sweeps: SDIO frequency (40/20/10 MHz), SDIO bus width (4-bit vs
  1-bit), and wake pulse sequencing. No variant removed the reboot loop.
- Consistent pattern remained: early `H_API` link-not-up events followed by `SW_CPU_RESET` and
  safe-mode fallback.
- Critical isolation test: `wifi.enable_on_boot: false` produced zero resets and no safe-mode
  entry in the same observation class, isolating failures to hosted/Wi-Fi startup path rather
  than LVGL/control logic.
- Added diagnostic delayed Wi-Fi enable (30s post-boot) to pinpoint whether resets begin exactly
  at runtime Wi-Fi activation.
- Host-side network check: `192.168.37.237` resolved to `dc:1e:d5:96:3e:d8` and responded to
  ping, while ports 80 and 6053 were refused at test time.

## 2026-07-26 Door Light Feature, All-Alarm ntfy, Entity Rename, 2 Items Closed by Decision

- Resolved 5 outstanding items in one pass: 2 closed by user decision (no code), 3 implemented.
- Closed: four Carel-comparison divergences (behavior confirmed as wanted); the two-tier
  guest/operator RBAC model (confirmed as the permanent design, not a stopgap — already
  matched what the user described).
- Door-triggered light is now its own feature: decoupled from `door_sensor_mode_light_control`
  (renamed `door_sensor_mode_nc_no`, no longer touches the light), new independent
  `sw_door_light_enabled` switch (default on) gates the reed sensor's light-on-open/off-close
  behavior.
- Found and fixed a second bug while in that code: `input_door_sensor_enabled` (door-alarm
  master switch) was never checked in the control tick — the door-open alarm ran regardless
  of its state. Fixed at the source and defensively in the control tick.
- New setting threaded through backup/restore (all 3 call sites), LVGL Door page, both web
  dashboard files.
- All alarm types now push to ntfy (door/no-cool/ice were event-log only before) — three new
  scripts matching the existing high/low/probe-fault pattern, reusing the generic "cleared"
  push on recovery.
- Renamed `sw_defrost_drip`'s name to remove a reserved `/` character (ESPHome deprecation
  warning → confirmed gone from build output).
- First real use of the new documentation-consistency closeout rule: updated both user-facing
  manuals to match (removed a now-fixed "known quirk" callout, added new setting rows,
  updated the ntfy table, added a permanent-design statement for the RBAC section).
- Build: RAM 21.4%, Flash 21.6%. Compile clean. Untested on hardware.

## 2026-07-26 User Manual + Quick Start Guide (Documentation Only, No Firmware Changes)

- Wrote `reference/USER_MANUAL.md` (full reference) and `reference/QUICK_START_GUIDE.md`
  (condensed + worked examples), covering every device function by group with NS-style
  (Nassi–Shneiderman) structograms for non-trivial control decisions.
- Extracted the settings inventory (34 entities: ranges/defaults/groups) directly from
  `esp32-p4-coolroom.yaml` and re-read `p4_control.h` in full rather than relying on memory,
  so the manuals reflect current firmware behavior, not stale assumptions.
- Quick Guide's worked example: cold storage settings for dessert plums at ~15° Brix, every
  relevant setting mapped to a value with a one-line rationale, explicitly labeled as an
  illustrative starting point (not food-safety authoritative). Added a small bonus table for
  apples/leafy-greens/dairy to cover the "etc." in the request without overstating precision.
- Found and flagged (not fixed — out of scope for a docs session): the Door Sensor Mode
  (NC/NO) switch also toggles the cabinet light relay — likely an unintentional coupling.
  Documented as a "known quirk" callout rather than silently patched.
- Screenshot placeholders throughout both docs for web dashboard + touchscreen, ready for
  real images once hardware is connected.
- No firmware/YAML changed — no compile run, no new build metrics.

## 2026-07-26 SD Auto-Remount + LVGL Settings Redesign (Single-Entry Paginated Model)

- Two asks: (1) SD card auto-recovers after a runtime failure without a reboot, (2) replace the
  touchscreen's fixed 3-tab settings layout with the old S3 project's single-entry, PIN-gated,
  paginated model, keeping WiFi config/superadmin password/SD-delete web-GUI-only.
- Found and fixed a real bug before writing the remount logic: `p4_sd_unmount()` no-op'd whenever
  `p4_sd_ready` was already false (exactly the post-failure state), leaving the VFS mount point
  registered and blocking a clean remount. Added `p4_sd_vfs_registered`, tracked independently, to
  fix it. New 60s interval retries `p4_sd_mount()`, logs `SD_REMOUNTED`, pushes a new
  `ntfy_sd_recovered_request`. Does not replay `backup.json` on recovery (avoids clobbering live
  settings changed since the last backup).
- Old 3 settings pages covered only 6 of 34 settings entities (rest were web-only); one page was
  mislabeled "Fallback" but was actually dead System Status diagnostics. Replaced with 7 new pages
  covering all 34 entities, reached via one PIN-gated "Settings" button (was: 3 independently-gated
  tabs + a `ctl_pin_target` global remembering which page to return to).
- Centralized all label refresh into one `refresh_all_settings_labels` script (called on `on_load`
  and after every button press) — the 5 pre-existing per-entity label updates now call it too, so
  web/HA changes stay reflected on the touchscreen, not just on-device presses.
- Retired `ctl_pin_target` and 3 per-page active-flag globals/diagnostic sensors, replaced with one
  `page_settings_active`/`page_settings_state`. Preserved the one live widget from the old page 3
  (lockout countdown) by moving it to the Info page instead of dropping it.
- Caught during compile (no hardware needed): 89 invalid flow-style YAML lines under block-mapping
  keys; 6 dangling label refs from the page swap; 12 ON/OFF+1 NC/NO toggle-label lambdas that
  failed to compile (ternary between different-length string literals decays to `const char*`, not
  `std::string`); an undersized ntfy buffer flagged by `-Wformat-truncation` (drive-by fix).
- Verified via grep across the entire `lvgl:` block: no WiFi config, superadmin/web password field,
  or SD-delete control anywhere in it.
- Build: RAM 21.3% (122,604 B), Flash 21.5% (1,579,656 B). Compile clean. Untested on hardware —
  neither the new pagination/PIN flow nor the auto-remount interval has touched a real device yet.

## 2026-07-26 Ntfy Timestamps, Full Event Logging, SD-Optional Operation + Failure Alert

- Four asks: ntfy timestamps, full event-log sensor context for control decisions, SD-optional
  operation, ntfy alert on SD failure.
- Audit found real gaps: compressor on/off never logged; door/no-cool/ice alarms never logged;
  existing alarm logging only happened inside the WiFi-gated ntfy block (so offline = nothing
  logged to SD either, a real bug since SD logging shouldn't depend on WiFi).
- Fixed: all 4 ntfy scripts get timestamps; new unconditional event logging for all 6 alarm types
  with sensor context; new consolidated COMPRESSOR_ON/OFF logging (one tick-end check, not
  scattered across relay-toggle sites); defrost start/end/manual-stop now include actual temps.
- SD-optional: confirmed already structurally sound, but found no runtime failure *detection*
  existed (card removed mid-session would leave `sd_card_ok` silently "true" forever). Added
  `p4_sd_mark_failed()` + a new `ntfy_sd_failure_request` covering both boot and runtime failure.
- Not built: auto-remount on card reinsertion (manual reboot to recover); ntfy push types for
  door/no-cool/ice (event-log only, as asked).
- Build: RAM 20.6%, Flash 21.0%. Compile clean. Untested on hardware.

## 2026-07-26 Full Settings Audit: Web-Settable, NVS-Persistent, Backed Up

- Asked to ensure every setting is settable from web GUI, has a help balloon, is in backup/
  restore, and saved to NVS. Ran a full audit rather than assuming the prior session's fix
  covered it — found 14 settings with no entity at all (not 3), one real NVS bug, one real
  backup/restore omission.
- Added 7 switch entities (probe2, humidity int/ext, door sensor, siren, defrost master enable,
  fallback enable) and 7 number entities (startup/defrost alarm grace, alarm hysteresis, fallback
  on/off timing, smart defrost delta/dwell) — all previously unreachable at runtime.
- Fixed: `ctl_startup_grace_min` had `restore_value: no` (only setting global marked that way) and
  was entirely missing from SD backup/restore. Both fixed, backward-compatible with old backups.
- New "Advanced Settings" dashboard section, data-driven from a config array (5 groups, ~30
  fields), each with a grounded help balloon — avoided hand-authoring 30 near-identical blocks.
  Mirrored in the static preview with demo values.
- No LVGL touchscreen controls added — web GUI was the explicit ask.
- Build: RAM +1.9 KB, Flash +9.4 KB. Compile clean. Untested on hardware.

## 2026-07-26 Fixed the No-Runtime-Toggle Gap Found Last Session

- Direct follow-up to a flagged issue: `input_smart_defrost_enabled`/`input_defrost_drip_enabled`/
  `input_defrost_term_temp_enabled` had no switch entity or LVGL control, permanently stuck at
  compile-time defaults.
- Added three new switch entities (`sw_smart_defrost`, `sw_defrost_drip`, `sw_defrost_term_temp`),
  mirroring the dew-point-trigger switch's exact pattern. All reachable via web_server UI/API now.
  No LVGL control added, matching the same scope as the dew-point switch.
- Purely additive — the underlying globals already existed and were already backed up/restored.
- Build: RAM +336 B, Flash +2.2 KB. Compile clean. Untested on hardware.

## 2026-07-25 Dew-Point Early Defrost Ported; Manual Calibration Offset Added

- User decision on the old S3 project's three deferred features: scrap primary-probe-override,
  port dew-point-triggered early defrost as-is, add only manual calibration offset entry (not the
  automated averaging routine).
- Dew-point defrost: Magnus-formula calc + trigger condition ported as pure functions, uses
  internal SHT31 as the air reference (correct pairing for this project). New `dew_point_start`
  trigger layered alongside manual/smart/interval. New real switch entity to enable/disable it —
  unlike three sibling enable-flags discovered to have no runtime toggle at all (found, not fixed).
- Manual calibration offset: new `probe1_offset_c`/`probe2_offset_c` number entities (±10°C),
  applied via filter lambda so every downstream consumer sees the calibrated value. No
  auto-calibration sequence, direct entry only.
- Extended SD backup/restore to round-trip the two offsets + trigger flag, made backward-compatible
  so older backup.json files without these keys still restore successfully.
- Noticed (not fixed): `opendir`/`readdir`/`closedir` linker warnings on the log manager — likely
  benign ESP-IDF pattern, unconfirmed without hardware.
- Build: RAM 20.2%, Flash 20.8%. Compile clean at every incremental step. Untested on hardware —
  no custom dashboard UI built (new entities reachable via ESPHome's own web UI, which was enough).

## 2026-07-25 Item 8 Resolved — Skip Real Dashboard Auth For Now

- Asked user to pick a direction for item 8 (real server-side dashboard authorization) rather than
  guess: skip / minimal reverse-proxy / wait for ESPHome upstream. Answer: skip for now, no
  concrete multi-user need identified. No code written. Closes out today's outstanding-items punch
  list (6, 7, 9 built earlier; 8 explicitly deferred by decision, not left ambiguous).

## 2026-07-25 Backup/Restore + Settings Buttons Wired; Live Compressor/Defrost Timers Added

- Given a 9-item outstanding-work punch list on request; user picked items 6, 7, 9 (item 8 set
  aside for clarification — not an actionable task as stated).
- #6: Backup/Restore buttons now POST to their real `btn_sd_backup`/`btn_sd_restore` entities.
  #7: The four Operational Settings Update buttons now POST to their real `number:` entities.
  Both pure frontend wiring — no backend changes, entities already existed.
- #9: Four new live countdown sensors (compressor lockout, defrost countdown/duration/drip
  remaining), computed from existing control-tick globals, no new state. New "Timers" card on the
  web dashboard. Found and fixed a related bug: LVGL's `lbl_lockout_timer_display` was defined but
  never updated by anything — permanently stuck at "0 min" — now shows the live countdown.
- Noted, not fixed: two other dead LVGL widgets on the same page, unclear original intent.
- Build: RAM +288 B, Flash +1.4 KB. Compile clean. Untested on hardware.

## 2026-07-25 SD Log File Manager + Simple WiFi Reconnect Implemented

- User picked the two options presented in the prior entry: full file manager for Delete/Download
  Logs, and read-only stats + simple (no-rollback) reconnect for WiFi Settings.
- New `p4_log_manager.h` — custom `AsyncWebHandler` registered on `web_server_base`'s shared
  `AsyncWebServer` (inherits the same basic-auth automatically). Routes: `GET /logs` (list),
  `GET /logs/download?file=X` (download, 4 MB cap), `POST /logs/delete?file=X` (delete). Filenames
  restricted to a strict allowlist (`events.csv` or `YYYY-MM-DD.csv`) as the only path-traversal
  defense. Every API call verified against the actual ESP-IDF web server shim source first.
- New WiFi reconnect: `input_wifi_new_ssid`/`input_wifi_new_password` text entities feed
  `wifi::global_wifi_component->save_wifi_sta()` — persists + reconnects immediately, same API
  ESPHome's captive portal uses. No test-before-commit safety net, per what was picked; recovery
  if wrong is the device's existing "CoolroomP4-Setup" fallback AP.
- Dashboard: Logs Management is now a real file picker; WiFi Settings shows current stats +
  reconnect form with an explicit warning and confirm-before-submit. Dead stub functions and
  "placeholder" help text removed. Preview mirrored (static mock content); fixed a `<select>`
  gating gap found while mirroring.
- Backup/Restore's stub wiring intentionally left untouched (separate, unrequested item).
- Build: RAM +280 B, Flash +5 KB. Compiled clean incrementally. Untested on hardware — WiFi
  reconnect path is now the top priority to verify carefully once connected.

## 2026-07-25 Help Balloons Guest-Gated; Hardware Config Became a Real Live Panel

- Reversed earlier same-day decision per feedback: help balloons now hide for guests entirely
  (not just stay clickable) — only visible after admin login. `setSectionInteractive()` updated
  in both dashboard files.
- Hardware Config button now opens a real live diagnostics panel instead of static help text —
  every stat (WiFi/RS485/RTC/SHT31/SHT20/SD-card status, chip temp, heap/PSRAM, SSID/RSSI/IP) was
  already a published entity, so this was pure frontend work. New `updateHardwarePanel()`
  populates it every poll cycle. Removed the now-redundant separate help icon and the dead
  `hardwareSettings()` stub.
- Researched (not implemented) the other three requested features — Delete Logs, Download Logs,
  WiFi scan+safe-switch — and confirmed via ESPHome source that all three need genuinely new
  firmware infrastructure (a custom SD file-manager HTTP handler; a new WiFi test-then-commit
  state machine with real risk to primary connectivity if done carelessly). Findings and options
  presented to the user rather than guessing at scope; response pending.
- No firmware/yaml changes, compile re-run: unchanged. Virtual preview mirrored, artifact
  republished at the same URL.

## 2026-07-25 Help Balloons Extended to System Administration Buttons

- Follow-up to the same-day setting-help-balloons work: added one help balloon per admin action
  button (Backup, Restore, Delete Logs, Download Logs, WiFi Settings, Hardware Config), not one
  per group — Backup/Restore especially needed separate explanations.
- Checked real backend status first: `btn_sd_backup`/`btn_sd_restore` are real entities the web
  dashboard's JS doesn't call yet (stub); Delete/Download Logs, WiFi, Hardware have no backend at
  all. Balloon text says what each button is meant to do and honestly flags whether it's wired up.
- Pure static asset change again, no firmware/yaml touched. Compile re-run: unchanged. Virtual
  preview mirrored, artifact republished at the same URL.

## 2026-07-25 Setting Help Balloons on the Web Dashboard

- Added clickable "i" help balloons to the web dashboard's five tunable settings (Setpoint, Alarm
  High Delta, Alarm Low Delta, Compressor Hysteresis, Touchscreen Access PIN) explaining each in
  plain English and naming which sensor/entity it correlates to, grounded in the actual
  `p4_control.h` logic (e.g. correctly attributes the No-Cooling alarm threshold to Compressor
  Hysteresis, not the alarm deltas). Admin action buttons (Backup/Restore/etc.) left without
  balloons — out of scope, they're actions not settings.
- Found and fixed a related bug while wiring this up: guest-mode `setSectionInteractive()` would
  have disabled the new help icons along with real controls. Now exempts `.help-icon` buttons.
- Flagged, not fixed: the four Operational Settings "Update" buttons are non-functional stubs —
  a stale code comment claims no backend endpoint exists, but ESPHome's `web_server` already
  auto-generates one for every `number:` entity (same pattern already used elsewhere on the page).
- Pure static asset change, no firmware/yaml touched. Compile re-run as a sanity check: unchanged.
- Virtual preview mirrored, artifact republished at the same URL.

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

**Device / product (current):**

- [ ] Fit RS485 relay + RTD boards; verify end-to-end cooling + Probes tab Raw/Offset/Corrected
- [ ] Replace USER_MANUAL / Quick Start screenshot placeholders with real captures
- [x] Carel follow-ups closed 2026-08-01 (A not required, B already done, C fan relay, D door hold)
- [x] Web SD log download — confirmed working 2026-08-01 (dashboard SD Card tab)
- [x] Multi-user server auth — **not required**; guest + single operator login is permanent

**Still deferred:**

- [ ] LVGL toggles for audio (web-only today)
- [ ] Dashboard OTA UI

**On hold (may not ship):**

- [~] Microphone / voice input — no mic on current hardware; may not be used
      (see `reference/AUDIO_ALERTS.md`)

**Note:** Older Phase 4 “flash and verify home page” checklist items are done — device is
in daily use on the bench.
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
