#!/usr/bin/env bash
# Regenerate on-device speech clips (female English voice via macOS `say`).
# Output: assets/audio/*.wav — 16 kHz mono PCM16 for ESPHome media_player.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
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

gen alarm_high        "Attention. High temperature alarm."
gen alarm_low         "Attention. Low temperature alarm."
gen alarm_door        "Attention. Door open alarm."
gen alarm_no_cool     "Attention. No cooling alarm."
gen alarm_ice         "Attention. Ice detected on the evaporator."
gen alarm_probe       "Attention. Temperature probe fault."
gen info_cooling_on   "Cooling started."
gen info_cooling_off  "Cooling stopped."
gen info_defrost_on   "Defrost started."
gen info_defrost_off  "Defrost complete."
gen info_door_open    "Door opened."
gen info_door_closed  "Door closed."
gen info_audio_test   "Audio alerts are working."

du -sh "$OUT"
echo "Done. Rebuild firmware after changing clips."
