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

1. ✅ Soft-mute + second alarm jiggle (2026-08-01): door alarm was gated behind
   `!fault`, so probe fault blocked mute-lift; door eval now runs every tick.
   Confirmed on glass: mute → hold reed open → bell jiggling again.
2. ✅ Sensor fallback duty cycle (2026-08-01): the ON window used to burn down inside the
   compressor off-delay (both default to 3 min, and boot seeds the off-delay), so the
   compressor never ran under probe fault. `p4_ctl_fallback_should_run()` now takes
   `compressor_locked_out` and holds the ON window instead of spending it.
3. ✅ Compressor gated on RS485 relay board online (2026-08-01): no coil → no start, red
   snowflake, centre status **RELAY BOARD OFFLINE**, no snow FX. Fallback ON window held while
   board is down. Web SSE first-paint fixed (leading edge + `state_detail_all`). Centre
   fonts enlarged (LVGL 80 / web twin).
4. ✅ Rotating multi-fault centre status + matching speech (2026-08-01): plain-language
   labels (RELAY/TEMP BOARD OFFLINE, HUMIDITY/AMBIENT SENSOR OFFLINE, COOLROOM PROBE BAD,
   …); audio clips + Audio-tab toggles edge-triggered with the status.
5. ✅ Hardware-offline ntfy pushes (2026-08-01): same four rising edges + ONLINE
   recoveries; shared Hardware Offline Priority (urgent); All-Clear for recoveries.
6. Probes tab live Raw/Offset/Corrected (needs RTD on bench)

## Leave for later

- RS485 / external I2C not on bench
- Manual screenshots still placeholders
- CPU % spike on hard browser refresh is expected (SSE full reconnect); web UI
  now EMA-smooths the displayed value (firmware sensor unchanged)

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
