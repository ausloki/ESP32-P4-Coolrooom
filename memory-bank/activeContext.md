# Active Context — Current Session State

**Date:** 2026-08-01  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Closeout complete this evening — fixed Probe 1/2 roles, 2CH RTD docs,
log-tuning tools + Win/mac launchers. Next: RS485/RTD bench bring-up.

## Working well

- Display/touch, WiFi, dashboard at `/` with SSE live data
- Soft-mute + HA opt-in; door reed on **GPIO46** (P1-7) — confirmed on glass
- HA shows entities (after reload); LVGL clock left / date right (`10 Apr`) — confirmed
- Home gauge: rings only on flat `#1C1C1E`, dial −10…+30, setpoint floor clamped
- Settings persistence across **reboot** confirmed live
- Coverage checker + persist-script staging check green
- Rotating centre status + matching audio + ntfy hardware-offline
- Virtual preview aligned with live gauge / Audio / Alarms & Notify (2026-08-01)
- Temperature Display Unit restored: Celsius default / Fahrenheit, LVGL + web + HA
- LVGL settings **1/8–8/8** on glass (Cory confirmed 2026-08-01 evening)
- Info page: glyph fix (`|` / `—`), SD used/free %, NTP sync-age latch
- Probe roles fixed: **CH1 room air / CH2 evaporator** (no source swaps)
- 2CH PT100 Modbus sheets in `reference/PT100-RS485-2CH-*.png` + `hardware_pins.md`
- Log tools: `tools/recommend_settings.py` + `CoolroomLogTools.cmd/.command`
- Web-only by design: Wireless change, Events, ntfy URL/priorities, SD file list
- Rule: `.cursor/rules/home-gauge-design.mdc` (always apply)

## Closed this session

1. ✅ Virtual preview sync
2. ✅ SD `opendir`/`readdir` (already fixed; noted closed)
3. ✅ °C/°F display preference
4. ✅ LVGL settings parity (8 pages) — glass-confirmed
5. ✅ Info tofu glyphs / SD percentages / NTP status latch
6. ✅ Carel A–D (+ manuals)
7. ✅ Fixed Probe 1/2 roles; removed source toggles
8. ✅ 2CH RTD product-sheet digest + log pull/recommend tools + OS launchers

## Still open

- Probes tab live Raw/Offset/Corrected (needs RTD on bench)
- Manual screenshots still placeholders (camera captures only; no FB screenshot tool)
- Dashboard OTA UI
- End-to-end Modbus once relay/RTD boards are fitted
- Richer log-based tuning once Probe 1 has real coolroom samples

## On hold / not required

- Microphone / voice input — no mic fitted; may not be used on this project
  (see `reference/AUDIO_ALERTS.md`)
- Real multi-user web auth — not required; guest + single operator login is the
  permanent model
- LVGL framebuffer screenshot component — declined

## Leave for later (bench)

- RS485 / external I2C not on bench — offline faults expected
- CPU % spike on hard browser refresh expected (SSE full reconnect); EMA-smoothed in UI

## Key commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
./tools/esphome_flash.sh --device /dev/cu.usbmodem213401
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
./tools/code_review_graph_cli.sh update --repo .
```
