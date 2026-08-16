# Handover Notes — 2026-08-16

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Device:** `192.168.50.21` (OTA; never USB `esphome upload` — that erases NVS)

## What landed

RS485 **online** lights now follow a successful Modbus poll, not only ESPHome
`on_online` (recovery-only in 2026.7). Live after OTA: RTD + relay **ON**,
Coolroom **19.6 °C**, evaporator **19.2 °C** (Probe 2 Enabled was NVS-off; turned
on after flash). Diagnostic address text is **100**, matching the poll.

## Bench (current)

| Bus | Status |
| --- | --- |
| RS485 RTU-4 relay addr **1** | Fitted — treat online/offline as live |
| RS485 2CH PT100 addr **100** (CH1 room, CH2 evap) | Fitted — treat as live |
| I2C SHT31 / SHT20 | **Not** fitted — leave enables off |
| CT clamp addr 110 | **Not** fitted — leave CT Clamp Enabled off |

See `.cursor/rules/bench-hardware-status.mdc`.

## Still open

- End-to-end compressor/light coil commands on the live RTU-4 (flags now true; plant
  actuation not fully proven this session).
- I2C humidity/ambient and CT run-proof still wait on hardware.
- Manual screenshots still placeholders.
- Dashboard OTA UI (web upload) — separate from ESPHome CLI OTA.

## Commands

```powershell
$env:PYTHONUTF8 = '1'
.venv\Scripts\python.exe tools/check_dashboard_coverage.py
.venv\Scripts\esphome.exe upload esp32-p4-coolroom.yaml --device 192.168.50.21
```
