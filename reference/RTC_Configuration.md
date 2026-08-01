# Wall-clock time — ESP32-P4 LP RTC + SNTP

Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B

**Item:** 11 — onboard **RTC battery holder** (wiki / docs), typically a rechargeable
1220 (3.0–3.3 V). This backs **SoC VBAT**, not an I2C RTC chip.

**Chip ID on the silkscreen / docs:** Waveshare does **not** name PCF8563, DS3231, or
any other I2C RTC part for this board.

## Waveshare FAQ model (authoritative)

**System Time API:** initialize the chip's internal **48-bit low-power RTC** via
`settimeofday()` / `esp_time_impl_set_boot_time()` (and standard `<time.h>` / POSIX
time). ESPHome’s SNTP platform does this on sync.

**Network Sync:** Wi-Fi co-processor (ESP32-C6) + NTP so the LP RTC stays accurate
after boot / long runs.

System time bridges high-resolution hardware timers while running and falls back to
the LP RTC when asleep or after reset (with VBAT if a cell is fitted).

## Firmware behaviour (this project)

- **No** `time: platform: pcf8563` — removed 2026-08-01 after bench I2C scan showed no
  ACK at `0x51` / `0x68`, matching Waveshare docs.
- Boot: if `p4_wall_clock_ok()` (VBAT already held wall time), NTP uses the long
  `ntp_with_rtc_sync_ms` interval; otherwise fast sync until the first SNTP
  `settimeofday()`.
- Diagnostics binary sensor: **System Time Valid** (`system_time_valid`) — true when
  the wall clock is valid, not “I2C RTC online”.
- Optional cell in holder item 11 improves survival across power cuts; without it,
  expect “Time: pending” until Wi‑Fi/NTP after every hard power loss.

## Verification

1. After Wi‑Fi joins, Info / Hardware should show **System Time** online and the
   home clock should match local time.
2. With a charged cell in item 11: power-cycle, confirm the clock is already valid
   **before** the first NTP sync (SoC VBAT path).
3. Do **not** treat a missing I2C address `0x51` as a fault — that chip is not part
   of this design.

## Related

- `reference/hardware_pins.md` — I2C device table (timekeeping is not I2C)
- Waveshare board wiki / FAQ for ESP32-P4-WIFI6-Touch-LCD-7B
- ESP-IDF timekeeping / SNTP docs
