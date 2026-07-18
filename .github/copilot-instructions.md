# Coolroom Controller — Copilot Instructions

## Hardware Platform

**Board**: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (ESP32-P4NRW32)  
**All pin assignments and hardware decisions must be verified against this exact board.**

## Before Every Code Decision

1. **Check `reference/` folder first** — schematic PDF, datasheets, hardware_pins.md, control diagrams
2. **Then check Waveshare GitHub**: https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B
3. Never assume generic ESP32 pin mappings — always verify from schematic or official examples
4. To extract text from the schematic PDF: `pip install pymupdf` then use `fitz.open()`

## Confirmed GPIO Assignments

| Signal | GPIO | Source |
|--------|------|--------|
| I2C SDA (external, RTC, GT911) | 7 | Waveshare wiki |
| I2C SCL | 8 | Waveshare wiki |
| RS485 UART TX | 27 | 13_RS485_Test example |
| RS485 UART RX | 26 | 13_RS485_Test example |
| WiFi SDIO D0-D3 | 14-17 | esp32_p4_function_ev_board.h FIB |
| WiFi SDIO CLK | 18 | esp32_p4_function_ev_board.h FIB |
| WiFi SDIO CMD | 19 | esp32_p4_function_ev_board.h FIB |
| ESP32-C6 WKUP | 6 | esp32_p4_function_ev_board.h FIB |
| ESP32-C6 RESET | 54 | esp32_p4_function_ev_board.h FIB |
| TF Card SDMMC (slot 1) | 39-44 | Waveshare wiki |
| I2S Audio (ES8311) | 9-13, 53 | Waveshare wiki |
| GT911 Touch INT | 23 | Schematic NLGPIO23 |
| GT911 Touch RST | 33 | Schematic NLGPIO33 (shared with LCD) |
| Display Backlight BL_CTRL | 32 | Schematic NLGPIO32 |
| LDO ch3 2.5V (MIPI D-PHY) | — | Schematic ESP_LDO_VO3 |

**Reserved (never reuse)**: GPIO 6, 7-8, 9-13, 14-19, 23, 26-27, 32-33, 39-44, 53-54

## Code Architecture

**Principle**: C++/ESP-IDF first. YAML is configuration, not logic.

- `p4_helpers.h` — boot diagnostics, NTP, RTD validation, temp formatting, heap
- `p4_control.h` — compressor hysteresis, alarms, defrost scheduling, probe fault
- `esp32-p4-coolroom.yaml` — thin YAML; lambdas call C++ helpers, never contain business logic

## Build

```bash
cd /Volumes/Scratch/Documents/ESP32-P4-Coolroom
source .venv/bin/activate
esphome compile esp32-p4-coolroom.yaml
```

**ESPHome**: 2026.7.0 (ARM64 native in `.venv`)  
**ESP-IDF**: 5.5.4, RISC-V toolchain 14.2.0_20260121  
**Target**: RAM < 25%, Flash < 20%  
**ARM64 penv fix** (if IDF build fails with arch mismatch):
```bash
lipo ~/.esphome-idf/penvs/5.5.4/bin/python -thin arm64 -output /tmp/py-arm64
mv python python.bak && cp /tmp/py-arm64 python
```

## Documentation Rules

When firmware changes, **always update in the same commit**:
- `reference/hardware_pins.md` — if any GPIO changes
- `reference/control_logic_ns_diagram.md` — if control loop changes
- `reference/program_control_logic_flowchart.md` — if phase status or architecture changes
- YAML `# PHASE STATUS` header comment

**At every compact / phase boundary**, also:
- Append a dated recap block to `reference/session_recaps.md` summarising what changed
- Push `session_recaps.md` together with the diagram updates in the same commit

## Phase Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | WiFi, HA API, web server, OTA | ✅ |
| 2 | RS485 Modbus: relays + 3x RTD + PCF8563 RTC | ✅ |
| 3 | Coolroom control logic (hysteresis, alarms, defrost) | ✅ |
| 4 | LVGL 7" MIPI-DSI touchscreen dashboard | ✅ |
| 5 | SD card logging, ntfy notifications, backup/restore | ⏳ |

## Key Reference Files

| File | Purpose |
|------|---------|
| `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf` | Board schematic (3.7 MB) |
| `reference/hardware_pins.md` | All confirmed GPIO assignments |
| `reference/control_logic_ns_diagram.md` | Control loop NS diagram |
| `reference/program_control_logic_flowchart.md` | Boot sequence + sensor/phase flowcharts |
| `reference/RTC_PCF8563_Configuration.md` | PCF8563 RTC integration guide |
