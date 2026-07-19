# Hardware Pin Reference — Waveshare ESP32-P4-WIFI6-Touch-LCD-7B

Board: [Waveshare ESP32-P4-WIFI6-Touch-LCD-7B](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)  
Chip: ESP32-P4NRW32 (RISC-V dual-core HP @ 400 MHz + LP @ 40 MHz)  
WiFi/BT: ESP32-C6-MINI-1 co-processor via SDIO (external module, not integrated)  
Flash: 32 MB NOR  
PSRAM: 32 MB (DMA-capable, stacked in-package)  
Display: 7 inch 1024×600 MIPI-DSI (touch via GT911)

---

## ✅ CONFIRMED PINS (from official Waveshare documentation)

### External I2C Header (PH2.0 4-PIN — item 19)
| Signal | GPIO |
|--------|------|
| SDA    | 7    |
| SCL    | 8    |

Source: Waveshare ESP-IDF tutorial, section 3 I2C Example.  
Note: External pullups provided on-board. Do NOT enable internal pullups in software.

### TF Card SDMMC (4-wire SDIO 3.0 slot — item 25)
| Signal | GPIO |
|--------|------|
| CLK    | 43   |
| CMD    | 44   |
| D0     | 39   |
| D1     | 40   |
| D2     | 41   |
| D3     | 42   |

Source: Waveshare ESP-IDF tutorial, section 5 SDMMC Example.

### Audio I2S / ES8311 Codec (built-in — items 10/23)
| Signal        | GPIO | Note                              |
|---------------|------|-----------------------------------|
| I2S MCLK      | 13   | Master clock to ES8311            |
| I2S SCLK      | 12   | Serial clock                      |
| I2S ASDOUT    | 11   | Audio output (codec → amp)        |
| I2S LRCK      | 10   | Left/Right channel select         |
| I2S DSDIN     | 9    | Audio input (mic → codec)         |
| PA_Ctrl       | 53   | NS4150B amplifier enable (HIGH=on)|

Source: Waveshare ESP-IDF tutorial, section 6 I2S Audio Example.  
⚠️ **GPIO 9-13 and GPIO 53 are dedicated to on-board audio. Never reuse for RS485 or other peripherals.**

---

## ✅ CONFIRMED PINS (from Waveshare official examples)

Download schematic for further verification: https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf

### RS485 Header (PH2.0 4-PIN — item 22)
| Signal    | GPIO  | Status                |
|-----------|-------|-----------------------|
| UART TX   | 27    | ✅ CONFIRMED          |
| UART RX   | 26    | ✅ CONFIRMED          |
| VCC (5V)  | PWR   | Power only            |
| GND       | GND   | Power only            |

Source: Waveshare `examples/ESP-IDF/13_RS485_Test/main/uart_echo_example_main.c` (ECHO_TEST_TXD=27, ECHO_TEST_RXD=26).

### WiFi Co-processor (ESP32-C6 via SDIO — item 2)
The C6 module connects to SDMMC host slot 0 on the ESP32-P4.  
The TF card uses slot 1 (GPIO 39-44). Slot 0 uses a separate pin set.

| Signal        | GPIO  | Status                |
|---------------|-------|-----------------------|
| SDIO CLK      | 18    | ✅ CONFIRMED          |
| SDIO CMD      | 19    | ✅ CONFIRMED          |
| SDIO D0       | 14    | ✅ CONFIRMED          |
| SDIO D1       | 15    | ✅ CONFIRMED          |
| SDIO D2       | 16    | ✅ CONFIRMED          |
| SDIO D3       | 17    | ✅ CONFIRMED          |
| ESP32-C6 WKUP | 6     | ✅ CONFIRMED          |
| ESP32-C6 RESET| 54    | ✅ CONFIRMED          |

Source: Waveshare `esp32_p4_function_ev_board.h` (CONFIG_BSP_BOARD_TYPE_FIB variant). Board variant detected from I2S audio pins (GPIO 9-13) and I2C pins (GPIO 7/8).

### Touch Controller (GT911 — item 7)
The GT911 communicates via I2C. It may share GPIO7/8 with the external I2C header, or use dedicated lines.

| Signal    | GPIO  | Status                |
|-----------|-------|-----------------------|
| SDA       | 7?    | Likely shared — VERIFY|
| SCL       | 8?    | Likely shared — VERIFY|
| INT       | ?     | Interrupt — VERIFY    |
| RST       | ?     | Reset — VERIFY        |

### Status LED (item 13)
| Signal    | GPIO  | Status                |
|-----------|-------|-----------------------|
| LED       | ?     | VERIFY FROM SCHEMATIC |

---

## Available GPIO Header Pins (2×12, 17 programmable — item 24)

The GPIO header exposes programmable GPIOs and power pins. Avoid all GPIOs in the reserved table.

### PH2.0 12PIN Header Map (item 24)

Source: board schematic PDF net labels (`reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`) + project wiring validation.

This is a **net availability map** for the 2x12 expansion header. Physical pin-number order on the connector should be verified against the board silk/schematic viewer during harness build.

| Group | Nets available on PH2.0 12PIN header |
| --- | --- |
| GPIO signals | GPIO2, GPIO3, GPIO4, GPIO5, GPIO20, GPIO28, GPIO29, GPIO30, GPIO31, GPIO34, GPIO49, GPIO50, GPIO51, GPIO52 |
| Power rails | ESP_3V3, Core_5V, GND |

### Supported Header Voltages

| Rail / Signal Type | Nominal Voltage | Notes |
| --- | --- | --- |
| ESP_3V3 | 3.3V | Logic rail for ESP32-P4 GPIO domain |
| Core_5V | 5.0V | Power rail only (peripheral supply) |
| GPIO signal level | 3.3V logic | Treat GPIO as 3.3V-only; do not drive above 3.3V |
| GND | 0V | Common reference |

### Wiring Guidance

- Use `ESP_3V3` for 3.3V sensors/logic interfaces.
- Use `Core_5V` only when the attached module requires 5V power and has 3.3V-compatible I/O (or proper level shifting).
- For digital inputs (e.g. door reed on GPIO20), wire the switch between GPIO and GND when using `INPUT_PULLUP`.

### Project Power Architecture (DIN PSU)

This project uses two DIN-mounted power supplies:

| Supply | Primary Loads |
| --- | --- |
| 5V DIN PSU | ESP32-P4 controller via PH2.0 12PIN header (`Core_5V` + `GND`) |
| 12V DIN PSU | RS485 RTU-4 relay module and RS485 RTD PT100 modules |

#### Grounding Requirement

Yes, the supplies should share a **common ground reference** for reliable RS485/UART-referenced communication in this architecture.

- Bond 5V PSU negative and 12V PSU negative together at one control-panel star point.
- Tie that common point to controller `GND` and RS485 device `GND`.
- Keep power returns tidy (avoid long daisy-chain ground loops).

Without a common ground, RS485 transceiver common-mode can drift and produce intermittent or unstable communication.

---

## ✅ CONFIRMED PINS (from board schematic PDF)

Source: `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf` — net labels NLGPIO23/33/32 confirmed adjacent to GT911 and backlight driver.

### Touch Controller (GT911 — item 7)
Shares I2C bus with external header (GPIO 7/8). GT911 default I2C address: 0x5D.

| Signal | GPIO | Status |
|--------|------|--------|
| SDA    | 7    | ✅ Shared with I2C bus |
| SCL    | 8    | ✅ Shared with I2C bus |
| INT    | 23   | ✅ CONFIRMED (NLGPIO23 → INT_TP) |
| RST    | 33   | ✅ CONFIRMED (NLGPIO33 → RESET_TP, shared with LCD reset) |

### MIPI-DSI Display (7" 1024×600, JD9365 — items 6/7)
| Signal        | GPIO | Status |
|---------------|------|--------|
| LCD RESET     | 33   | ✅ CONFIRMED (schematic + ESPHome model) |
| Backlight BL  | 32   | ✅ CONFIRMED (NLGPIO32 → BL_CTRL) |

ESPHome model: `WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B`  
LDO channel 3 at 2.5V required for MIPI D-PHY power.

---

## 🚫 RESERVED / OCCUPIED GPIO (DO NOT USE)

| GPIO       | Used By                            |
|------------|------------------------------------|
| 6          | ESP32-C6 wakeup                    |
| 7–8        | I2C bus (RTC, GT911, external hdr) |
| 9–13       | On-board I2S audio (ES8311 / mics) |
| 14–19      | ESP32-C6 SDIO co-processor         |
| 23         | GT911 touch INT                    |
| 26         | RS485 UART RX                      |
| 27         | RS485 UART TX                      |
| 32         | Display backlight BL_CTRL          |
| 33         | LCD + touch RST (shared)           |
| 39–44      | TF card SDMMC (slot 1)             |
| 53         | Audio amplifier enable             |
| 54         | ESP32-C6 reset                     |

---

## ✅ USER GPIO — Field I/O Devices

| GPIO | Function           | Device          | Status           |
|------|--------------------|-----------------|------------------|
| 20   | Door reed sensor   | Normally-closed | ✅ INPUT_PULLUP  |

**GPIO 20 — Door Reed Sensor:**
- Connected to external GPIO header (item 24, 2×12 connector)
- Reed switch can be configured as NC (Normally Closed) or NO (Normally Open)
- INPUT_PULLUP mode pulls to 3.3V, reed pulls to GND when closed
- **Web GUI Toggle:** "Door Sensor Mode + Light Relay" switch (Phase 5)
  - ON = NC mode (switch closed when door closed) + Light Relay ON
  - OFF = NO mode (switch open when door closed) + Light Relay OFF
- Integration: Binary sensor `door_reed_sensor` → `ctl_door_alarm_active` flag
- **Light Relay Control:** Automatically toggles light relay when door opens/closes
- Control logic: Compressor disabled when door is open (safety interlock)
- Global variable: `ctl_door_sensor_mode_is_nc` (persistent across restarts)

---

## Key Architectural Notes

1. **No on-chip WiFi.** The ESP32-P4 has no WiFi/Bluetooth silicon. All wireless communication goes through the ESP32-C6 co-processor via SDIO using the `esp_hosted` framework and ESPHome's `esp32_hosted` component.

2. **SDIO WiFi pins confirmed** from Waveshare `esp32_p4_function_ev_board.h` FIB variant (GPIO 14-19, 6, 54). Applied to `esp32-p4-coolroom.yaml` as of Phase 2/3 boundary commit.

3. **RS485 UART pins confirmed** from Waveshare `13_RS485_Test` example (GPIO 27 TX, 26 RX). Applied to YAML same commit.

3. **Mandatory: Verify RS485 UART pins before RS485 devices work.** The relay board and RTD sensor require the correct UART TX/RX pin assignments.

4. **Two separate SDIO buses.** The TF card (SD slot) uses SDMMC host 1 on GPIO 39-44. The C6 co-processor uses SDMMC host 0 on a separate set of GPIOs. These must never conflict.

5. **ESP32-P4 engineering sample flag.** If your board has chip revision < 3.0, set `engineering_sample: true` in the ESPHome `esp32:` block. Production boards ship with revision ≥ 3.0 and do not need this flag.
