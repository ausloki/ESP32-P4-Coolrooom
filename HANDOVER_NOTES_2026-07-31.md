# Handover Notes — 2026-07-31 (updated 2026-08-01)

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Closeout:** persistence live-proven; compile + NVS-safe flash mandatory at every
firmware closeout. Use `./tools/esphome_flash.sh`, never plain USB `esphome upload`.

## Resume checklist

Verified on hardware 2026-08-01:
1. ✅ LVGL header: **date left**, **12h time** right.
2. ✅ Alarm bell: tap stops jiggle, stays red while alarm active; second alarm type
   re-jiggles (door eval no longer gated by probe fault).
3. ✅ Home Assistant entities populated (after reload / re-add).
4. ✅ Door reed on **GPIO46 = P1 pin 7**, return on P1 pin 8.
5. ✅ Web Coolroom Status: alarms on gauge centre status + bell — **no pink strip**.
6. ✅ Settings survive **reboot** (live-tested). They do **not** survive a USB factory flash.
7. ✅ Rotating multi-fault centre status + spoken alerts + ntfy hardware-offline pushes.
8. ✅ Compressor gated on RS485 relay board online; fallback ON window held while locked out.

Still open / hardware-gated:
- Probes tab Raw / Offset / Corrected (raw stays `--` until RTD online).
- End-to-end Modbus control once relay/RTD boards are fitted.
- Manual screenshots still placeholders in USER_MANUAL / Quick Start.
- Carel control decisions pending: asymmetric hysteresis, min ON-time, fan control,
  door-open cooling pause (see session_recaps 2026-07-25).

Already closed (do not re-chase):
- Door Sensor Mode ≠ light relay (separate Door-Triggered Light switch).
- `opendir`/`readdir` SD listing — fixed via `esp32.disable_vfs_support_dir: false`.
- Virtual preview synced to live gauge + Audio / Alarms & Notify (2026-08-01).

## Bench (unchanged)

RS485 relay/RTD and external I2C sensors **not connected** — Modbus offline expected.
Centre status will rotate RELAY/TEMP BOARD OFFLINE (and humidity/ambient if those
sensors are enabled).

## Key paths

| Area | Files |
|------|--------|
| HA gate | `esp32-p4-coolroom.yaml` (`sw_ha_api_enabled`), `p4_helpers.h` (`p4_ha_api_drop_clients`) |
| HA entity list check | `tools/list_ha_entities.py --host <ip>` |
| Bell mute mask | `p4_control.h` (`p4_ctl_alarm_mask`), yaml globals + 50 ms icon tick |
| Centre status | `p4_ui.h` (`p4_ui_home_status_text`) |
| Probe raw | `probe1_temp_raw` / `probe2_temp_raw` + `assets/dashboard.html` |
| Door reed pin | `esp32-p4-coolroom.yaml` (`door_reed_pin_num`), `reference/hardware_pins.md` |
| Flash (NVS-safe) | `tools/esphome_flash.sh` |
| Coverage / persist | `tools/check_dashboard_coverage.py` |
| Live reboot smoke | `tools/test_settings_persistence.py` |
| Virtual preview | `assets/dashboard_virtual_preview.html` |
| Hardware refs | `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, `.cursor/rules/waveshare-hardware-check.mdc` |

## Commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
./tools/esphome_flash.sh --device /dev/cu.usbmodem213401   # preserves settings
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
.venv/bin/python tools/list_ha_entities.py --host 192.168.37.237
./tools/code_review_graph_cli.sh update --repo .
```

> ⚠️ **Do not use `esphome upload` over USB unless you want defaults.** It writes
> `firmware.factory.bin` from `0x0`, padding `0xFF` straight over the NVS partition at
> `0x9000`–`0x15000`. Use `tools/esphome_flash.sh` (serial) or OTA to an IP instead.

See also: `reference/session_recaps.md`, `memory-bank/activeContext.md`.
