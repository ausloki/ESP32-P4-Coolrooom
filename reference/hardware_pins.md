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

## 🚫 RESERVED / OCCUPIED GPIO (DO NOT USE)

| GPIO Range | Used By                            |
|------------|------------------------------------|
| 9–13       | On-board I2S audio (ES8311 / mics) |
| 39–44      | TF card SDMMC (slot 1)             |
| 53         | Audio amplifier enable             |
| 7–8        | External I2C header                |

---

## Available GPIO Header Pins (2×12, 17 programmable — item 24)

The GPIO header exposes 17 remaining programmable GPIOs and power pins.  
Exact assignments depend on the schematic. Avoid GPIOs in the reserved table above.

---

## ⚠️ UNCONFIRMED PINS — VERIFY FROM SCHEMATIC

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

## 🚫 RESERVED / OCCUPIED GPIO (DO NOT USE)

| GPIO Range | Used By                            |
|------------|---------------------------------|
| 9–13       | On-board I2S audio (ES8311 / mics) |
| 39–44      | TF card SDMMC (slot 1)             |
| 53         | Audio amplifier enable             |
| 7–8        | External I2C header + onboard RTC  |
| 14–19      | ESP32-C6 SDIO co-processor         |
| 26–27      | RS485 UART                         |
| 54         | ESP32-C6 reset                     |
| 6          | ESP32-C6 wakeup                    |

---

## Key Architectural Notes

1. **No on-chip WiFi.** The ESP32-P4 has no WiFi/Bluetooth silicon. All wireless communication goes through the ESP32-C6 co-processor via SDIO using the `esp_hosted` framework and ESPHome's `esp32_hosted` component.

2. **SDIO WiFi pins confirmed** from Waveshare `esp32_p4_function_ev_board.h` FIB variant (GPIO 14-19, 6, 54). Applied to `esp32-p4-coolroom.yaml` as of Phase 2/3 boundary commit.

3. **RS485 UART pins confirmed** from Waveshare `13_RS485_Test` example (GPIO 27 TX, 26 RX). Applied to YAML same commit.

3. **Mandatory: Verify RS485 UART pins before RS485 devices work.** The relay board and RTD sensor require the correct UART TX/RX pin assignments.

4. **Two separate SDIO buses.** The TF card (SD slot) uses SDMMC host 1 on GPIO 39-44. The C6 co-processor uses SDMMC host 0 on a separate set of GPIOs. These must never conflict.

5. **ESP32-P4 engineering sample flag.** If your board has chip revision < 3.0, set `engineering_sample: true` in the ESPHome `esp32:` block. Production boards ship with revision ≥ 3.0 and do not need this flag.
