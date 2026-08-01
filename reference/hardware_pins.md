# Hardware Pin Reference — Waveshare ESP32-P4-WIFI6-Touch-LCD-7B

Board: [Waveshare ESP32-P4-WIFI6-Touch-LCD-7B](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)  
Chip: ESP32-P4NRW32 (RISC-V dual-core HP @ 400 MHz + LP @ 40 MHz)  
WiFi/BT: ESP32-C6-MINI-1 co-processor via SDIO (external module, not integrated)  
Flash: 32 MB NOR  
PSRAM: 32 MB (DMA-capable, stacked in-package)  
Display: 7 inch 1024×600 MIPI-DSI (touch via GT911)

**Authoritative sources (check local PDFs first — do not treat chat memory as truth):**

1. Board hardware manual / schematic: `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`
2. This digested map: `reference/hardware_pins.md` (update it when pins change)
3. ESP32-P4 TRM / datasheet: `reference/esp32-p4_technical_reference_manual_en.pdf`,
   `reference/esp32-p4_datasheet_en.pdf`
4. Live Waveshare ESP-IDF demos (re-check before pin changes):
   https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF
5. Board wiki: https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B

When Waveshare table names (e.g. codec `ASDOUT` / `DSDIN`) conflict with ESPHome/ESP-IDF
`dout`/`din`, trust **schematic / sample code** MCU direction macros and document the
naming inversion here.

### Bench note (current)

RS485 (relay / RTD boards) and external I2C temp sensors are **not** connected on the
current bench setup. Offline Modbus / missing external readings are expected until that
hardware is fitted — see `.cursor/rules/bench-hardware-status.mdc`.

---

## ✅ CONFIRMED PINS (from official Waveshare documentation)

### External I2C Header (PH2.0 4-PIN — item 19)
| Signal | GPIO |
|--------|------|
| SDA    | 7    |
| SCL    | 8    |

Source: Waveshare ESP-IDF tutorial, section 3 I2C Example.  
Note: External pullups provided on-board. Do NOT enable internal pullups in software.

**Devices on this bus (shared — I2C is a multi-drop bus, not point-to-point):**

| Device | I2C Address | Role |
| --- | --- | --- |
| *(on-board timekeeping)* | *(not I2C)* | ESP32-P4 LP/VBAT RTC via battery holder item 11 — Waveshare does **not** name an I2C RTC part; see `reference/RTC_Configuration.md` |
| GT911 touch controller | 0x5D | Display touch (required — do not remove/repurpose this bus) |
| SHT31 | 0x44 (0x45 on some breakouts — check ADDR pin strapping before flashing) | Internal (coolroom) humidity + temperature |
| SHT20 (HTU21D-compatible) | 0x40 (fixed) | External (ambient) humidity + temperature |

Humidity/temp sensors wire to this same item-19 4-pin header in parallel (SDA/SCL/VCC/GND).
There is no I2C RTC on this board; firmware uses SoC LP RTC + SNTP only. A 1220 cell in
holder item 11 backs SoC VBAT so wall time can survive power loss. There is no
second I2C connector on this board. On-board pullups (noted above) are shared across all
devices on the bus — do not add per-sensor pullup resistors, that would over-pull the bus.

Phase 2 deliberately kept this bus free of temperature sensors (RS485 RTD handled all
temperature sensing at the time). That still holds for the primary coolroom probe and the
evaporator probe (both RTD, RS485). The external/ambient reading has since moved off RS485
entirely: the dedicated second RTD board (RS485 slave 101) that used to cover it has been
decommissioned, and SHT20 (this bus) covers that same ambient role now — plus humidity, which
RTD never could provide at all.

Cable-length caution: the external (SHT20) sensor's cable run leaves the enclosure. Standard
100kHz I2C over an unshielded multi-meter cable can become unreliable; keep the run as short as
practical, use twisted/shielded cable if it must be long, and treat `SHT20 (External Humidity)
Online` (diagnostics) as the signal that the run is marginal if it flaps.

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
| Signal        | GPIO | Direction   | Note                                        |
|---------------|------|-------------|---------------------------------------------|
| I2S MCLK      | 13   | ESP → codec | Master clock to ES8311                      |
| I2S SCLK      | 12   | ESP → codec | Serial (bit) clock                          |
| I2S LRCK      | 10   | ESP → codec | Left/Right channel select                   |
| I2S DSDIN     | 9    | ESP → codec | Playback data — ESPHome `i2s_dout_pin`      |
| I2S ASDOUT    | 11   | codec → ESP | Mic/ADC data — ESPHome `i2s_din_pin`        |
| PA_Ctrl       | 53   | ESP → amp   | NS4150B amplifier enable (HIGH=on)          |

Source: Waveshare ESP-IDF tutorial, section 6 I2S Audio Example.  
⚠️ **GPIO 9-13 and GPIO 53 are dedicated to on-board audio. Never reuse for RS485 or other peripherals.**  
⚠️ **DSDIN/ASDOUT are named from the ES8311's point of view and therefore invert
relative to ESPHome's `dout`/`din`, which are named from the ESP's point of view.
Wiring playback to GPIO11 (the codec's own output) yields correct clocks and amp
switching pops but total silence — the samples never reach the codec.**

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
The C6 module connects to SDMMC host **slot 1** on the ESP32-P4 (GPIO matrix pins below).
The TF card uses **slot 0** (GPIO 39-44, IO-MUX). Confirmed against Waveshare BSP
`esp32_p4_wifi6_touch_lcd_7b` (`bsp_sdcard_mount` → `SDMMC_HOST_SLOT_0`) and Espressif
`host_sdcard_with_hosted`. Do not put both on the same slot.

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

### PH2.0 12PIN Header Map — P1 and P3 (item 24)

Source: board schematic PDF, connectors **P1** and **P3** (`PH2.0连接器12P`), pin-number
designators `PIP101…PIP1012` / `PIP301…PIP3012`
(`reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, page 1). This is the **actual per-pin order**,
not a net-availability list. The 7 GPIOs on P1 plus the 10 on P3 are exactly the
17 programmable GPIOs Waveshare advertises for this board.

| Pin | P1 | P3 |
| --- | --- | --- |
| 1  | GPIO52 | GPIO36 ⚠️ strapping |
| 2  | GPIO51 | GPIO34 ⚠️ strapping / JTAG select |
| 3  | GPIO50 | GPIO31 |
| 4  | GPIO49 | GPIO30 |
| 5  | GPIO48 | GPIO29 |
| 6  | GPIO47 | GPIO28 |
| 7  | **GPIO46 — door reed** | GPIO5 |
| 8  | **GND — door reed return** | GPIO4 |
| 9  | ESP_LDO_VO4 | GPIO3 |
| 10 | ESP_3V3 | GPIO2 |
| 11 | GND | GND |
| 12 | BAT | ESP_3V3 |

⚠️ **GPIO34 / GPIO36 are ESP32-P4 strapping pins** (datasheet §3, p.36: GPIO35–GPIO38 set
boot mode, GPIO34 selects the JTAG source and has *no internal pull resistors*). Do not use
either for field I/O — pulling them at reset changes boot behaviour.

**GPIO20 is not on either header.** It is the on-board battery-sense divider
(`BAT → R92 → GPIO20 → R93 → GND`); see the reserved table below.

**Neither 12-pin header carries 5V.** `Core_5V` is only on the 4-pin headers
**H9** (pin 1 `Core_5V`, 2 `GND`, 3 `D_SDA`, 4 `D_SCL` — external I2C) and
**H11** (pin 1 `Core_5V`, 2 `GND`, 3 `CANH`, 4 `CANL` — CAN). The RS485 header is
**H10** (1 `VCC`, 2 `GND`, 3 `A`, 4 `B`).

### ⭐ Field-I/O GPIO allocation — check here before assigning a pin

This is the single source of truth for "what pins can I use on THIS board." Update it in the
same commit whenever a header GPIO is claimed or freed.

| GPIO | Header pin | Status | Used by |
| --- | --- | --- | --- |
| GPIO46 | P1-7 | 🔴 in use | Door reed sensor (return on P1-8 GND) |
| GPIO47 | P1-6 | 🟢 free | — |
| GPIO48 | P1-5 | 🟢 free | — |
| GPIO49 | P1-4 | 🟢 free | — |
| GPIO50 | P1-3 | 🟢 free | — |
| GPIO51 | P1-2 | 🟢 free | — |
| GPIO52 | P1-1 | 🟢 free | — |
| GPIO2  | P3-10 | 🟢 free | — |
| GPIO3  | P3-9 | 🟢 free | — |
| GPIO4  | P3-8 | 🟢 free | — |
| GPIO5  | P3-7 | 🟢 free | — |
| GPIO28 | P3-6 | 🟢 free | — |
| GPIO29 | P3-5 | 🟢 free | — |
| GPIO30 | P3-4 | 🟢 free | — |
| GPIO31 | P3-3 | 🟢 free | — |
| GPIO34 | P3-2 | ⛔ do not use | ESP32-P4 strapping / JTAG-select pin |
| GPIO36 | P3-1 | ⛔ do not use | ESP32-P4 strapping (boot-mode) pin |

Grounds available for two-wire field devices: P1-8, P1-11, P3-11. 3.3V on P1-10 / P3-12.
No 5V on these headers — see the H9/H11 note above.

**Before claiming a "free" pin**, still open the schematic and confirm the pin appears on P1
or P3 with no other net attached (this is exactly the check that GPIO20 failed). A GPIO number
existing does not mean the pad is broken out or unshared on this board.

### Supported Header Voltages

| Rail / Signal Type | Nominal Voltage | Notes |
| --- | --- | --- |
| ESP_3V3 | 3.3V | Logic rail for ESP32-P4 GPIO domain |
| Core_5V | 5.0V | Power rail only (peripheral supply) |
| GPIO signal level | 3.3V logic | Treat GPIO as 3.3V-only; do not drive above 3.3V |
| GND | 0V | Common reference |

### Wiring Guidance

- Use `ESP_3V3` (P1 pin 10 / P3 pin 12) for 3.3V sensors/logic interfaces.
- `Core_5V` is not on P1/P3 — take 5V from H9 pin 1 or H11 pin 1, and only when the attached module has 3.3V-compatible I/O (or proper level shifting).
- For digital inputs (e.g. the door reed on GPIO46), wire the switch between GPIO and GND when using `INPUT_PULLUP`. On P1 that is pins 7 and 8, which are adjacent — a plain two-wire tail.

### Project Power Architecture (DIN PSU)

This project uses two DIN-mounted power supplies:

| Supply | Primary Loads |
| --- | --- |
| 5V DIN PSU | ESP32-P4 controller via PH2.0 12PIN header (`Core_5V` + `GND`) |
| 12V DIN PSU | RS485 RTU-4 relay module and RS485 RTD PT100 modules |

### Modbus RTD hardware — 2-channel PT100 → RS485 (coolroom build)

**Canonical product sheets (operator photos of the module label):**

| File | Contents |
| --- | --- |
| `reference/PT100-RS485-2CH-terminals.png` | Specs + 12-terminal pinout + 2/3-wire PT100 wiring |
| `reference/PT100-RS485-2CH-modbus.png` | Modbus RTU register map + CH1 read example |
| `reference/PT100-RS485-2CH-modbus-correction.png` | CH2 read + on-module temp correction (regs 03/04) |
| `reference/PT100-RS485-2CH-modbus-addr-baud.png` | Change device address (reg 05) + baud code (reg 06) |

This is a **two-channel** DIN-rail transmitter: one Modbus slave, **RT1 + RT2**.
Firmware already matches that model (`rtd_board_1` addr **100**, holding regs **1** / **2**,
scale **0.1**, universal addr **249**).

Related PDFs under `reference/` (`RTD.pdf`, `BRT-PT100AD-…pdf`) describe Brightwin
**BRT PT100AD**-family converters (often **single**-channel, 8-terminal). Prefer the
**2CH photos** for this coolroom board.

| Spec | Value (from product sheet) |
| --- | --- |
| Input | **2-wire or 3-wire PT100** (3-wire recommended) — **two channels** |
| Range | Typical **−199 … +600 °C**; max **−199 … +650 °C** |
| Accuracy | **±0.2 °C** (marketing sheet also cites max error ±0.5 °C) |
| Supply | **7–30 VDC**, operating current **&lt;50 mA**; **5V** terminal can power the module if DC unused |
| Bus | RS485 **Modbus RTU**, baud **2400–115200** (default **9600**), **8 data / 1 stop / no parity** |
| Address | **1–247**; default **100** (0x64); universal **249** (terminals sheet) |
| Nodes | **&gt;100** |
| Size / env | **95 × 36 × 47 mm**; operating **−25 … +65 °C** |
| OUT (term 10) | Isolation type: RS485 signal GND. Non-isolation: 5V out ON/OFF via **reg 07** |

**Holding registers** (function **03** read / **06** write where R/W; sheet wording “byte” ≈ bit width):

| Reg | Function | Access | Notes |
| --- | --- | --- | --- |
| **00** | Broadcast / discover | R | With FC 03 returns device address |
| **01** | Channel **1** temperature | R | Signed 16-bit; **÷10 → °C** |
| **02** | Channel **2** temperature | R | Signed 16-bit; **÷10 → °C** |
| **03** | Channel **1** temp correction | R/W | Default **65** (0x41); range **0–99** |
| **04** | Channel **2** temp correction | R/W | Same scale as reg 03 (sheet page 3). Earlier overview labelled this “data check” — treat **04** as CH2 correction per the worked example |
| **05** | Device address | R/W | Default **100** / 0x64; range **1–255**; takes effect immediately (no reboot). Unknown addr → inquire via broadcast **0** |
| **06** | Baud rate code | R/W | Default **03** = **9600**; takes effect immediately (no reboot). See baud table below |
| **07** | 5V OUT control | R/W | Non-isolation type only; default 0 = off |

**Baud codes (write to reg 06):**

| Code | Baud |
| --- | --- |
| 01 | 2400 |
| 02 | 4800 |
| **03** | **9600** (default) |
| 04 | 19200 |
| 05 | 38400 |
| 06 | 57600 |
| 07 | 115200 |

**Config write examples** (FC **06**, current addr 100): set address → **1**:
`64 06 00 05 00 01 51 FE`; set baud → **2400**: `64 06 00 06 00 01 A1 FE`.
Echo reply = success. Coolroom keeps module at **addr 100 / 9600** to match
`modbus_rtd1_address` and the RS485 UART — only change these if the UART is updated
in lockstep (sheet note “register address is 03” under baud is a typo; the command uses
**reg 06**).

**Read examples** (addr 100, same ÷10 decode):

| Channel | Send | Temp payload |
| --- | --- | --- |
| CH1 | `64 03 00 01 00 01 DC 3F` | e.g. `01 0C` → 26.8 °C |
| CH2 | `64 03 00 02 00 01 2C 3F` | same decode |

Firmware uses the same scale (`rtd_temp_scale: "0.1"`).

**On-module correction** (write FC **06**): e.g. CH1 → `64 06 00 03 00 41 B0 0F`
(sets correction **65**). Higher correction → **lower** reported °C; lower → higher.
Coolroom firmware leaves module correction at factory default and applies operator
offsets in software (`probe1_offset_c` / `probe2_offset_c`) — do not write regs 03/04
from the controller unless deliberately migrating calibration into the module.

**Terminals (12-screw module — see terminals photo):**

| # | Mark | Use |
| --- | --- | --- |
| 1 | RT1+ | Channel 1 PT100 + |
| 2 / 3 | RT1− | Channel 1 PT100 − (pair for 3-wire) |
| 4 | RT2+ | Channel 2 PT100 + |
| 5 / 6 | RT2− | Channel 2 PT100 − (pair for 3-wire) |
| 7 / 8 | DC+ / DC− | 7–30 VDC supply |
| 9 | 5V | 5V output (or 5V **input** if 7–30 V unused) |
| 10 | OUT | See table above (GND vs controllable 5V out) |
| 11 | A/D+ | RS485 **A** |
| 12 | B/D− | RS485 **B** |

**3-wire (preferred):** same-colour pair → both RT− screws of that channel (e.g. #2+#3 for
CH1); different-colour lead → RT+ (e.g. #1).  
**2-wire:** one lead to RT+, one to RT−; **short the two RT− screws** of that channel
(e.g. jump #2–#3 for CH1, #5–#6 for CH2).

### Modbus RTD probe roles (firmware) — fixed, do not swap

One 2CH module at slave address **100**:

| Channel / role | Physical placement | Firmware |
| --- | --- | --- |
| **Probe 1 — Coolroom room air** | Air probe in the coolroom → **RT1** | Holding reg **1** → `rtd1_ch1_raw` |
| **Probe 2 — Evaporator coil** | On / in the evaporator coil → **RT2** | Holding reg **2** → `rtd1_ch2_raw` |

These **roles** are fixed (do not swap air and coil in software). Wire the correct PT100
to the correct channel terminals.

- **Probe 2 Enabled** (settings) can turn the evaporator channel off for sites that only
  fit one RTD — that disables ice / smart-defrost features that need coil temp; it does
  **not** remake CH2 into the room sensor.
- SHT31 (I2C) remains the **cabinet humidity / auxiliary temp** sensor (dew-point /
  frost-rate / auto-cal reference). SHT20 is **ambient** outside the room. Neither
  replaces Probe 1 or Probe 2.

### Modbus relay coil roles (firmware)

Waveshare Modbus RTU Relay 4-CH, slave address **1**:

| Coil | Role | Enable default | Behaviour |
| --- | --- | --- | --- |
| 0 | Evaporator **Fan** | Off | Follows compressor when enabled; forced off during defrost + drip |
| 1 | **Compressor** | On | Hysteresis / fallback / door-hold |
| 2 | **Light** | On | Manual + optional door-triggered |
| 3 | **Siren** | On | Alarm output |

Defrost is always **passive** on this controller (compressor held off; no heater coil).
See `reference/USER_MANUAL.md` §4.13 and `reference/CAREL_CONTROL_DECISIONS.md`.

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
Shares I2C bus with external header (GPIO 7/8). GT911 default I2C address: 0x5D
(alternate 0x14 — Waveshare BSP probes both).

| Signal | GPIO | Status |
|--------|------|--------|
| SDA    | 7    | ✅ Shared with I2C bus |
| SCL    | 8    | ✅ Shared with I2C bus |
| INT    | 23   | Schematic net NLGPIO23 → INT_TP exists |
| RST    | 33   | Schematic net NLGPIO33 → RESET_TP, **shared with LCD reset** |

**Official Waveshare BSP (`waveshare/esp32_p4_wifi6_touch_lcd_7b`, used by
[ESP32-P4-WIFI6-Touch-LCD-7B examples](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B)):**

```c
#define BSP_LCD_RST           (GPIO_NUM_33)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_NC)   // do NOT drive touch reset separately
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_NC)   // do NOT use INT — I2C poll only
```

So even though the schematic has INT_TP / RESET_TP nets, **vendor firmware treats
touch RST and INT as not connected**. LCD reset on GPIO33 alone resets the GT911;
a second post-init pulse on that shared line hard-resets the panel without re-init
(black screen in ESPHome). Touch is brought up by probing I2C `0x5D` then `0x14`,
with `mirror_x`/`mirror_y` both set. Match that in ESPHome: **no** `reset_pin`,
**no** `interrupt_pin`, poll via `update_interval`, keep mirrors.

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
| 7–8        | I2C bus (GT911, external hdr / SHT) |
| 9–13       | On-board I2S audio (ES8311 / mics) |
| 14–19      | ESP32-C6 SDIO co-processor         |
| 20         | Battery voltage sense divider      |
| 23         | GT911 touch INT                    |
| 26         | RS485 UART RX                      |
| 27         | RS485 UART TX                      |
| 32         | Display backlight BL_CTRL          |
| 33         | LCD + touch RST (shared)           |
| 34–38      | ESP32-P4 strapping pins (boot/JTAG)|
| 39–44      | TF card SDMMC (slot 0)             |
| 53         | Audio amplifier enable             |
| 54         | ESP32-C6 reset                     |

**GPIO 20 — battery sense, not free I/O.** The schematic wires it as the mid-point of a
resistor divider off the battery rail (`BAT → R92 → GPIO20 → R93 → GND`), so it is an ADC
input for pack voltage. It is also not brought out to P1 or P3. Firmware used it for the
door reed until 2026-07-31; that was a documentation error, corrected to GPIO46.

---

## ✅ USER GPIO — Field I/O Devices

| GPIO | Function           | Device          | Header       | Status           |
|------|--------------------|-----------------|--------------|------------------|
| 46   | Door reed sensor   | Normally-closed | P1 pin 7     | ✅ INPUT_PULLUP  |
| —    | Door reed return   | GND             | P1 pin 8     | ✅               |

**GPIO 46 — Door Reed Sensor:**
- Datasheet pin 88, plain `IO` pad in the `VDD_IO_5` domain, with no "At Reset"/"After
  Reset" default function and no analog or LP-IO mux — so it is safe to hold low or open
  through a reset. Its only alternate function is EMAC RMII group 2, which this board does
  not populate (no Ethernet PHY).
- Wired to P1 pin 7 with the reed return on the adjacent P1 pin 8 (`GND`)
- Set in `esp32-p4-coolroom.yaml` via the `door_reed_pin_num` substitution
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

4. **Mandatory: Verify RS485 UART pins before RS485 devices work.** The relay board and RTD sensor require the correct UART TX/RX pin assignments.

5. **Hosted SDIO + TF card slots (critical).** ESP-Hosted C6 must stay on SDMMC **slot 1**
   with GPIO 14-19 (`CONFIG_ESP_HOSTED_SDIO_SLOT_1`). The TF card must use SDMMC **slot 0**
   (GPIO 39-44). Putting the TF mount on slot 1 (as an earlier revision of `p4_logging.h`
   did) collides with hosted and produces `H_API: ESP-Hosted link not yet up` / reset loops
   when Wi-Fi starts. Forcing `esp32_hosted.slot: 0` in ESPHome remaps hosted pins to 39-44
   in generated `sdkconfig.h` and is wrong for this board — leave hosted at default slot 1.

6. **Waveshare ESP-IDF hosted baseline (11_esp_brookesia_phone).** Example `sdkconfig` sets: reset active high, reset GPIO 54, 4-bit SDIO, 40MHz clock, CMD/CLK/D0..D3 = 19/18/14/15/16/17. Use this as the first-pass reference before changing hosted settings.

7. **Board exposes two USB connections — use both simultaneously during bring-up.** Confirmed on real hardware (2026-07-29/30): this board presents two separate `/dev/cu.usbmodem*` entries on macOS when connected (e.g. `usbmodem213401` and `usbmodem5B7B0287481` in one session — exact suffixes are host-assigned and will differ across reconnects/machines). Both queried identically via `esptool.py flash_id` (same chip type, same MAC `e8:f6:0a:e0:8f:52`) — this is the same physical ESP32-P4 exposing itself twice, not two boards. Practical workflow: connect both ports and dedicate one to flashing (`esphome upload` / `esptool.py write_flash`) and the other to a persistent logging/monitor session (`esphome logs` or a raw serial terminal), so you don't have to tear down and re-open a monitor session after every reflash. To identify which port is free, `esptool.py --port <path> flash_id` is safe/read-only and won't disturb a session already open on the other port.

8. **ESP32-P4 engineering sample flag.** If your board has chip revision < 3.0, set `engineering_sample: true` in the ESPHome `esp32:` block. Production boards ship with revision ≥ 3.0 and do not need this flag.
