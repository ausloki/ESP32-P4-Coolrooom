# Active Context — Current Session State

**Date:** 2026-07-31  
**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Flashed config hash **`0xb59981d8`**. Door reed moved off GPIO20 (battery sense)
to **GPIO46** = header `P1` pin 7, ground return on `P1` pin 8. Previous closeout was
compact commit **`a61542e`** (config hash `0xfa32a15e`); the pin fix is uncommitted on top.

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
6. Door reed on GPIO46: wire the switch across `P1` pins 7–8 and confirm the Door Reed
   Sensor entity follows it. Unwired it floats high = "open" in NC mode (expected).

## Leave for later

- RS485 / external I2C not on bench
- RTC PCF8563 still SNTP-primary until chip confirmed
- Manual screenshots still placeholders
- SD log manager `opendir`/`readdir` linker notes if they recur

## Key commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
# Settings-preserving flash. `esphome upload` over USB writes factory.bin from
# 0x0 and ERASES NVS (0x9000-0x15000) — that is why settings "came back".
./tools/esphome_flash.sh --device /dev/cu.usbmodem5B7B0287481
.venv/bin/python tools/check_dashboard_coverage.py
./tools/code_review_graph_cli.sh update --repo .
```

## Manuals

Updated this closeout for HA API, probe live table, header clock, bell soft-mute.
Handover: `HANDOVER_NOTES_2026-07-31.md`. Recap: `reference/session_recaps.md`.
