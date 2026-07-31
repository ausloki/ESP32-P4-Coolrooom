# Project Instructions — ESP32-P4 Coolroom Controller

These extend (not replace) the global Project Closeout Routine already in effect for every
project. This file adds project-specific requirements for this repo.

## Hardware: local schematic/manual first, then Waveshare live docs

Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.

**Before changing GPIO, I2S/audio, I2C, display/touch, SD, ESP32-C6, or Modbus wiring**,
consult the local reference PDFs and pin map — do not trust memory or chat alone:

- `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf` — board hardware manual / schematic
- `reference/esp32-p4_technical_reference_manual_en.pdf` / `esp32-p4_datasheet_en.pdf` — SoC
- `reference/hardware_pins.md` — project’s digested GPIO/I2C/audio map. For a **new field-I/O
  pin**, use the **"⭐ Field-I/O GPIO allocation"** table there (free vs. in-use header pins for
  THIS board) and update that row in the same commit when a pin is claimed or freed.
- Relay / RTD PDFs under `reference/` when touching Modbus

Then re-check live Waveshare ESP-IDF demos if pins may have changed upstream:

- https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF
- https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B

Prefer schematic / demo sample pin macros over table wording when names disagree
(Waveshare often names pins from the *peripheral*; ESPHome/ESP-IDF name them from the
*ESP*). See `.cursor/rules/waveshare-hardware-check.mdc`.

## Bench hardware status (current)

**Not connected yet** — do not treat as bugs or spend time diagnosing:

- RS485 relay board / RTD Modbus boards (Modbus timeouts and `* Online` OFF are expected)
- External I2C temp/humidity sensors on the expansion I2C header

Live verification of compressor/defrost/sensor control waits until those are attached.
On-board UI, Wi-Fi, web, and speaker path remain fair game. See
`.cursor/rules/bench-hardware-status.mdc` — update that rule when hardware is fitted.

## Flashing: `esphome upload` over USB erases all saved settings

`esphome upload` writes `firmware.factory.bin` from offset `0x0`. That merged image is
padded with `0xFF` across everything it spans — **including the NVS partition at
`0x9000`–`0x15000`** (see `.esphome/build/esp32-p4-coolroom/partitions.csv`). Every serial
upload therefore wipes stored settings, and the next boot falls back to each global's
`initial_value`. The tell-tale symptom is a setting "coming back on its own" after a
flash — e.g. ntfy re-enabling (`initial_value: "true"`) or the Home Assistant API toggle
reverting to off.

A plain reboot is safe: `persist_config_to_nvs` stages every restoring global and calls
`global_preferences->sync()`, so values are committed before power-down.

**Use one of these instead when settings must survive:**

```bash
./tools/esphome_flash.sh --device /dev/cu.usbmodemXXXX   # serial, skips the NVS window
.venv/bin/esphome upload esp32-p4-coolroom.yaml --device <device-ip>   # OTA, app only
```

`tools/esphome_flash.sh` reads offsets from the build's own `flasher_args.json` and refuses
to write anything overlapping NVS. Pass `--erase-settings` to deliberately fall back to the
factory image (needed after a partition-table change).

When testing anything persistence-related, reboot the device rather than reflashing —
otherwise a serial flash will look exactly like a persistence bug.

## Closeout Addition: Compile + Flash (mandatory)

Firmware/dashboard closeout is not complete until a **fresh compile and an NVS-safe
flash** have both succeeded. Do not stop at "compiled OK" or "committed".

```bash
# 1) If assets/dashboard.html changed:
python3 scripts/embed_dashboard.py

# 2) Coverage / persistence contract (entities + NVS staging list):
.venv/bin/python tools/check_dashboard_coverage.py

# 3) Compile:
./tools/esphome_compile.sh esp32-p4-coolroom.yaml

# 4) Flash — NEVER plain `esphome upload` over USB (erases NVS):
./tools/esphome_flash.sh --device /dev/cu.usbmodemXXXX
# or OTA: .venv/bin/esphome upload esp32-p4-coolroom.yaml --device <ip>

# 5) Optional live reboot-survival smoke (LAN, no secrets):
.venv/bin/python tools/test_settings_persistence.py --host <device-ip>

# 6) Graph + docs + compact commit (existing routine)
./tools/code_review_graph_cli.sh update --repo .
```

Use `tools/test_settings_persistence.py` after any change that touches restoring globals,
`persist_config_to_nvs`, or flash tooling. Prefer a **reboot** over a reflash when judging
whether settings stick.

## Closeout Addition: Dashboard Coverage & Persistence Check

The web settings UI is a **hand-maintained** JavaScript array
(`ADVANCED_SETTINGS_GROUPS` in `assets/dashboard.html`) — nothing generates it from the
ESPHome config. A setting added to `esp32-p4-coolroom.yaml` will therefore be invisible in
the web GUI unless the dashboard is edited too. Likewise, switches using
`restore_mode: DISABLED` and all `number` entities only survive a reboot if they write a
global with `restore_value: yes` **and** call `persist_config_to_nvs`.

A third hand-maintained list matters here too: `persist_config_to_nvs` names every global it
stages. A restoring global missing from it can be lost if the controller reboots within the
1 s poll window after the change.

**At closeout, whenever an entity or restoring global is added, removed, or renamed, run:**

```bash
.venv/bin/python tools/check_dashboard_coverage.py
```

It checks all three lists — dashboard coverage, the per-entity persistence contract, and the
`persist_config_to_nvs` staging list — and exits non-zero on any unexplained gap. If an
entity is deliberately not on the custom dashboard, add it to `EXPECTED_ABSENT` in that
script **with a reason** rather than suppressing the check. Remember the dashboard must be re-embedded (`scripts/embed_dashboard.py`)
and reflashed for HTML edits to reach the device.

## Closeout Addition: Documentation Consistency Check

This project maintains two end-user documents that describe device behavior in plain
language:

- `reference/USER_MANUAL.md` — full function/setting reference, organized by group, with
  NS-style structograms for non-trivial control decisions.
- `reference/QUICK_START_GUIDE.md` — condensed setup guide + worked recommended-settings
  examples.

**At closeout, for any change that touches `esp32-p4-coolroom.yaml`, `p4_control.h`,
`p4_logging.h`, `p4_helpers.h`, or `p4_log_manager.h`, check whether the change affects
anything either manual describes, and update the manual(s) if so — before considering the
work closed out.** This runs alongside the existing build/test verification, not instead of
it.

Concretely, check for and reflect:

- **A setting added, removed, or renamed** — add/remove/rename its row in the relevant
  group's table in `USER_MANUAL.md` (§4.x), and in `QUICK_START_GUIDE.md`'s groups-at-a-glance
  table or worked example if it's the kind of setting an operator would actually tune.
- **A setting's range, default, or unit changed** — update the corresponding table row in
  both documents (the Quick Start worked example only if the changed setting appears there).
- **A setting's group changed** (moved to a different touchscreen page / web dashboard
  section) — move its row to the correct §4.x section and update the touchscreen-page /
  web-section reference in that section's italic subheading and in the Document Map (§6).
- **Control logic changed** (a function in `p4_control.h`, or the control-tick sequencing in
  `esp32-p4-coolroom.yaml`) — update or add the matching NS structogram. If the behavior is
  new enough that no structogram exists yet, add one rather than describing it in prose only.
- **A new user-facing feature added** (new alarm type, new notification, new touchscreen
  page, new access-control behavior, new SD/backup behavior, etc.) — add a new subsection (or
  extend an existing one) following the same "what it does / range / default / when to
  change it" table format already used throughout, plus a structogram if the feature involves
  more than a single always/never behavior.
- **A quirk or known limitation is fixed** — remove the corresponding callout (e.g. the "Door
  Sensor Mode also toggles the light relay" note in §4.6, if that's ever fixed) rather than
  leaving stale documentation of a bug that no longer exists.

If a change genuinely has no user-facing effect (internal refactor, comment-only change, a
bug fix that restores documented behavior rather than changing it), it's fine to make no
manual edit — but note that explicitly in the session recap ("no manual update needed —
internal only") rather than silently skipping the check, matching the project's existing
practice of recording what was and wasn't done.

Screenshots in both documents are still placeholder-only pending hardware connection — that
doesn't block this check; text/table/diagram accuracy is independent of the screenshots.
