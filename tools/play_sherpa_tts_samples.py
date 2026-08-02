#!/usr/bin/env python3
"""Download Sherpa-onnx TTS models and play coolroom-alert samples.

Voices played (same phrases as tools/compare_tts_voices.py):
  Kokoro  — af_sky, af_bella, af_sarah  (US female; multi-lang v1.0 int8)
  Matcha  — en_US LJSpeech female
  Kitten  — nano en sid=0 (compact baseline)

Models land in tools/tts_compare/sherpa/ (gitignored).
WAVs land in assets/audio/tts_compare/.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tarfile
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODEL_ROOT = ROOT / "tools" / "tts_compare" / "sherpa"
OUT_ROOT = ROOT / "assets" / "audio" / "tts_compare"
RELEASE = "https://github.com/k2-fsa/sherpa-onnx/releases/download"

PHRASES = [
    ("alarm_high", "Attention. High temperature."),
    ("alarm_door", "Attention. Door open."),
    ("info_audio_test", "Audio alerts are working."),
]

KOKORO_SPEAKERS = [
    ("af_sky", 10),
    ("af_bella", 2),
    ("af_sarah", 9),
]


def _run(cmd: list[str], **kw) -> None:
    subprocess.run(cmd, check=True, **kw)


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 0:
        print(f"  already have {dest.name}")
        return
    print(f"  downloading {url}")
    _run(["curl", "-fL", "--progress-bar", "-o", str(dest), url])


def _extract_bz2(archive: Path, dest_dir: Path, marker: Path) -> None:
    if marker.exists():
        print(f"  already extracted {marker.parent.name}")
        return
    dest_dir.mkdir(parents=True, exist_ok=True)
    print(f"  extracting {archive.name}")
    with tarfile.open(archive, "r:bz2") as tf:
        tf.extractall(dest_dir)


def _ensure_wav_16k_mono(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
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


def _write_raw_wav(path: Path, samples, sample_rate: int) -> None:
    """Write float32 samples [-1,1] as PCM16 WAV without soundfile."""
    import array
    import struct

    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = array.array("h")
    for s in samples:
        v = int(max(-1.0, min(1.0, float(s))) * 32767.0)
        pcm.append(v)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm.tobytes())


def ensure_models() -> dict[str, Path]:
    MODEL_ROOT.mkdir(parents=True, exist_ok=True)
    paths: dict[str, Path] = {}

    # Prefer int8 multi-lang (~126 MB) — has af_* speaker names.
    kokoro_name = "kokoro-int8-multi-lang-v1_0"
    kokoro_dir = MODEL_ROOT / kokoro_name
    kokoro_tgz = MODEL_ROOT / f"{kokoro_name}.tar.bz2"
    try:
        _download(
            f"{RELEASE}/tts-models/{kokoro_name}.tar.bz2",
            kokoro_tgz,
        )
        _extract_bz2(kokoro_tgz, MODEL_ROOT, kokoro_dir / "tokens.txt")
    except subprocess.CalledProcessError:
        # Fall back to full fp multi-lang if int8 tag missing.
        kokoro_name = "kokoro-multi-lang-v1_0"
        kokoro_dir = MODEL_ROOT / kokoro_name
        kokoro_tgz = MODEL_ROOT / f"{kokoro_name}.tar.bz2"
        _download(f"{RELEASE}/tts-models/{kokoro_name}.tar.bz2", kokoro_tgz)
        _extract_bz2(kokoro_tgz, MODEL_ROOT, kokoro_dir / "tokens.txt")
    paths["kokoro"] = kokoro_dir

    matcha_dir = MODEL_ROOT / "matcha-icefall-en_US-ljspeech"
    matcha_tgz = MODEL_ROOT / "matcha-icefall-en_US-ljspeech.tar.bz2"
    _download(f"{RELEASE}/tts-models/matcha-icefall-en_US-ljspeech.tar.bz2", matcha_tgz)
    _extract_bz2(matcha_tgz, MODEL_ROOT, matcha_dir / "tokens.txt")
    paths["matcha"] = matcha_dir

    vocos = MODEL_ROOT / "vocos-22khz-univ.onnx"
    _download(f"{RELEASE}/vocoder-models/vocos-22khz-univ.onnx", vocos)
    paths["vocos"] = vocos

    kitten_dir = MODEL_ROOT / "kitten-nano-en-v0_1-fp16"
    kitten_tgz = MODEL_ROOT / "kitten-nano-en-v0_1-fp16.tar.bz2"
    _download(f"{RELEASE}/tts-models/kitten-nano-en-v0_1-fp16.tar.bz2", kitten_tgz)
    _extract_bz2(kitten_tgz, MODEL_ROOT, kitten_dir / "tokens.txt")
    paths["kitten"] = kitten_dir

    return paths


def _kokoro_model_file(kokoro_dir: Path) -> Path:
    for name in ("model.int8.onnx", "model.onnx"):
        p = kokoro_dir / name
        if p.exists():
            return p
    raise SystemExit(f"No Kokoro model.onnx in {kokoro_dir}")


def _kitten_model_file(kitten_dir: Path) -> Path:
    for name in ("model.fp16.onnx", "model.onnx", "model.int8.onnx"):
        p = kitten_dir / name
        if p.exists():
            return p
    raise SystemExit(f"No Kitten model in {kitten_dir}")


def _matcha_acoustic(matcha_dir: Path) -> Path:
    for name in ("model-steps-3.onnx", "model.onnx"):
        p = matcha_dir / name
        if p.exists():
            return p
    raise SystemExit(f"No Matcha acoustic model in {matcha_dir}")


def make_tts(kind: str, paths: dict[str, Path]):
    import sherpa_onnx

    if kind == "kokoro":
        d = paths["kokoro"]
        lexicon = ""
        us = d / "lexicon-us-en.txt"
        zh = d / "lexicon-zh.txt"
        if us.exists() and zh.exists():
            lexicon = f"{us},{zh}"
        elif us.exists():
            lexicon = str(us)
        cfg = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(
                kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(
                    model=str(_kokoro_model_file(d)),
                    voices=str(d / "voices.bin"),
                    tokens=str(d / "tokens.txt"),
                    data_dir=str(d / "espeak-ng-data"),
                    lexicon=lexicon,
                ),
                provider="cpu",
                num_threads=2,
            ),
            max_num_sentences=1,
        )
    elif kind == "matcha":
        d = paths["matcha"]
        cfg = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(
                matcha=sherpa_onnx.OfflineTtsMatchaModelConfig(
                    acoustic_model=str(_matcha_acoustic(d)),
                    vocoder=str(paths["vocos"]),
                    tokens=str(d / "tokens.txt"),
                    data_dir=str(d / "espeak-ng-data"),
                ),
                provider="cpu",
                num_threads=2,
            ),
            max_num_sentences=1,
        )
    elif kind == "kitten":
        d = paths["kitten"]
        cfg = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(
                kitten=sherpa_onnx.OfflineTtsKittenModelConfig(
                    model=str(_kitten_model_file(d)),
                    voices=str(d / "voices.bin"),
                    tokens=str(d / "tokens.txt"),
                    data_dir=str(d / "espeak-ng-data"),
                ),
                provider="cpu",
                num_threads=2,
            ),
            max_num_sentences=1,
        )
    else:
        raise ValueError(kind)

    if not cfg.validate():
        raise SystemExit(f"Invalid sherpa config for {kind}")
    return sherpa_onnx.OfflineTts(cfg)


def synth(tts, text: str, sid: int, out: Path) -> Path:
    import sherpa_onnx

    gen = sherpa_onnx.GenerationConfig()
    gen.sid = sid
    gen.speed = 1.0
    gen.silence_scale = 0.2
    audio = tts.generate(text, gen)
    if len(audio.samples) == 0:
        raise SystemExit(f"Empty audio for {out}")
    raw = out.with_suffix(".src.wav")
    _write_raw_wav(raw, audio.samples, audio.sample_rate)
    _ensure_wav_16k_mono(raw, out)
    raw.unlink(missing_ok=True)
    return out


def play(path: Path, label: str) -> None:
    print(f"\n▶ {label}\n  {path}")
    _run(["afplay", str(path)])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-play", action="store_true")
    ap.add_argument(
        "--phrase",
        default="all",
        help="alarm_high | alarm_door | info_audio_test | all",
    )
    args = ap.parse_args()

    phrases = PHRASES
    if args.phrase != "all":
        phrases = [p for p in PHRASES if p[0] == args.phrase]
        if not phrases:
            raise SystemExit(f"Unknown phrase id: {args.phrase}")

    print("Ensuring Sherpa models…")
    paths = ensure_models()

    playlist: list[tuple[str, Path]] = []

    print("\nLoading Kokoro…")
    kokoro = make_tts("kokoro", paths)
    for name, sid in KOKORO_SPEAKERS:
        out_dir = OUT_ROOT / f"sherpa_kokoro_{name}"
        for pid, text in phrases:
            out = out_dir / f"{pid}.wav"
            print(f"  kokoro/{name}: {pid}")
            synth(kokoro, text, sid, out)
            playlist.append((f"Kokoro {name} — {pid}", out))

    print("\nLoading Matcha LJSpeech…")
    matcha = make_tts("matcha", paths)
    out_dir = OUT_ROOT / "sherpa_matcha_ljspeech"
    for pid, text in phrases:
        out = out_dir / f"{pid}.wav"
        print(f"  matcha/ljspeech: {pid}")
        synth(matcha, text, 0, out)
        playlist.append((f"Matcha LJSpeech — {pid}", out))

    print("\nLoading Kitten nano…")
    kitten = make_tts("kitten", paths)
    out_dir = OUT_ROOT / "sherpa_kitten_nano_sid0"
    for pid, text in phrases:
        out = out_dir / f"{pid}.wav"
        print(f"  kitten/sid0: {pid}")
        synth(kitten, text, 0, out)
        playlist.append((f"Kitten nano sid0 — {pid}", out))

    print(f"\nWrote {len(playlist)} clips under {OUT_ROOT}")
    if not args.no_play:
        print("Playing… (Ctrl-C to stop)")
        for label, path in playlist:
            play(path, label)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
