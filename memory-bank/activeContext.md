# Active Context — Current Session State

**Date:** 2026-07-30 (night)
**Session:** Emergency display recovery after `79758c6` black/delayed screen + dead touch.
**Status:** Recovery firmware **compiled + flashed** (config hash `0x698d1304`). Serial shows
device in main loop. **Awaiting operator visual confirm** that the 7" panel and touch come up
promptly (seconds, not minutes). WiFi still disabled on boot. Changes **not committed**.

## What Broke

Commit `79758c6` re-added GT911 `reset_pin` on shared GPIO33 after mipi_dsi init. ESPHome
pulses that pin in `gt911_touchscreen.cpp::setup()`, hard-resetting the LCD with no re-init →
blank display; touch also dead in that window. Operator also reported blank lasting up to ~6
minutes on a prior boot (not always permanent).

## Fix Applied (working tree)

- Removed shared `reset_pin` from `display:` / `touchscreen:`
- `p4_gt911_prepare_for_lcd_reset()` in `p4_helpers.h` + `on_boot` priority 950 (hold INT
  GPIO23 low before LCD reset straps addr 0x5D)
- Kept mirror_x / mirror_y
- Flashed via `/dev/cu.usbmodem5B7B0287481`; monitor on `/dev/cu.usbmodem213401` (JTAG)

## Immediate Next Actions

1. Operator: confirm display + touch within a few seconds of boot/power-cycle.
2. If OK: commit recovery fix; leave WiFi disabled.
3. WiFi: delayed `wifi.enable` experiment only after UI confirmed; hosted still logs
   `ESP-Hosted link not yet up` even with `enable_on_boot: false`.
4. Still open: SD mount/log-manager `opendir` warnings; Phase 2 RS485; screenshot docs.

## Manuals

No USER_MANUAL / QUICK_START update needed — internal bring-up only.
