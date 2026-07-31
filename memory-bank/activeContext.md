# Active Context — Current Session State

**Date:** 2026-08-01  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Core hardware checks confirmed by Cory (HA entities, LVGL header, bell mute,
door reed GPIO46). Persistence live-verified earlier. Closeout requires compile +
NVS-safe flash (`./tools/esphome_flash.sh`, never plain USB `esphome upload`).

## Working well

- Display/touch, WiFi, dashboard at `/` with SSE live data
- Soft-mute + HA opt-in; door reed on **GPIO46** (P1-7) — confirmed on glass
- HA shows entities (after reload); LVGL date left / 12h time right — confirmed
- Settings persistence across **reboot** confirmed live
- Coverage checker + persist-script staging check green

## Still to verify / open

1. Optional: second alarm type while muted → jiggle resumes
2. Probes tab live Raw/Offset/Corrected (needs RTD on bench)
3. Web: no pink alarm strip above Coolroom Status (spot-check)
4. After any USB flash: confirm settings still present (or re-apply + Backup to SD)

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
