# Handover Notes — 2026-07-31

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Closeout:** persistence live-proven; compile + NVS-safe flash now mandatory at every
firmware closeout. Use `./tools/esphome_flash.sh`, never plain USB `esphome upload`.

## Resume checklist

Verified on hardware 2026-08-01:
1. ✅ LVGL header: **date left**, **12h time** right.
2. ✅ Alarm bell: tap stops jiggle, stays red while alarm active.
3. ✅ Home Assistant entities populated (after reload / re-add).
4. ✅ Door reed on **GPIO46 = P1 pin 7**, return on P1 pin 8.

Still open:
5. Web: Coolroom Status shows alarms on the gauge; **no pink strip** above it (spot-check).
6. Probes tab (operator): Raw / Offset / Corrected table (raw stays `--` until RTD online).
7. Settings survive **reboot** (live-tested). They do **not** survive a USB factory flash.
8. Optional: fire a *second* alarm type while muted → confirm jiggle resumes.

## Bench (unchanged)

RS485 relay/RTD and external I2C sensors **not connected** — Modbus offline expected.

## Key paths

| Area | Files |
|------|--------|
| HA gate | `esp32-p4-coolroom.yaml` (`sw_ha_api_enabled`), `p4_helpers.h` (`p4_ha_api_drop_clients`) |
| HA entity list check | `tools/list_ha_entities.py --host <ip>` (what HA actually sees over the API) |
| Bell mute mask | `p4_control.h` (`p4_ctl_alarm_mask`), yaml globals + 50 ms icon tick |
| Probe raw | `probe1_temp_raw` / `probe2_temp_raw` + `assets/dashboard.html` |
| Door reed pin | `esp32-p4-coolroom.yaml` (`door_reed_pin_num`), `reference/hardware_pins.md` (P1/P3 map) |
| Flash (NVS-safe) | `tools/esphome_flash.sh` |
| Coverage / persist contract | `tools/check_dashboard_coverage.py` |
| Live reboot smoke | `tools/test_settings_persistence.py` |
| Hardware refs | `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, `.cursor/rules/waveshare-hardware-check.mdc` |

## Commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
./tools/esphome_flash.sh --device /dev/cu.usbmodem5B7B0287481   # preserves settings
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
.venv/bin/python tools/list_ha_entities.py --host 192.168.37.237        # HA sees N entities?
./tools/code_review_graph_cli.sh update --repo .
./tools/code_review_graph_cli.sh status
```

> ⚠️ **Do not use `esphome upload` over USB unless you want defaults.** It writes
> `firmware.factory.bin` from `0x0`, padding `0xFF` straight over the NVS partition at
> `0x9000`–`0x15000`, so every saved setting is erased and globals fall back to their
> `initial_value`. Use `tools/esphome_flash.sh` (serial) or OTA to an IP instead. Plain
> reboots are safe — persistence itself works (proven 2026-07-31).

See also: `reference/session_recaps.md`, `memory-bank/activeContext.md`.
