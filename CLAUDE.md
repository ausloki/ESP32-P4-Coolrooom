# Project Instructions — ESP32-P4 Coolroom Controller

These extend (not replace) the global Project Closeout Routine already in effect for every
project. This file adds project-specific requirements for this repo.

## Hardware: local schematic/manual first, then Waveshare live docs

Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.

**Before changing GPIO, I2S/audio, I2C, display/touch, SD, ESP32-C6, or Modbus wiring**,
consult the local reference PDFs and pin map — do not trust memory or chat alone:

- `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf` — board hardware manual / schematic
- `reference/esp32-p4_technical_reference_manual_en.pdf` / `esp32-p4_datasheet_en.pdf` — SoC
- `reference/hardware_pins.md` — project’s digested GPIO/I2C/audio map
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
