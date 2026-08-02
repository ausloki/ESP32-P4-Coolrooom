#!/usr/bin/env python3
"""Regenerate on-device speech clips with Sherpa-onnx Kokoro (default: af_sarah).

Output: assets/audio/*.wav — 16 kHz mono PCM16 for ESPHome media_player.
Phrases match scripts/generate_audio_clips.sh / home-gauge centre status labels.

Requires:
  .venv/bin/pip install sherpa-onnx
  Model under tools/tts_compare/sherpa/kokoro-* (auto-downloaded if missing).

Usage:
  .venv/bin/python scripts/generate_audio_clips_kokoro.py
  .venv/bin/python scripts/generate_audio_clips_kokoro.py --speaker af_bella
"""

from __future__ import annotations

import argparse
import array
import shutil
import subprocess
import sys
import tarfile
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "audio"
MODEL_ROOT = ROOT / "tools" / "tts_compare" / "sherpa"
RELEASE = "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models"

# Kokoro multi-lang v1.0 speaker map (US female af_*).
SPEAKERS = {
    "af_alloy": 0,
    "af_aoede": 1,
    "af_bella": 2,
    "af_heart": 3,
    "af_jessica": 4,
    "af_kore": 5,
    "af_nicole": 6,
    "af_nova": 7,
    "af_river": 8,
    "af_sarah": 9,
    "af_sky": 10,
}

PHRASES: list[tuple[str, str]] = [
    ("alarm_high", "Attention. High temperature."),
    ("alarm_low", "Attention. Low temperature."),
    ("alarm_door", "Attention. Door open."),
    ("alarm_no_cool", "Attention. Not cooling."),
    ("alarm_ice", "Attention. Ice on coil."),
    ("alarm_probe", "Attention. Coolroom probe bad."),
    ("alarm_relay_board", "Attention. Relay board offline."),
    ("alarm_temp_board", "Attention. Temperature board offline."),
    ("alarm_humidity_sensor", "Attention. Humidity sensor offline."),
    ("alarm_ambient_sensor", "Attention. Ambient sensor offline."),
    ("info_cooling_on", "Cooling started."),
    ("info_cooling_off", "Cooling stopped."),
    ("info_defrost_on", "Defrost started."),
    ("info_defrost_off", "Defrost complete."),
    ("info_door_open", "Door opened."),
    ("info_door_closed", "Door closed."),
    ("info_audio_test", "Audio alerts are working."),
]


def _run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 0:
        return
    print(f"Downloading {url}")
    _run(["curl", "-fL", "--progress-bar", "-o", str(dest), url])


def ensure_kokoro() -> Path:
    MODEL_ROOT.mkdir(parents=True, exist_ok=True)
    for name in ("kokoro-int8-multi-lang-v1_0", "kokoro-multi-lang-v1_0"):
        d = MODEL_ROOT / name
        if (d / "tokens.txt").exists() and (d / "voices.bin").exists():
            return d
    name = "kokoro-int8-multi-lang-v1_0"
    tgz = MODEL_ROOT / f"{name}.tar.bz2"
    try:
        _download(f"{RELEASE}/{name}.tar.bz2", tgz)
    except subprocess.CalledProcessError:
        name = "kokoro-multi-lang-v1_0"
        tgz = MODEL_ROOT / f"{name}.tar.bz2"
        _download(f"{RELEASE}/{name}.tar.bz2", tgz)
    print(f"Extracting {tgz.name}")
    with tarfile.open(tgz, "r:bz2") as tf:
        tf.extractall(MODEL_ROOT)
    d = MODEL_ROOT / name
    if not (d / "tokens.txt").exists():
        raise SystemExit(f"Kokoro extract failed: {d}")
    return d


def _model_file(kokoro_dir: Path) -> Path:
    for n in ("model.int8.onnx", "model.onnx"):
        p = kokoro_dir / n
        if p.exists():
            return p
    raise SystemExit(f"No model.onnx in {kokoro_dir}")


def _to_16k_mono(src: Path, dest: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        aiff = Path(td) / "in.aiff"
        try:
            _run(["afconvert", "-f", "AIFF", "-d", "BEI16", str(src), str(aiff)])
            _run(
                [
                    "afconvert",
                    "-f",
                    "WAVE",
                    "-d",
                    "LEI16@16000",
                    "-c",
                    "1",
                    str(aiff),
                    str(dest),
                ]
            )
        except subprocess.CalledProcessError:
            shutil.copy2(src, dest)


def _write_wav(path: Path, samples, sample_rate: int, lead_silence_ms: float = 0.0) -> None:
    pcm = array.array("h")
    # Leading silence parks the I2S re-init click before speech starts
    # (keep_alive would remove the click but truncates on ESPHome 2026.7.0).
    lead = int(sample_rate * (lead_silence_ms / 1000.0))
    pcm.extend([0] * max(0, lead))
    for s in samples:
        pcm.append(int(max(-1.0, min(1.0, float(s))) * 32767.0))
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm.tobytes())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--speaker",
        default="af_sarah",
        choices=sorted(SPEAKERS),
        help="Kokoro US female speaker (default: af_sarah)",
    )
    ap.add_argument("--speed", type=float, default=1.0)
    ap.add_argument(
        "--lead-silence-ms",
        type=float,
        default=120.0,
        help="Silence prepended to each clip (ms) so I2S start-click lands before words",
    )
    args = ap.parse_args()

    try:
        import sherpa_onnx
    except ImportError:
        print("Install: .venv/bin/pip install sherpa-onnx", file=sys.stderr)
        return 1

    sid = SPEAKERS[args.speaker]
    kokoro_dir = ensure_kokoro()
    print(f"Model: {kokoro_dir.name}  speaker={args.speaker} sid={sid}")

    lexicon = ""
    us = kokoro_dir / "lexicon-us-en.txt"
    zh = kokoro_dir / "lexicon-zh.txt"
    if us.exists() and zh.exists():
        lexicon = f"{us},{zh}"
    elif us.exists():
        lexicon = str(us)

    cfg = sherpa_onnx.OfflineTtsConfig(
        model=sherpa_onnx.OfflineTtsModelConfig(
            kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(
                model=str(_model_file(kokoro_dir)),
                voices=str(kokoro_dir / "voices.bin"),
                tokens=str(kokoro_dir / "tokens.txt"),
                data_dir=str(kokoro_dir / "espeak-ng-data"),
                lexicon=lexicon,
            ),
            provider="cpu",
            num_threads=2,
        ),
        max_num_sentences=1,
    )
    if not cfg.validate():
        raise SystemExit("Invalid sherpa TTS config")
    tts = sherpa_onnx.OfflineTts(cfg)

    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for pid, text in PHRASES:
            print(f"Generating {pid} ({args.speaker})…")
            gen = sherpa_onnx.GenerationConfig()
            gen.sid = sid
            gen.speed = args.speed
            gen.silence_scale = 0.2
            audio = tts.generate(text, gen)
            if len(audio.samples) == 0:
                raise SystemExit(f"Empty audio for {pid}")
            raw = tmp / f"{pid}.wav"
            dest = OUT / f"{pid}.wav"
            _write_wav(
                raw,
                audio.samples,
                audio.sample_rate,
                lead_silence_ms=args.lead_silence_ms,
            )
            _to_16k_mono(raw, dest)

    # Record which voice is currently installed (for humans / future regen).
    (OUT / "VOICE.txt").write_text(
        f"engine=sherpa-onnx-kokoro\nspeaker={args.speaker}\nsid={sid}\n"
        f"model={kokoro_dir.name}\n"
        f"lead_silence_ms={args.lead_silence_ms}\n"
        f"note=Regenerate with scripts/generate_audio_clips_kokoro.py\n"
    )
    print(f"Done — {len(PHRASES)} clips in {OUT}")
    print("Rebuild + flash firmware to hear them on the controller.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
