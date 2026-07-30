# Active Context — Current Session State

**Date:** 2026-07-30 (late night)
**Branch:** `cursor/wifi-sdmmc-slot-fix`
**Status:** WiFi/SDMMC slot fix already on branch (`dd53feb`). Follow-on web/LVGL work
committed as **`b39a64e`**, flashed config hash **`0x9b1da7d5`**.

## Working well

- Display/touch usable; WiFi on boot with TF on SDMMC **slot 0**, hosted C6 on **slot 1**
- Coolroom dashboard served at `/` (handler prepended over ESPHome stock UI)
- Guest web view: gauge + timers + health; settings hidden until Login
- Login uses SHA-256 of `secrets.yaml` credentials baked at embed time — **must use
  `sha256HexSync` fallback** because `crypto.subtle` is unavailable on plain `http://`

## Just fixed / still verify on hardware

1. **Web Login over HTTP** — pure-JS SHA-256 fallback (was silently failing with SubtleCrypto)
2. **LVGL settings blue +/- / Toggle** — larger 100×56 / 240×56 controls, value in panel chip
3. **Home header** — page + header scrollable; time | date | wifi icon fitted to 1024 width;
   connected WiFi icon **cyan** (not green); dBm text stays on Info page
4. **Backup.json** — remount/retry before write (still needs a mounted writable SD)

## Leave for next session

- Re-check LVGL settings blue control sizing after flash (operator feedback loop)
- Confirm Login on `http://<device-ip>/` with current `secrets.yaml` password
- Web GUI visual pass vs `assets/dashboard_virtual_preview.html` (deferred — leave gating as-is)
- RTC chip still **unconfirmed** (SNTP-only; PCF8563@0x51 assumption only)
- SD log manager `opendir`/`readdir` linker warnings
- RS485/Modbus hardware validation
- Manual screenshots still placeholders

## Key commands

```bash
python3 scripts/embed_dashboard.py   # after dashboard.html or secrets web password change
./tools/esphome_compile.sh esp32-p4-coolroom.yaml
.venv/bin/esphome upload esp32-p4-coolroom.yaml --device /dev/cu.usbmodem5B7B0287481
```

## Manuals

USER_MANUAL / QUICK_START / RBAC / AUTHENTICATION updated for guest vs Login and `/` dashboard.
No further manual edit needed for this closeout beyond handover.
