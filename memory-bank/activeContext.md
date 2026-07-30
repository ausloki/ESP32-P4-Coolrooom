# Active Context — Current Session State

**Date:** 2026-07-30
**Session:** Hosted-link root-cause isolation for reboot loop, with controlled SDIO/wake experiments and Wi-Fi-on-boot isolation.
**Status:** BUILDABLE, DIAGNOSTIC MODE ACTIVE (`wifi.enable_on_boot: false` + delayed `wifi.enable`), HARDWARE VALIDATION IN PROGRESS

## Current Focus

- Confirm whether reboot loop starts exactly when hosted Wi-Fi is enabled.
- Keep all non-hosted logic unchanged while isolating the trigger path.

## What Was Verified This Session

- Multiple hosted tuning variants did not materially improve stability:
  - SDIO frequency: 40MHz, 20MHz, 10MHz
  - SDIO bus width: 4-bit and 1-bit
  - Wake pulse sequencing variant
- Common behavior under those variants:
  - repeated early `H_API` link-not-up events
  - `SW_CPU_RESET`
  - eventual safe-mode fallback
- Isolation test result:
  - with `wifi.enable_on_boot: false`, resets dropped to zero in the observation window
  - this strongly implicates hosted/Wi-Fi startup path as the reset trigger
- Host network observation:
  - `192.168.37.237` resolved to `dc:1e:d5:96:3e:d8` and responded to ping
  - TCP ports 80 and 6053 were refused at test time

## Current Firmware State

- `esp32_hosted` remains on Waveshare-aligned baseline:
  - active-high reset
  - 4-bit SDIO
  - 40MHz SDIO
  - default hosted slot behavior (no `slot: 0` override)
- Diagnostic toggles currently active:
  - `wifi.enable_on_boot: false`
  - delayed Wi-Fi runtime activation (`delay: 30s` then `wifi.enable`)

## Next Actions

1. Flash and run delayed-enable build to verify whether resets begin at Wi-Fi enable moment.
2. If yes, focus on hosted startup sequencing/watchdog interactions only.
3. Keep RTC confirmation task separate once runtime stability is restored.

## Key Anchors

- Primary config: `esp32-p4-coolroom.yaml`
- Session recap trail: `reference/session_recaps.md`
- Main handover: `HANDOVER_NOTES_2026-07-18.md`
- Hardware references: `reference/hardware_pins.md`
- Build helper and known ESPHome native-IDF workaround: `tools/esphome_compile.sh`
