# Audio alerts — speaker path + mic backlog

## Done (this pass)

- ES8311 DAC + I2S speaker + NS4150B PA enable (`p4_audio.yaml`)
- Pre-recorded female English clips (`assets/audio/*.wav`, Kokoro **af_sarah** via
  Sherpa-onnx — `scripts/generate_audio_clips_kokoro.py` / `ENGINE=kokoro
  scripts/generate_audio_clips.sh`; Apple Samantha still available with
  `ENGINE=apple`)
- Master **Audio Alerts Enabled** + per-phrase toggles (alarms + info), including
  hardware-offline phrases that match the home centre status
  (`RELAY BOARD OFFLINE`, `TEMP BOARD OFFLINE`, `HUMIDITY SENSOR OFFLINE`,
  `AMBIENT SENSOR OFFLINE`) and updated wording for probe / not-cooling / ice
- Web **Speaker Volume (%)** (50–90, default 85) — NVS-backed, synced with HA media player
- Edge-triggered play on alarm/info events; home bell soft-mute silences speech
- Web dashboard **Audio** tab + Test Speaker button
- Per-phrase **🔊** speaker-icon preview on the web Audio tab and on LVGL Settings 8/8
  (plays that clip even if the Speak toggle or master is off)
- `hw_audio_ok` from boot I2C probe at `0x18`
- Soft-start click suppression (see below) — glass-confirmed 2026-08-02

## Canonical anti-click approach (do not regress)

**Board:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B — ES8311 codec + NS4150B Class-D
(`PA_Ctrl` = GPIO53). Every announcement tears down / recreates the I2S TX channel;
that bus bring-up produces an audible pop **if the amp is already powered**.

### What works (current production)

Soft-start each phrase in `p4_audio.yaml` (`audio_prepare_play` /
`audio_preview_prepare` + 25 ms interval + globals
`ctl_audio_softstart_phase` / `ctl_audio_softstart_at_ms`):

1. **PA off** + ES8311 mute + speaker mute  
2. Start playback into **~400 ms leading silence** on the WAV  
   (`--lead-silence-ms 400` in `scripts/generate_audio_clips_kokoro.py`)  
3. After ~120 ms: **PA on** (still muted — amp biases into silence)  
4. After ~150 ms more: unmute + restore operator volume  

Glass-confirmed: **no click, full phrase completes**.

| Piece | Where |
|---|---|
| Prepare (PA off, mute, arm phase 1) | `audio_prepare_play` / `audio_preview_prepare` |
| Phase advance (PA on → unmute) | `interval: 25ms` in `p4_audio.yaml` |
| Lead-in silence on clips | `assets/audio/*.wav` via kokoro generator |
| Timing globals | `ctl_audio_softstart_*` in `esp32-p4-coolroom.yaml` |

When regenerating clips, keep **≥ 400 ms** lead-in silence or the unmute will land on
speech (or the PA-on step will be audible).

### What does *not* work (tried — do not re-enable casually)

| Approach | Result |
|---|---|
| ESPHome `speaker.keep_alive: true` (PR [#15565](https://github.com/esphome/esphome/pull/15565)) | Removes click, but on **ESPHome 2026.7.0** hits `ERR_LOCKSTEP_DESYNC` / “Event/record queues desynced, restarting speaker task” and **truncates** clips mid-phrase. Revisit only after that PR is fixed and re-tested on this board. |
| DAC mute only, PA left on | Phrase OK; **click remains** — amp still hears I2S bring-up. |
| Leave PA always on after boot | Same — does not hide I2S re-init pop. |
| Boot silence “warmup” clip | Fought the same I2S path; not needed with soft-start. |

### Rules for future audio changes

1. **Prefer soft-start** over `keep_alive` until upstream is proven clean on P4 + ES8311 + NS4150B with our announcement pipeline.  
2. **Never** start I2S recreate with PA already on and DAC unmuted.  
3. Keep lead-in silence when regenerating WAVs; document the ms in `assets/audio/VOICE.txt`.  
4. After any change to prepare / interval / clip lead-in: Test Speaker on glass — full phrase, no click.  
5. Pin map and PA GPIO stay in `reference/hardware_pins.md` / substitutions — do not reuse GPIO 9–13 / 53.

## Two other gotchas that cost a debugging session

**Pin direction.** Waveshare's I2S table names the data pins from the *codec's* point of
view — `ASDOUT` (GPIO11) is the ES8311 driving the bus, `DSDIN` (GPIO9) is the ES8311
listening. ESPHome's `i2s_dout_pin` / `i2s_din_pin` are named from the *ESP's* point of
view, so they invert: playback is `i2s_dout_pin: GPIO9`, mic is `i2s_din_pin: GPIO11`.
Wiring playback to GPIO11 produces a completely healthy-looking system — clocks run, the
media player reports `PLAYING` for the correct duration, no decode errors — but the samples
are transmitted onto the codec's own output pin and never arrive. The only symptom is the
amplifier popping as it enables and disables.

**Volume is not a linear percentage of loudness.** ESPHome passes the media player volume
straight to the ES8311 volume register, where 0.75 is 0 dB and 1.0 is +32 dB. The stock
ESPHome default of 0.5 lands near −32 dB (inaudible for speech), and 1.0 clips hard. This
project uses `volume_initial: 0.85`, `volume_min: 0.50`, `volume_max: 0.90`, exposed on the
web as **Speaker Volume (%)** (50–90, default 85). The same value is what HA sees on
*Coolroom Speaker*.

## On hold — microphone / voice input

**Not in scope for now.** No microphone is fitted on the current build, and voice
input may not be used on this project. Speaker announcements remain the audio path.

Board still has I2S DIN (`audio_i2s_din_pin` / GPIO11) and ES8311 ADC if that ever
changes. Web **Audio** tab (and virtual preview) already expose speak toggles including
hardware-offline phrases; LVGL audio toggles remain a separate deferred item
(web-only today).

If picking mic work up later (only if product need returns):

1. ESPHome `microphone` + `i2s_audio` input on the same bus (or second I2S port if
   duplex conflicts with speaker on this BSP).
2. Decide product use: local keyword / push-to-talk vs cloud STT (needs WiFi +
   privacy policy).
3. Keep announcements priority over mic capture (PA on = speaker; mute mic while
   playing). Soft-start must still run before speech.
4. Add LVGL toggles for audio (currently web-only) once the settings page budget
   allows.

Do not reuse GPIO 9–13 / 53 for other peripherals.
