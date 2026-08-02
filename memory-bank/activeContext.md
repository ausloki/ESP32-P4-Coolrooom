# Active Context — Current Session State

**Date:** 2026-08-02  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Closeout in progress — audio soft-start + web toggle refresh + home alt
gauge. Next: RS485/RTD bench bring-up (revisit Nest alt gauge with live temps).

## Working well

- Display/touch, WiFi, dashboard at `/` with SSE live data
- Soft-mute + HA opt-in; door reed on **GPIO46** (P1-7) — confirmed on glass
- HA shows entities (after reload); LVGL clock left / date right (`10 Apr`) — confirmed
- Home gauge classic: rings only on flat `#1C1C1E`, dial −10…+30, setpoint floor clamped
- Home gauge alt (swipe up): cyan set + blue current overlay, Nest knobs, ambient 384;
  centre readout always blue; °C/°F via settings — **revisit with live RTD**
- Audio: Kokoro af_sarah; soft-start anti-click (canonical — `AUDIO_ALERTS.md`);
  web Audio toggles optimistic + retry GET refresh
- Settings persistence across **reboot** confirmed live
- Coverage checker + persist-script staging check green
- Rotating centre status + matching audio + ntfy hardware-offline
- Virtual preview aligned with live gauge / Audio / Alarms & Notify
- Per-phrase speaker-icon audio preview on LVGL Settings 8/8 + web Audio tab
- Temperature Display Unit: Celsius default / Fahrenheit, LVGL + web + HA
- LVGL settings **1/8–8/8** on glass
- Probe roles: **CH1 room air / CH2 evaporator**
- Rule: `.cursor/rules/home-gauge-design.mdc` (always apply)

## Still open

- Probes tab live Raw/Offset/Corrected (needs RTD on bench)
- Manual screenshots still placeholders
- Dashboard OTA UI
- End-to-end Modbus once relay/RTD boards are fitted
- Alt home gauge / Nest knobs with real coolroom + ambient readings
- Richer log-based tuning once Probe 1 has real coolroom samples

## On hold / not required

- Microphone / voice input — no mic fitted; may not be used
  (see `reference/AUDIO_ALERTS.md`)
- Real multi-user web auth — guest + single operator login is the permanent model
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
