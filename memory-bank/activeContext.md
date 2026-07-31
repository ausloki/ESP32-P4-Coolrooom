# Active Context — Current Session State

**Date:** 2026-07-31  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Persistence live-verified (10/10 settings survived reboot). Closeout now
requires compile + NVS-safe flash. Use `./tools/esphome_flash.sh`, never plain USB
`esphome upload`.

## Working well

- Display/touch, WiFi, dashboard at `/` with SSE live data
- Soft-mute + HA opt-in; door reed on **GPIO46** (P1-7)
- Settings persistence across **reboot** confirmed live
- Coverage checker + persist-script staging check green

## Verify on hardware

1. Header: date left, 12h AM/PM time right
2. Bell mute: red + still; new alarm type re-jiggles
3. Wireless → Home Assistant API Enabled (default OFF) — turn on before adding to HA
4. Probes tab live Raw/Offset/Corrected (raw needs RTD)
5. Door reed on GPIO46: wire across P1 pins 7–8
6. After any USB flash: confirm settings still present (or re-apply + Backup to SD)

## Leave for later

- RS485 / external I2C not on bench
- RTC PCF8563 still SNTP-primary until chip confirmed
- Manual screenshots still placeholders

## Key commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
# Settings-preserving flash. `esphome upload` over USB writes factory.bin from
# 0x0 and ERASES NVS (0x9000-0x15000) — that is why settings "came back".
./tools/esphome_flash.sh --device /dev/cu.usbmodem5B7B0287481
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
./tools/code_review_graph_cli.sh update --repo .
```

## Manuals

USER_MANUAL updated for HA gate symptom, guest-mode missing-settings, flash wipe,
Factory Reset WiFi wipe, Speaker Amplifier, Restart/Factory Reset on Hardware tab.
Handover: `HANDOVER_NOTES_2026-07-31.md`. Recap: `reference/session_recaps.md`.
