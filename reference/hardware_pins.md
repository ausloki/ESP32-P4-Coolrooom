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

## ⚠️ UNCONFIRMED PINS — VERIFY FROM SCHEMATIC BEFORE DEPLOYMENT

Download schematic: https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf

### RS485 Header (PH2.0 4-PIN — item 22)
| Signal    | GPIO  | Status                |
|-----------|-------|-----------------------|
| UART TX   | ?     | VERIFY FROM SCHEMATIC |
| UART RX   | ?     | VERIFY FROM SCHEMATIC |
| VCC (5V)  | PWR   | Power only            |
| GND       | GND   | Power only            |

Likely candidate: UART1 or UART4 on the ESP32-P4. Possible GPIO: 35/36, 32/33, or 14/15.  
Check schematic net names `UART_RXD` / `UART_TXD` near the RS485 header footprint.

### WiFi Co-processor (ESP32-C6 via SDIO — item 2)
The C6 module connects to SDMMC host slot 0 on the ESP32-P4.  
The TF card uses slot 1 (GPIO 39-44). Slot 0 uses a separate pin set.

| Signal        | GPIO  | Status                |
|---------------|-------|-----------------------|
| SDIO CLK      | ?     | VERIFY FROM SCHEMATIC |
| SDIO CMD      | ?     | VERIFY FROM SCHEMATIC |
| SDIO D0       | ?     | VERIFY FROM SCHEMATIC |
| SDIO D1       | ?     | VERIFY FROM SCHEMATIC |
| SDIO D2       | ?     | VERIFY FROM SCHEMATIC |
| SDIO D3       | ?     | VERIFY FROM SCHEMATIC |
| ESP32-C6 RESET| ?     | VERIFY FROM SCHEMATIC |

Typical ESP32-P4 reference designs use GPIO 25-30 for SDMMC slot 0, but Waveshare may differ.  
Check schematic net names `SDIO_CLK` / `SDIO_CMD` / `SDIO_D0..D3` near the C6 module footprint.  
The C6 reset may share a GPIO with the SDMMC D3 line or use a dedicated GPIO.

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

## Key Architectural Notes

1. **No on-chip WiFi.** The ESP32-P4 has no WiFi/Bluetooth silicon. All wireless communication goes through the ESP32-C6 co-processor via SDIO using the `esp_hosted` framework and ESPHome's `esp32_hosted` component.

2. **Mandatory: Verify SDIO WiFi pins before first flash.** The `esp32_hosted` config in `esp32-p4-coolroom.yaml` contains placeholder SDIO pins. The device will not achieve WiFi connectivity until these match the schematic.

3. **Mandatory: Verify RS485 UART pins before RS485 devices work.** The relay board and RTD sensor require the correct UART TX/RX pin assignments.

4. **Two separate SDIO buses.** The TF card (SD slot) uses SDMMC host 1 on GPIO 39-44. The C6 co-processor uses SDMMC host 0 on a separate set of GPIOs. These must never conflict.

5. **ESP32-P4 engineering sample flag.** If your board has chip revision < 3.0, set `engineering_sample: true` in the ESPHome `esp32:` block. Production boards ship with revision ≥ 3.0 and do not need this flag.
