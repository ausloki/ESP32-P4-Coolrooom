# ESP32-P4 Coolroom Controller

ESPHome-based coolroom controller running on the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** board.

## Hardware-First Rule

This project targets the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** board specifically, not a generic ESP32 device.

**Before any code or pin changes:**
- Verify against `reference/hardware_pins.md` and the [board schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf).
- The ESP32-P4 has **no on-chip WiFi**. All wireless communication routes through the ESP32-C6 co-processor via SDIO using `esp32_hosted`. Do not assume standard `wifi:` component behaviour.
- GPIO 9–13 and GPIO 53 are **reserved** by the on-board audio codec. Do not use for RS485 or other peripherals.
- GPIO 39–44 are **reserved** by the TF card SDMMC slot. Do not reuse.
- See `reference/hardware_pins.md` for confirmed and unconfirmed pin assignments.

## Board Summary

| Feature | Detail |
|---------|--------|
| Chip | ESP32-P4NRW32 — RISC-V dual-core HP @ 400 MHz + LP @ 40 MHz |
| WiFi/BT | ESP32-C6 co-processor (SDIO) — WiFi 6 2.4 GHz, BT 5 |
| Flash | 32 MB NOR |
| PSRAM | 32 MB (DMA-capable, octal, stacked in-package) |
| Display | 7" 1024×600 MIPI-DSI with GT911 capacitive touch |
| I2C header | GPIO7 (SDA) / GPIO8 (SCL) — confirmed |
| RS485 header | TX/RX pins TBD — verify from schematic |
| TF card | GPIO39-44 (SDMMC 4-wire) — confirmed |

## Project Status

| Phase | Feature Set | Status |
|-------|-------------|--------|
| 1 | WiFi provisioning, HA native API, web server, OTA, NTP | **Current** |
| 2 | RS485 Modbus: relay board + RTD temp sensor | Planned |
| 3 | Coolroom control logic (setpoint, compressor, defrost, alarms) | Planned |
| 4 | LVGL touchscreen UI on 7" MIPI-DSI display | Planned |
| 5 | SD card logging, ntfy notifications, SD backup/restore | Planned |

## Key Differences from S3 Project

- **No on-chip WiFi** — requires `esp32_hosted` SDIO component pointing at ESP32-C6.
- **RISC-V architecture** — ESP-IDF only (no Arduino). All lambdas are standard C++/IDF.
- **MIPI-DSI display** — replaces RGB parallel display from S3 board; different driver component.
- **Two SDMMC buses** — slot 1 (GPIO39-44) for TF card; slot 0 for WiFi C6 co-processor. Must not conflict.
- **Home Assistant integration** — native API with encrypted transport (`api:` block with `encryption: key:`).
- **WiFi network scanning/provisioning** — `captive_portal:` component provides a browser-based AP mode UI for selecting and configuring the target WiFi network without serial access.

## WiFi Provisioning

When the device cannot connect to any configured network it starts an access point:

- **SSID**: `CoolroomP4-Setup`
- **Password**: set in `secrets.yaml` → `ap_password`

Connect to that AP and navigate to **192.168.4.1** to:
- Scan and see available networks
- Enter credentials for your target network
- Device reconnects automatically and persists the credentials

## Home Assistant Setup

1. Ensure device and HA are on the same network.
2. HA auto-discovers the device via mDNS.
3. Set the API encryption key in `secrets.yaml` to match whatever HA expects.
4. All entities appear under the device in HA with their web-server sorting group names.

## Development Environment

```bash
# Create virtualenv and install tooling
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install esphome code-review-graph

# Compile firmware (no device needed)
.venv/bin/esphome compile esp32-p4-coolroom.yaml

# Flash via USB-C (item 15 on board — Type-C USB1.1 FS)
.venv/bin/esphome upload esp32-p4-coolroom.yaml

# OTA flash (once WiFi is working)
.venv/bin/esphome upload --device esp32-p4-coolroom.local esp32-p4-coolroom.yaml
```

## No-USB Operations

Once WiFi is live, all diagnostics can be done without USB:
- **OTA updates** via `esphome upload --device <ip-or-hostname>`
- **Web diagnostics** at `http://<device-ip>/` (auth required)
- **HA entity monitoring** for sensor values, relay states, alarms

## Code-Review Graph

The project uses `code-review-graph` for change-impact analysis (same workflow as the S3 project):

```bash
# Build/refresh the graph
./tools/code_review_graph_cli.sh update --repo .

# Check graph status
./tools/code_review_graph_cli.sh status

# Analyse what changes are at risk before committing
./tools/code_review_graph_cli.sh detect-changes --base HEAD~1 --brief
```

## RS485 Hardware (same as S3 project)

- **Relay board**: Waveshare RTU relay board, Modbus RTU slave address 1
- **RTD temp sensor**: Waveshare RS485 RTD transmitter, slave address 100
  - CH1 register 01 = coolroom temperature × 0.1 °C
  - CH2 register 02 = evaporator temperature × 0.1 °C
  - Universal commissioning address 249 (guarded, only probed on conflict)
