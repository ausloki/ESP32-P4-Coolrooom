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

## Bench (unchanged)

RS485 relay/RTD and external I2C sensors **not connected** — Modbus offline expected.

## Key paths

| Area | Files |
|------|--------|
| HA gate | `esp32-p4-coolroom.yaml` (`sw_ha_api_enabled`), `p4_helpers.h` (`p4_ha_api_drop_clients`) |
| Bell mute mask | `p4_control.h` (`p4_ctl_alarm_mask`), yaml globals + 50 ms icon tick |
| Probe raw | `probe1_temp_raw` / `probe2_temp_raw` + `assets/dashboard.html` |
| Audio | `p4_audio.yaml`, `assets/audio/*.wav`, `reference/AUDIO_ALERTS.md` |
| Hardware refs | `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf`, `.cursor/rules/waveshare-hardware-check.mdc` |

## Commands

```bash
python3 scripts/embed_dashboard.py
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
.venv/bin/esphome upload esp32-p4-coolroom.yaml --device /dev/cu.usbmodem5B7B0287481
./tools/code_review_graph_cli.sh update --repo .
./tools/code_review_graph_cli.sh status
```

See also: `reference/session_recaps.md` (2026-07-31 HA Gate entry), `memory-bank/activeContext.md`.
