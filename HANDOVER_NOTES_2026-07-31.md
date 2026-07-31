# Handover Notes — 2026-07-31

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Closeout:** compile / flash / graph / compact for HA gate, probe live readings, header
clock, web alarm-strip removal, bell soft-mute UX, and related audio/Wi‑Fi/control work
already in the working tree.

## Resume checklist

1. Confirm after flash: LVGL header shows **date left**, **12h time right**.
2. With an alarm active: bell jiggles red → tap → still red, no jiggle → clear condition →
   grey; or fire a *second* alarm type while muted → jiggle resumes.
3. Web: Coolroom Status shows alarms on the gauge; **no pink strip** above it.
4. Wireless tab: **Home Assistant API Enabled** off by default; turn on only to add ESPHome
   in HA (encryption key from `secrets.yaml`).
5. Probes tab (operator): Raw / Offset / Corrected table (raw stays `--` until RTD online).
6. **Door reed is now GPIO46 = header `P1` pin 7**, ground return on `P1` pin 8 (adjacent).
   It was GPIO20, which is the board's battery-sense divider and is not on any header — the
   old wiring instruction was impossible to follow. Wire a switch across P1 7–8 and check the
   Door Reed Sensor entity toggles. Unwired, `INPUT_PULLUP` floats high = "open" in NC mode.

## Bench (unchanged)

RS485 relay/RTD and external I2C sensors **not connected** — Modbus offline expected.

## Key paths

| Area | Files |
|------|--------|
| HA gate | `esp32-p4-coolroom.yaml` (`sw_ha_api_enabled`), `p4_helpers.h` (`p4_ha_api_drop_clients`) |
| Bell mute mask | `p4_control.h` (`p4_ctl_alarm_mask`), yaml globals + 50 ms icon tick |
| Probe raw | `probe1_temp_raw` / `probe2_temp_raw` + `assets/dashboard.html` |
| Audio | `p4_audio.yaml`, `assets/audio/*.wav`, `reference/AUDIO_ALERTS.md` |
| Door reed pin | `esp32-p4-coolroom.yaml` (`door_reed_pin_num`), `reference/hardware_pins.md` (P1/P3 map) |
| Hardware refs | `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, `.cursor/rules/waveshare-hardware-check.mdc` |

## Commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
./tools/esphome_flash.sh --device /dev/cu.usbmodem5B7B0287481   # preserves settings
.venv/bin/python tools/check_dashboard_coverage.py
./tools/code_review_graph_cli.sh update --repo .
./tools/code_review_graph_cli.sh status
```

> ⚠️ **Do not use `esphome upload` over USB unless you want defaults.** It writes
> `firmware.factory.bin` from `0x0`, padding `0xFF` straight over the NVS partition at
> `0x9000`–`0x15000`, so every saved setting is erased and globals fall back to their
> `initial_value`. Use `tools/esphome_flash.sh` (serial) or OTA to an IP instead. Plain
> reboots are safe — persistence itself works.

See also: `reference/session_recaps.md` (2026-07-31 HA Gate entry), `memory-bank/activeContext.md`.
