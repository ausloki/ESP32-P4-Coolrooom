# Phase 5 Static Audit — 2026-07-23

## Scope

This audit reviews the current non-hardware Phase 5 implementation in the ESP32-P4 firmware and supporting docs.

Board context: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B with SDMMC slot 1 on GPIO 39-44 and I2C on GPIO 7/8.

## Implemented

### SD card logging

- SD card mount/unmount helpers exist in `p4_logging.h` and target SDMMC slot 1 on GPIO 39-44.
- Daily temperature logging is implemented through `p4_sd_log_temps()`.
- Daily CSV files are currently written to `/sdcard/YYYY-MM-DD.csv`.
- Event logging is implemented through `p4_sd_log_event()` and writes to `/sdcard/events.csv`.
- Free-space reporting is implemented through `p4_sd_free_mb()` and exposed through `sd_free_mb`.
- SD-card health is exposed through the `sd_card_ok` global and `sd_card_online` binary sensor.

### Backup / restore

- Manual backup to SD is implemented through `btn_sd_backup`.
- Manual restore from SD is implemented through `btn_sd_restore`.
- Boot-time restore attempt is implemented in `on_boot` priority `-200`.
- The backup file path is `/sdcard/backup.json`.

### ntfy notifications

- The HTTP client and ntfy scripts are implemented.
- Edge-triggered notification flow exists in the 10-second control loop.
- Implemented notification types:
  - high temperature alarm
  - low temperature alarm
  - alarm clear
  - probe fault
- Offline-safe behavior is implemented: notification edge flags reset while Wi-Fi is unavailable.

## Partial / Limited

### Backup scope is narrower than the Phase 5 description implies

- Current backup/restore only covers four values:
  - `setpoint`
  - `comp_diff`
  - `alarm_high`
  - `alarm_low`
- It does **not** currently persist the wider Phase 6 control parameter set such as lockout, defrost timing, alarm persist, fallback, door delay, no-cool, or ice thresholds.

### ntfy coverage is narrower than the full alarm surface

- Current ntfy coverage is limited to high alarm, low alarm, clear, and probe fault.
- There is no notification script for:
  - no-cool alarm
  - ice alarm
  - door alarm
  - defrost start/end summaries
  - backup/restore success/failure

## Not Present In Current Firmware

- No web-based log export workflow was found in the firmware or dashboard integration.
- No OTA firmware update flow from the web dashboard was found.
- No SD-backed bulk export or browse workflow for event logs was found.

## Documentation Drift Corrected

- Historical recap text previously said daily CSV logging wrote to `/sdcard/logs/YYYY-MM-DD.csv`.
- Current implementation writes to `/sdcard/YYYY-MM-DD.csv`.
- That mismatch was corrected in `reference/session_recaps.md` during this audit pass.

## Non-Hardware Next Steps

1. Decide whether Phase 5 backup/restore should remain minimal or be expanded to cover the broader control parameter set.
2. Decide whether ntfy should remain limited to high/low/probe fault or cover additional alarm classes.
3. Decide whether log export / dashboard download support is still in scope for Phase 5.
4. Reconcile any remaining roadmap language that still implies Phase 5 is fully complete.

## Hardware-Gated Follow-Up

The following still require the physical ESP32-P4 board and are not resolved by this static audit:

- SD card mount and write behavior on real media
- backup/restore behavior on real media
- ntfy behavior across real Wi-Fi disconnect/reconnect conditions
- RTC identity confirmation by I2C scan or chip marking inspection
