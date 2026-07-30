# Active Context — Current Session State

**Date:** 2026-07-30
**Session:** Resumed an interrupted afternoon session (hosted-WiFi crash-loop fix + several UI
passes, never closed out). Root-caused and fixed the horseshoe gauge's orientation bug, then ran
five live flash-and-observe rounds tuning gauge diameter/spacing plus two rounds resizing the
left icon rail, all confirmed directly against the physical screen. Closed out both this evening's
work and the previously-undocumented afternoon session together.
**Status:** BUILDABLE + FLASHED, ON REAL HARDWARE. WiFi deliberately disabled (hosted-link
instability, unresolved). Home screen gauge + icon rail in a confirmed-good state per operator.

## Current Focus

### What Was Confirmed This Session

- The project's first physical hardware arrived 2026-07-29. Before any flashing: took a full
  32MB factory backup (`backups/factory_backup_2026-07-29_e8f60ae08f52_32MB.bin`, gitignored,
  local-only) and verified it non-trivially (bootloader magic, partition table, genuine
  ESP-IDF/Waveshare factory strings).
- First flash hung silently (ROM banner only, then nothing). Root-caused to three real,
  previously-unverifiable config gaps: `engineering_sample: true` never applied for this
  revision-1.3 (< 3.0) chip despite being documented as required; `flash_mode: qio` didn't
  actually apply due to a real ESPHome gap (boolean choice set, companion literal string
  Kconfig left stale — worked around with an explicit `sdkconfig_options` string override);
  PSRAM speed silently defaulting to 20MHz instead of Waveshare's validated 200MHz. All three
  cross-confirmed against Waveshare's own official example repo's `sdkconfig.defaults` files.
- An afternoon session (2026-07-30) then found and fixed a real hosted-WiFi reset loop
  (`H_API link not yet up` → assert/reset), isolated via a controlled SDIO parameter sweep to the
  WiFi bring-up path specifically (not LVGL/control logic), and shipped a stability-first build
  with `wifi.enable_on_boot: false`. That session also ran 5 UI correction passes but never
  completed closeout — header status left stale, nothing committed, no session recap written.
  Reconstructed that history into `reference/session_recaps.md` and the handover doc this session
  rather than leaving it lost.
- This evening: found the actual bug behind the afternoon session's repeated failed attempts to
  fix the horseshoe gauge orientation — a genuine math error (LVGL angles are clockwise from
  0°=3 o'clock; `330/210` centers the gap at 12 o'clock, not 6 o'clock as that session believed).

### Latest Completed Work

- **Arc orientation fixed**: all four gauge arcs (grey reference ring + blue/cyan/pink reading
  arcs) changed from `start_angle: 330 / end_angle: 210` (top-opening, confirmed wrong) to
  `150 / 30` (bottom-opening, correct) — also resolved a second latent bug where the tick marks
  had been correctly positioned for bottom-opening the whole time, contradicting the wrongly-
  oriented arcs. Tick marks (24 label widgets) later removed entirely per operator feedback.
- **Gauge diameter/spacing tuned in 5 live rounds**, each flashed and confirmed on the physical
  screen before the next: zero-gap touching bands (too big) → real ~10px gaps (34px diameter
  pitch) → ~3.5mm smaller → ~2mm smaller ×2 (second round required holding the innermost/pink
  ring back to a smaller step, since the full reduction would have tucked it behind the fixed
  270px center circle — corrected a wrong assumption mid-session that the circle's true outer
  edge was 306px; LVGL draws `border_width` inside a box, not outside it, so it's exactly 270px)
  → outer (blue) and middle (cyan) grown back +1mm each on request. **Final state**: grey 377px,
  blue 350px, cyan 316px, pink 284px, all `arc_width: 12`, all `150/30`.
- **Left icon rail resized twice**: doubled (`font_mdi_large` 50→100, buttons 72→100×100), then
  reduced 25% (100→75, buttons 75×75) — both passes evenly spaced the 4 icons across the full
  504px content height (below the 48px header, above the 552px tab bar) and updated button
  `radius` to keep true circles (36→50→38).
- **Confirmed a real, separate, unresolved bug**: three independent, properly reset-triggered
  raw-serial captures (5–6 min each) show only the 229-byte ROM bootloader banner and nothing
  else — zero app-level log output ever, on any of the ~10 flashes done across this session and
  the last, despite the device reliably reaching a working UI. Not yet root-caused.
- **New standing hardware note** (`reference/hardware_pins.md` item 7): this board exposes two
  simultaneous USB connections (same MAC on both `/dev/cu.usbmodem*` paths) — practical workflow
  is one port for flashing, the other held open for a persistent log/monitor session.
- **Documentation hygiene**: cleaned up stale/self-contradictory inline yaml comments from the
  rapid iteration; corrected the same wrong `330/210` claim (and a wrong alarm-icon glyph code)
  in `reference/DISPLAY_ARCHITECTURE_VISUAL.md`, flagging its detailed pixel diagrams as stale
  rather than fully rewriting them; fixed a structural bug in `HANDOVER_NOTES_2026-07-18.md`
  where the afternoon session's insertions had severed the 2026-07-26 addendum's heading from
  its own body text (content wasn't lost, just silently reattached to the wrong section).

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh, clean
Flashed and verified (hash-checked) via esphome upload, multiple times this session
RAM:   22.1% (127,128 / 576,464 bytes)
Flash: 21.6% (1,582,204 / 7,340,032 bytes)
config_hash: 0x831afa6a (final, this session)
```

### Immediate Next Actions

1. Root-cause the missing-app-log bug (zero output on USB console past the ROM banner) — a
   proper `esphome logs` session or a from-scratch check of the `logger:` component config
   against `hardware_uart: USB_SERIAL_JTAG` would be the next step.
2. Fix the actual hosted-WiFi reset loop (currently just disabled, not fixed) — device has no
   network connectivity until this is resolved.
3. If further gauge shrinking is ever wanted: the innermost ring is now up against the fixed
   center-circle constraint — next step would be a deliberate resize of the center hub itself
   (affects temp-readout text layout), not just another diameter tweak.
4. Resume the RS485 relay/PHY bring-up and remaining Phase 5 hardware validation items — now
   actually possible with real hardware connected, previously all blocked.

### Outstanding Items

1. Missing app-level log output on USB console — new this session, real, unresolved.
2. Hosted-WiFi reset loop — root cause not fixed, only worked around by disabling WiFi entirely.
3. Ethernet/RS485/sensor hardware bring-up — now unblocked by real hardware, not yet started.
4. `opendir`/`readdir`/`closedir` linker warnings on the SD log manager — still unconfirmed
   without a real SD card test (now possible).
5. Both user manuals and the LVGL mockup artifact still need real screenshots — hardware is now
   available for this.
6. `reference/DISPLAY_ARCHITECTURE_VISUAL.md`'s detailed pixel-coordinate diagrams are stale
   (flagged, not rewritten) — low priority, current values are readable directly from the yaml.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml` — see `# Center Arc Gauge Container` for the
  home-screen gauge, `# Left Sidebar - Control Buttons` for the icon rail
- Hardware notes: `reference/hardware_pins.md` (item 7: dual USB ports; item 8: engineering
  sample flag)
- Factory backup: `backups/factory_backup_2026-07-29_e8f60ae08f52_32MB.bin` (gitignored, keep a
  copy somewhere durable — it's the only copy of the original factory firmware)
- Session log: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Display reference (partially stale, see disclaimer at its top): `reference/DISPLAY_ARCHITECTURE_VISUAL.md`
- Old S3 reference project (read-only, for porting decisions):
  `/Volumes/Scratch/Documents/ESP32-Coolroom-Prescision/esp32-coolroom.yaml` +
  `esphome_includes.h`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- User-facing docs: `reference/USER_MANUAL.md`, `reference/QUICK_START_GUIDE.md`
- Web dashboard virtual preview (Artifact): `https://claude.ai/code/artifact/a5947d8c-7dfc-4b4f-adb0-fcf77b175ca3`
- LVGL touchscreen mockup (Artifact, now superseded by real hardware — kept for reference):
  `https://claude.ai/code/artifact/9d09d4c2-8fbd-4ddc-b788-07c5afb2c8ab`

---

**Ready for:** Real hardware bring-up is genuinely underway now — display, gauge visuals, and
icon rail are in a confirmed-good state. The two open bugs (missing logs, WiFi reset loop) are
the highest-value next targets; both are now debuggable in a way they weren't before real
hardware existed.
