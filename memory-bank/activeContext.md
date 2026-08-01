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
- Rotating centre status + matching audio + ntfy hardware-offline
- Virtual preview aligned with live gauge / Audio / Alarms & Notify (2026-08-01)
- Rule: `.cursor/rules/home-gauge-design.mdc` (always apply)

## Closed this session (can-do-now punch list)

1. ✅ Virtual preview sync (no pink strip; rotating fault labels; ADVANCED_SETTINGS parity)
2. ✅ `opendir`/log listing — already fixed (`disable_vfs_support_dir: false`); noted closed
3. ✅ Stale handover / activeContext refreshed
4. ✅ Device spot-check: no `settingsGate` / `alarm-banner`; Status rotates
   `HUMIDITY SENSOR OFFLINE` (SHT enabled, not fitted); Hardware Offline Priority = urgent

## Still open

- Probes tab live Raw/Offset/Corrected (needs RTD on bench)
- Manual screenshots still placeholders
- Carel **A** not required; **B** done; **D** + fan relay implemented 2026-08-01
  (see `reference/CAREL_CONTROL_DECISIONS.md`)
- Deferred product: mic/voice, LVGL audio toggles, web log download, dashboard OTA UI,
  multi-user server auth

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
