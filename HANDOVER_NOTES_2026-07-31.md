# Handover Notes — 2026-07-31 (updated 2026-08-16)

Latest bench / RS485 online-flag fix: **`HANDOVER_NOTES_2026-08-16.md`**.

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Closeout:** persistence live-proven; compile + NVS-safe flash (or OTA) mandatory at every
firmware closeout. Use `./tools/esphome_flash.sh` or ESPHome OTA to IP — never plain USB
`esphome upload`.

## Resume checklist

Verified on hardware 2026-08-01 (OTA item 11: 2026-08-03):
1. ✅ LVGL header: **date left**, **12h time** right.
2. ✅ Alarm bell: tap stops jiggle, stays red while alarm active; second alarm type
   re-jiggles (door eval no longer gated by probe fault).
3. ✅ Home Assistant entities populated (after reload / re-add).
4. ✅ Door reed on **GPIO46 = P1 pin 7**, return on P1 pin 8.
5. ✅ Web Coolroom Status: alarms on gauge centre status + bell — **no pink strip**.
6. ✅ Settings survive **reboot** (live-tested). They do **not** survive a USB factory flash.
7. ✅ Rotating multi-fault centre status + spoken alerts + ntfy hardware-offline pushes.
8. ✅ Compressor gated on RS485 relay board online; fallback ON window held while locked out.
9. ✅ LVGL Settings **1/8–8/8** menu on glass (Cory, evening) — Audio, Comp Min Run,
   Skip-If-Cold / Force-Max, Frost Rate, Ice enable+dwell, probe sources + calibrate.
10. ✅ Info page: Memory `|` / System Time `—` glyphs; SD used/free %; NTP sync age.
11. ✅ ESPHome OTA confirmed working (2026-08-03): `.venv/bin/esphome upload` → `192.168.37.237`; device returned; NVS preserved (ntfy stayed OFF).

Still open / hardware-gated:
- I2C SHT / CT clamp still not fitted.
- Manual screenshots still placeholders in USER_MANUAL / Quick Start (camera only).
- Dashboard OTA UI (web upload UI — separate from ESPHome CLI OTA above).

Already closed (do not re-chase):
- Carel A–D — `reference/CAREL_CONTROL_DECISIONS.md` (2026-08-01).
- Door Sensor Mode ≠ light relay (separate Door-Triggered Light switch).
- `opendir`/`readdir` SD listing — fixed via `esp32.disable_vfs_support_dir: false`.
- Virtual preview synced to live gauge + Audio / Alarms & Notify (2026-08-01).
- Web SD log download; multi-user auth not required; mic on hold.
- LVGL framebuffer screenshot component — declined.

## Bench (updated 2026-08-16)

RS485 relay (addr 1) and 2CH PT100 (addr 100) **are connected**. I2C SHT and CT
clamp are not. See `HANDOVER_NOTES_2026-08-16.md`.

## Key paths

| Area | Files |
|------|--------|
| HA gate | `esp32-p4-coolroom.yaml` (`sw_ha_api_enabled`), `p4_helpers.h` (`p4_ha_api_drop_clients`) |
| HA entity list check | `tools/list_ha_entities.py --host <ip>` |
| Bell mute mask | `p4_control.h` (`p4_ctl_alarm_mask`), yaml globals + 50 ms icon tick |
| Centre status | `p4_ui.h` (`p4_ui_home_status_text`) |
| Probe raw | `probe1_temp_raw` / `probe2_temp_raw` + `assets/dashboard.html` |
| Door reed pin | `esp32-p4-coolroom.yaml` (`door_reed_pin_num`), `reference/hardware_pins.md` |
| Flash (NVS-safe) | `tools/esphome_flash.sh` or ESPHome OTA to IP (confirmed 2026-08-03) |
| Coverage / persist | `tools/check_dashboard_coverage.py` |
| Live reboot smoke | `tools/test_settings_persistence.py` |
| Virtual preview | `assets/dashboard_virtual_preview.html` |
| Hardware refs | `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, `.cursor/rules/waveshare-hardware-check.mdc` |

## Commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
./tools/esphome_flash.sh --device /dev/cu.usbmodem213401   # preserves settings
.venv/bin/esphome upload esp32-p4-coolroom.yaml --device 192.168.37.237  # OTA (confirmed)
.venv/bin/python tools/check_dashboard_coverage.py
.venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
.venv/bin/python tools/list_ha_entities.py --host 192.168.37.237
./tools/code_review_graph_cli.sh update --repo .
```

> ⚠️ **Do not use `esphome upload` over USB unless you want defaults.** It writes
> `firmware.factory.bin` from `0x0`, padding `0xFF` straight over the NVS partition at
> `0x9000`–`0x15000`. Use `tools/esphome_flash.sh` (serial) or OTA to an IP instead.
> ESPHome OTA to LAN IP confirmed 2026-08-03 (NVS preserved).

See also: `reference/session_recaps.md`, `memory-bank/activeContext.md`.
