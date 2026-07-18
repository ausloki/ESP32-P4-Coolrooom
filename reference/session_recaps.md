# Session Recaps

One entry per compact/phase-boundary. Always push with the compact commit.

---

## 2026-07-18 — Phases 2→4 + Context Setup

**Session scope**: Full build of Phases 2 through 4 from Phase 1 scaffold.

### Phase 2 → 3 Boundary
**What changed:**
- RS485 UART pins corrected: GPIO 35/36 → **GPIO 27 (TX) / 26 (RX)** from Waveshare 13_RS485_Test example
- WiFi SDIO pins corrected: GPIO 25-30 → **GPIO 14-19, 6, 54** from esp32_p4_function_ev_board.h FIB variant
- PCF8563 RTC integrated: `pcf8563` time platform, `hw_rtc_ok` health flag, `rtc_online` binary sensor
- Build environment fixed: ARM64 pydantic_core conflict in IDF penv resolved via `lipo -thin arm64`
- **Compiled clean**: RAM 16.9% (97.5 KB/576 KB), Flash 12.8% (942 KB/7.3 MB)

### Phase 3 Complete
**What changed:**
- New file: `p4_control.h` — pure C++ control functions (no Arduino, no lambdas)
  - `p4_ctl_compressor_eval()` — hysteresis band ±½·diff around setpoint; probe fault lockout
  - `p4_ctl_alarm_high/low()` — delta threshold alarms vs setpoint
  - `p4_ctl_defrost_due()` — time-based scheduling, 10-min boot grace, 8h default interval
  - `p4_ctl_defrost_timeout()` — 30-min safety cutout
  - `p4_ctl_probe_fault()` — stale probe detection (30 s default)
- YAML: 10 s control loop interval (probe fault → compressor → alarm → defrost)
- YAML: new globals `ctl_probe_fault`, `ctl_alarm_high/low_active`, `ctl_defrost_*_ms`
- YAML: new binary sensors `alarm_high_active`, `alarm_low_active`, `probe_fault_active`
- **Compiled clean**: RAM 17.0% (97.9 KB/576 KB), Flash 12.9% (945 KB/7.3 MB)

### Phase 4 Complete
**What changed (all GPIO confirmed from board schematic PDF):**
- GT911 touch INT: GPIO 23 (NLGPIO23 → INT_TP), RST: GPIO 33 (shared with LCD, NLGPIO33)
- Display backlight BL_CTRL: GPIO 32 (NLGPIO32)
- LDO channel 3 @ 2.5V: ESP_LDO_VO3 net confirmed in schematic
- New YAML blocks: `esp_ldo` (ch3/2.5V), `display` (mipi_dsi), `touchscreen` (gt911), `output`+`light` (LEDC backlight), `font` (4 Roboto sizes), `color` (8 dark-theme constants), `lvgl` (full dashboard)
- LVGL dashboard (1024×600 landscape):
  - Header: device title + live HH:MM:SS clock
  - Main card: 64 px coolroom temp, colour-coded (green/red/blue) vs alarm thresholds
  - Setpoint card: current SP display + ▲/▼ touch buttons wired to `setpoint` number entity
  - Probes card: evaporator + ambient temperatures
  - Status card: 7 LEDs (compressor, defrost, high alarm, low alarm, probe fault, RS485, RTC)
  - Bottom bar: status message (updates on probe fault)
- Sensor `on_value` → LVGL label updates; relay `on_turn_on/off` → LED brightness
- **Compiled clean**: RAM 18.1% (104 KB/576 KB), Flash 16.5% (1.18 MB/7.3 MB)

### Context & Tooling
- `.github/copilot-instructions.md` created with full GPIO table, code architecture, build commands, doc rules
- `/memories/repo/project.md` created with all board/build/phase facts
- `/memories/hardware-preferences.md` updated to ESP32-P4 board
- `chat.sessionSync.enabled: true` set in VS Code user settings
- Session store reindexed

**Commit range**: `79dfde8` (Phase 1) → `03d7a1b` (context files)  
**GitHub**: https://github.com/ausloki/ESP32-P4-Coolrooom/tree/main

---

## 2026-07-18 — Phase 5: SD Logging, ntfy, Backup/Restore

### What changed

**p4_logging.h (new)**:
- `p4_sd_mount()` / `p4_sd_unmount()` / `p4_sd_is_ready()` — SDMMC slot 1 (GPIO 39-44), FATFS/VFS
- `p4_sd_log_temps()` — daily CSV at `/sdcard/logs/YYYY-MM-DD.csv`, header auto-created
- `p4_sd_log_event()` — timestamped events at `/sdcard/events.csv`
- `p4_sd_backup_params()` — JSON dump to `/sdcard/backup.json`
- `p4_sd_restore_params()` — JSON parse from SD, updates NVS globals
- `p4_sd_free_mb()` — FATFS f_getfree() → MB

**esp32-p4-coolroom.yaml**:
- Phase status header updated: Phase 5 ← CURRENT
- New substitutions: `log_interval_min`, `ntfy_server`, `ntfy_topic`, `ntfy_priority`
- `p4_logging.h` added to includes
- `on_boot priority -200`: SD mount + optional param restore from backup.json
- FATFS sdkconfig options: `CONFIG_FATFS_LFN_HEAP`, `CONFIG_FATFS_MAX_LFN`
- `http_request:` component (IDF, no SSL verify, 10s timeout)
- New globals: `sd_card_ok`, `ntfy_alarm_hi/lo_sent`, `ntfy_probe_fault_sent`, `log_tick_count`
- New sensors: `sd_free_mb`, `sd_card_online` binary sensor
- New buttons: `btn_sd_backup`, `btn_sd_restore` (HA + web UI)
- 10s control loop extended with steps 5 (SD temperature log) and 6 (ntfy edge-triggered push)
- 4 ntfy scripts: `ntfy_high_alarm_request`, `ntfy_low_alarm_request`, `ntfy_alarm_clear_request`, `ntfy_probe_fault_request`

**References updated**:
- `control_logic_ns_diagram.md` — Phase 5 globals added to state flag table
- `program_control_logic_flowchart.md` — Phase 5 marked current, Phase 4 complete

**Build**: RAM 18.3% (105.6 KB/576 KB), Flash 19.4% (1.43 MB/7.3 MB) ✅

**Commit**: Phase 5 complete  
**GitHub**: https://github.com/ausloki/ESP32-P4-Coolrooom/tree/main

---
