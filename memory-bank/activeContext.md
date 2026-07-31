# Active Context — Current Session State

**Date:** 2026-07-31  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Flashed config hash **`0xfa32a15e`** (dashboard build `20260731-2209`). Graph
updated; compact commit **`a61542e`**.

## Working well

- Display/touch, WiFi (TF SDMMC slot 0 / C6 slot 1), dashboard at `/` with SSE live data
- Guest gauge vs operator settings after Login
- Soft-mute + HA opt-in documented in USER_MANUAL / QUICK_START

## Verify on hardware after this flash

1. Header: date left, 12h AM/PM time right
2. Bell mute: red + still; new alarm type re-jiggles
3. No pink alarm strip above web Coolroom Status
4. Wireless → Home Assistant API Enabled (default OFF)
5. Probes tab live Raw/Offset/Corrected (raw needs RTD)

## Leave for later

- RS485 / external I2C not on bench
- RTC PCF8563 still SNTP-primary until chip confirmed
- Manual screenshots still placeholders
- SD log manager `opendir`/`readdir` linker notes if they recur

## Key commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
.venv/bin/esphome upload esp32-p4-coolroom.yaml --device /dev/cu.usbmodem5B7B0287481
./tools/code_review_graph_cli.sh update --repo .
```

## Manuals

Updated this closeout for HA API, probe live table, header clock, bell soft-mute.
Handover: `HANDOVER_NOTES_2026-07-31.md`. Recap: `reference/session_recaps.md`.
