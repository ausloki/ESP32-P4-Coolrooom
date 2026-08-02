#!/usr/bin/env bash
# Regenerate on-device speech clips.
# Default production voice is Kokoro af_sarah (Sherpa-onnx) — see
# scripts/generate_audio_clips_kokoro.py. This script keeps the macOS `say`
# (Apple Samantha) path as ENGINE=apple for A/B or offline Mac-only regen.
#
#   ENGINE=kokoro ./scripts/generate_audio_clips.sh   # default
#   ENGINE=apple  ./scripts/generate_audio_clips.sh
#
# Output: assets/audio/*.wav — 16 kHz mono PCM16 for ESPHome media_player.
# Phrases match the home-gauge centre status labels in p4_ui.h.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${ENGINE:-kokoro}"

if [[ "$ENGINE" == "kokoro" ]]; then
  exec "$ROOT/.venv/bin/python" "$ROOT/scripts/generate_audio_clips_kokoro.py" \
    --speaker "${SPEAKER:-af_sarah}"
fi

OUT="$ROOT/assets/audio"
TMP="${TMPDIR:-/tmp}/p4_audio_gen"
VOICE="${VOICE:-Samantha}"
RATE="${RATE:-170}"

mkdir -p "$OUT" "$TMP"

gen() {
  local id="$1" text="$2"
  echo "Generating $id ($VOICE)…"
  say -v "$VOICE" -r "$RATE" -o "$TMP/${id}.aiff" "$text"
  afconvert -f WAVE -d LEI16@16000 -c 1 "$TMP/${id}.aiff" "$OUT/${id}.wav"
}

gen alarm_high             "Attention. High temperature."
gen alarm_low              "Attention. Low temperature."
gen alarm_door             "Attention. Door open."
gen alarm_no_cool          "Attention. Not cooling."
gen alarm_ice              "Attention. Ice on coil."
gen alarm_probe            "Attention. Coolroom probe bad."
gen alarm_relay_board      "Attention. Relay board offline."
gen alarm_temp_board       "Attention. Temperature board offline."
gen alarm_humidity_sensor  "Attention. Humidity sensor offline."
gen alarm_ambient_sensor   "Attention. Ambient sensor offline."
gen info_cooling_on        "Cooling started."
gen info_cooling_off       "Cooling stopped."
gen info_defrost_on        "Defrost started."
gen info_defrost_off       "Defrost complete."
gen info_door_open         "Door opened."
gen info_door_closed       "Door closed."
gen info_audio_test        "Audio alerts are working."

du -sh "$OUT"
echo "Done. Rebuild firmware after changing clips."
