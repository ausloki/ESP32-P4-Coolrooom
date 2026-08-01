# Active Context — Current Session State

**Date:** 2026-08-01  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Home gauge design locked (LVGL + web twins). Closeout requires embed +
compile + NVS-safe flash (`./tools/esphome_flash.sh`, never plain USB `esphome upload`).

## Working well

- Display/touch, WiFi, dashboard at `/` with SSE live data
- Soft-mute + HA opt-in; door reed on **GPIO46** (P1-7) — confirmed on glass
- HA shows entities (after reload); LVGL clock left / date right (`10 Apr`) — confirmed
- Home gauge: rings only on flat `#1C1C1E`, dial −10…+30, setpoint floor clamped
- Settings persistence across **reboot** confirmed live
- Coverage checker + persist-script staging check green
- Rule: `.cursor/rules/home-gauge-design.mdc` (always apply)

## Still to verify / open

1. Optional: second alarm type while muted → jiggle resumes (door reed is enough)
2. Probes tab live Raw/Offset/Corrected (needs RTD on bench)
3. ✅ Web gauge twin matches LVGL (Cory, 2026-08-01)
4. ✅ SD Backup works (Cory); full sensor coverage in backup.json still unverified without RTD/I2C probes
5. ✅ I2C/RTC: **dropped PCF8563** (2026-08-01). Waveshare FAQ = SoC 48-bit LP RTC +
   NTP; no I2C RTC chip in docs. Diagnostics: **System Time Valid**. Optional 1220 in
   holder item 11 backs VBAT.

## Leave for later

- RS485 / external I2C not on bench
- Manual screenshots still placeholders
- CPU % spike on hard browser refresh is expected (SSE full reconnect); smooth later if desired

## Key commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
# Settings-preserving flash. `esphome upload` over USB writes factory.bin from
# 0x0 and ERASES NVS (0x9000-0x15000) — that is why settings "came back".
./tools/esphome_flash.sh --device /dev/cu.usbmodem213401
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
./tools/code_review_graph_cli.sh update --repo .
```
