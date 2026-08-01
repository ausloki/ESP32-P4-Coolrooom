# Audio alerts — speaker path + mic backlog

## Done (this pass)

- ES8311 DAC + I2S speaker + NS4150B PA enable (`p4_audio.yaml`)
- Pre-recorded female English clips (`assets/audio/*.wav`, Samantha via
  `scripts/generate_audio_clips.sh`)
- Master **Audio Alerts Enabled** + per-phrase toggles (alarms + info), including
  hardware-offline phrases that match the home centre status
  (`RELAY BOARD OFFLINE`, `TEMP BOARD OFFLINE`, `HUMIDITY SENSOR OFFLINE`,
  `AMBIENT SENSOR OFFLINE`) and updated wording for probe / not-cooling / ice
- Web **Speaker Volume (%)** (50–90, default 85) — NVS-backed, synced with HA media player
- Edge-triggered play on alarm/info events; home bell soft-mute silences speech
- Web dashboard **Audio** tab + Test Speaker button
- `hw_audio_ok` from boot I2C probe at `0x18`

## Two gotchas that cost a debugging session

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

## Follow-up — microphone / voice input

Not started. Board has I2S DIN (`audio_i2s_din_pin` / GPIO11) and ES8311 ADC.

Suggested next steps when picking this up:

1. ESPHome `microphone` + `i2s_audio` input on the same bus (or second I2S port if
   duplex conflicts with speaker on this BSP).
2. Decide product use: local keyword / push-to-talk vs cloud STT (needs WiFi +
   privacy policy).
3. Keep announcements priority over mic capture (PA on = speaker; mute mic while
   playing).
4. Add LVGL toggles for audio (currently web-only) once the settings page budget
   allows.

Do not reuse GPIO 9–13 / 53 for other peripherals.
