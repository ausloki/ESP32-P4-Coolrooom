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
| ------- | ------ |
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
| ----- | ----------- | ------ |
| 1 | WiFi provisioning, HA native API, web server, OTA, NTP | Done |
| 2 | RS485 Modbus: relay board + RTD temp sensor | Done (boards not on current bench) |
| 3 | Coolroom control logic (setpoint, compressor, passive defrost, fan, alarms) | Done |
| 4 | LVGL touchscreen UI on 7" MIPI-DSI display | Done |
| 5 | SD card logging, ntfy notifications, SD backup/restore | **Current** |

Operator docs: `reference/USER_MANUAL.md`, `reference/QUICK_START_GUIDE.md`,
`reference/CAREL_CONTROL_DECISIONS.md` (hysteresis / min-run / fan / door-hold choices).

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

## Offline Autonomous Operation (No Wi-Fi / No HA)

Main control logic is designed to run locally even when there is no Wi-Fi or Home Assistant connection.

- Compressor, passive defrost, optional evaporator fan, alarm, sensor-fault handling, and SD logging run from the 10s local control loop.
- Wi-Fi/API disconnect does not reboot firmware (`wifi.reboot_timeout: 0s`, `api.reboot_timeout: 0s`).
- Time sync (SNTP) and push notifications (ntfy) are treated as optional network features.
- While offline, ntfy requests are skipped; notification edge flags reset so active alarms can notify after reconnect.

Operational note:

- Probe validity and RS485 health still determine safe operation; network presence is not part of compressor/defrost decisions.
- Opt-in **Hold Compressor While Door Open** (default off) can force the compressor off while the reed reads open; fan follows the compressor when **Fan Relay Enabled** is on.

## Site-to-Site VPN Routing (Preferred)

This project now targets Home Assistant via LAN IP over router-to-router VPN routing,
not public IP fallback.

- **HA LAN target**: `192.168.37.136`
- **Transport model**: two routers maintain tunnel routes between sites (WireGuard preferred)
- **Security model**: keep ESPHome device services private to LAN/VPN

Minimum routing and firewall requirements:

1. Add a route on each router for the remote site subnet via the VPN tunnel.
2. Allow inter-site traffic from the project subnet to `192.168.37.136` on HA port `8123/TCP`.
3. Allow Home Assistant to reach project-site ESPHome nodes on `6053/TCP` over VPN.
4. Keep `6053/TCP` closed on WAN (do not expose publicly).
5. Prefer closing public `8123/TCP` once VPN path is verified.

Validation checks:

```bash
# From project site (or dashboard host over VPN), verify HA endpoint reachability
curl -I --connect-timeout 5 http://192.168.37.136:8123/

# From HA site, verify ESPHome API reachability to a project node
nc -vz <project-device-lan-ip> 6053
```

Notes:

- ESPHome native API is inbound (`Home Assistant -> device`).
- The firmware does not establish a VPN session itself; VPN is handled by site routers.

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

### Compile Helpers (recommended)

Use the repo helper scripts to avoid recurring PATH/certificate/toolchain issues:

```bash
# Validate environment and report IDF penv architecture details
./tools/esphome_env_check.sh

# Compile with project-safe SSL cert + PATH setup
./tools/esphome_compile.sh

# Optional: force arm64-only IDF penv python on Apple Silicon if mismatch recurs
./tools/esphome_compile.sh --fix-arm64-penv
```

Notes:

- `tools/esphome_compile.sh` exports `SSL_CERT_FILE` from `certifi` and prepends standard system paths.
- `tools/esphome_compile.sh` also auto-retries a known ESPHome native-IDF reconfigure failure by patching the generated `.esphome/.../src/CMakeLists.txt` when `esp_http_server` or `esp_ringbuf` are omitted from `src` `REQUIRES`.
- `tools/esphome_env_check.sh` verifies `esphome` and `certifi` in `.venv` and checks ESP-IDF penv python architecture.

### Git Hook Dependency Checker (cross-machine)

Set up versioned repository hooks so dependency checks run automatically after checkout, merge, and before local commits:

```bash
# Configure repo to use versioned hooks in .githooks
python3 tools/setup_git_hooks.py

# On Windows (PowerShell / Command Prompt)
py -3 tools\setup_git_hooks.py
```

Run the checker manually at any time:

```bash
# Quick validation
python3 tools/dependency_check.py --quick

# Create .venv and install required Python packages from requirements.txt
python3 tools/dependency_check.py --install
```

Hook behavior:

- `.githooks/post-checkout` and `.githooks/post-merge` run non-blocking checks.
- `.githooks/pre-commit` runs a non-blocking dependency warning before each commit.
- If dependencies are missing, hooks print remediation commands for macOS/Linux and Windows.

### Windows One-Step Bootstrap

For first-time setup on Windows, run the bootstrap script from PowerShell:

```powershell
./tools/bootstrap_windows.ps1
```

This script:

- Creates `.venv` if missing
- Installs project tooling from `requirements.txt` using `tools/dependency_check.py --install`
- Configures `core.hooksPath=.githooks` via `tools/setup_git_hooks.py`
- Runs a final quick dependency verification

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
  - Coil 0 = **Fan** (enable default off; follows compressor; off during defrost/drip)
  - Coil 1 = **Compressor**
  - Coil 2 = **Light**
  - Coil 3 = **Siren**
  - Defrost is **passive** (no heater coil on this controller)
- **RTD temp sensor**: Waveshare RS485 RTD transmitter, slave address 100
  - CH1 register 01 = coolroom temperature × 0.1 °C
  - CH2 register 02 = evaporator temperature × 0.1 °C
  - Universal commissioning address 249 (guarded, only probed on conflict)

A second RTD board (slave 101) used to cover the external/ambient reading; it has been
decommissioned. That role is now covered by the I2C SHT20 sensor instead, which also adds
external humidity — see `reference/hardware_pins.md` for the I2C address table and wiring.
