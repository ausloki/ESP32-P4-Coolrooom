#!/usr/bin/env python3
"""Generate and play coolroom-alert speech samples from several TTS engines.

Engines:
  apple          macOS `say` (current production voice: Samantha)
  piper          local Piper neural TTS (rhasspy/piper)
  nabucasa       Home Assistant Cloud TTS via HA REST (JennyNeural / AriaNeural)
  openai         OpenAI Audio Speech API (alloy / shimmer)

Outputs WAV files under assets/audio/tts_compare/<engine>_<voice>/ and can play
them with afplay so you can A/B against the embedded Samantha clips.

Credentials (env or flags — never committed):
  OPENAI_API_KEY
  HA_URL          e.g. http://homeassistant.local:8123
  HA_TOKEN        long-lived access token (Nabu Casa Cloud must be logged in)

Examples:
  # Apple baseline + Piper (after tools/tts_compare/piper is installed)
  .venv/bin/python tools/compare_tts_voices.py --engines apple,piper --play

  # OpenAI
  OPENAI_API_KEY=sk-... .venv/bin/python tools/compare_tts_voices.py \\
      --engines openai --voices alloy,shimmer --play

  # Nabu Casa via Home Assistant
  HA_URL=http://192.168.1.10:8123 HA_TOKEN=... \\
      .venv/bin/python tools/compare_tts_voices.py \\
      --engines nabucasa --voices JennyNeural,AriaNeural --play
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_ROOT = ROOT / "assets" / "audio" / "tts_compare"
PIPER_DIR = ROOT / "tools" / "tts_compare"
PIPER_BIN = PIPER_DIR / "piper" / "piper"
# Good clear female US English — solid alert tone without being overly dramatic.
DEFAULT_PIPER_VOICE = "en_US-lessac-medium"
PIPER_VOICE_DIR = PIPER_DIR / "voices"

# Representative coolroom phrases (subset of scripts/generate_audio_clips.sh).
PHRASES: list[tuple[str, str]] = [
    ("alarm_high", "Attention. High temperature."),
    ("alarm_door", "Attention. Door open."),
    ("alarm_no_cool", "Attention. Not cooling."),
    ("info_cooling_on", "Cooling started."),
    ("info_audio_test", "Audio alerts are working."),
]

APPLE_VOICE_DEFAULT = "Samantha"
APPLE_RATE_DEFAULT = "170"


def _run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, **kw)


def _download(url: str, dest: Path) -> None:
    """Download with curl (system certs) — urllib often fails SSL on macOS Python.org builds."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    _run(["curl", "-fsSL", "-o", str(dest), url])


def _ensure_wav_16k_mono(src: Path, dest: Path) -> None:
    """Convert any audio afconvert understands → 16 kHz mono PCM16 WAV."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        aiff = Path(td) / "in.aiff"
        # Prefer afconvert (always on macOS). Fall back to copying if already WAVE/PCM.
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
            # Piper already emits WAV; copy if afconvert rejects the container.
            shutil.copy2(src, dest)


def gen_apple(voice: str, rate: str, out_dir: Path) -> list[Path]:
    paths: list[Path] = []
    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for pid, text in PHRASES:
            aiff = tmp / f"{pid}.aiff"
            wav = out_dir / f"{pid}.wav"
            print(f"  apple/{voice}: {pid}")
            _run(["say", "-v", voice, "-r", rate, "-o", str(aiff), text])
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
                    str(wav),
                ]
            )
            paths.append(wav)
    return paths


def _piper_model_paths(voice: str) -> tuple[Path, Path]:
    onnx = PIPER_VOICE_DIR / f"{voice}.onnx"
    cfg = PIPER_VOICE_DIR / f"{voice}.onnx.json"
    return onnx, cfg


def ensure_piper_voice(voice: str) -> Path:
    """Download Piper ONNX voice model into tools/tts_compare/voices/ if missing."""
    onnx, cfg = _piper_model_paths(voice)
    if onnx.exists() and cfg.exists():
        return onnx
    PIPER_VOICE_DIR.mkdir(parents=True, exist_ok=True)
    parts = voice.split("-")
    if len(parts) < 3:
        raise SystemExit(f"Unexpected piper voice id: {voice}")
    locale, name, quality = parts[0], parts[1], parts[2]
    lang = locale.split("_")[0]
    base = (
        "https://huggingface.co/rhasspy/piper-voices/resolve/main/"
        f"{lang}/{locale}/{name}/{quality}/{voice}"
    )
    for suffix, dest in ((".onnx", onnx), (".onnx.json", cfg)):
        url = base + suffix
        print(f"Downloading Piper voice…\n  {url}")
        _download(url, dest)
    return onnx


def gen_piper(voice: str, out_dir: Path) -> list[Path]:
    try:
        from piper import PiperVoice
    except ImportError as e:
        raise SystemExit(
            "piper-tts is not installed. Run:\n"
            "  .venv/bin/pip install piper-tts"
        ) from e
    import wave

    onnx = ensure_piper_voice(voice)
    pv = PiperVoice.load(str(onnx))
    paths: list[Path] = []
    out_dir.mkdir(parents=True, exist_ok=True)
    for pid, text in PHRASES:
        raw = out_dir / f"{pid}.src.wav"
        wav = out_dir / f"{pid}.wav"
        print(f"  piper/{voice}: {pid}")
        with wave.open(str(raw), "wb") as wf:
            pv.synthesize_wav(text, wf)
        _ensure_wav_16k_mono(raw, wav)
        raw.unlink(missing_ok=True)
        paths.append(wav)
    return paths


# Kept for docs / older callers — binary tarball is x86_64-only and incomplete on arm64.
def ensure_piper(voice: str) -> None:
    ensure_piper_voice(voice)


def gen_openai(voice: str, api_key: str, out_dir: Path, model: str) -> list[Path]:
    paths: list[Path] = []
    out_dir.mkdir(parents=True, exist_ok=True)
    for pid, text in PHRASES:
        wav = out_dir / f"{pid}.wav"
        raw = out_dir / f"{pid}.src"
        print(f"  openai/{voice}: {pid}")
        body = json.dumps(
            {"model": model, "voice": voice, "input": text, "response_format": "wav"}
        )
        # curl avoids macOS Python.org SSL cert issues with urllib.
        _run(
            [
                "curl",
                "-fsSL",
                "-o",
                str(raw),
                "-H",
                f"Authorization: Bearer {api_key}",
                "-H",
                "Content-Type: application/json",
                "-d",
                body,
                "https://api.openai.com/v1/audio/speech",
            ]
        )
        data = raw.read_bytes()
        if data[:1] == b"{":
            raise SystemExit(f"OpenAI TTS error: {data.decode('utf-8', errors='replace')}")
        if data[:4] == b"RIFF":
            wav.write_bytes(data)
            _ensure_wav_16k_mono(wav, wav)
        else:
            _ensure_wav_16k_mono(raw, wav)
        raw.unlink(missing_ok=True)
        paths.append(wav)
    return paths


def gen_nabucasa(
    voice: str, ha_url: str, ha_token: str, language: str, out_dir: Path
) -> list[Path]:
    """Use HA /api/tts_get_url then download the returned media path."""
    paths: list[Path] = []
    out_dir.mkdir(parents=True, exist_ok=True)
    base = ha_url.rstrip("/")
    for pid, text in PHRASES:
        wav = out_dir / f"{pid}.wav"
        print(f"  nabucasa/{voice}: {pid}")
        # Prefer modern tts.speak engine path via /api/tts_get_url
        payload = {
            "platform": "cloud",
            "message": text,
            "language": language,
            "options": {"voice": voice},
        }
        req = urllib.request.Request(
            f"{base}/api/tts_get_url",
            data=json.dumps(payload).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {ha_token}",
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                info = json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", errors="replace")
            raise SystemExit(
                f"HA tts_get_url failed ({e.code}): {detail}\n"
                "Need a long-lived token and an active Nabu Casa Cloud login on that HA."
            ) from e
        url = info.get("url") or info.get("path")
        if not url:
            raise SystemExit(f"Unexpected tts_get_url response: {info}")
        if url.startswith("/"):
            url = base + url
        media_req = urllib.request.Request(
            url, headers={"Authorization": f"Bearer {ha_token}"}
        )
        with urllib.request.urlopen(media_req, timeout=60) as resp:
            raw = out_dir / f"{pid}.src"
            raw.write_bytes(resp.read())
        _ensure_wav_16k_mono(raw, wav)
        raw.unlink(missing_ok=True)
        paths.append(wav)
    return paths


def play_paths(labeled: list[tuple[str, Path]], pause_s: float) -> None:
    for label, path in labeled:
        print(f"\n▶ {label}\n  {path.name}: {path}")
        _run(["afplay", str(path)])
        if pause_s > 0:
            _run(["sleep", str(pause_s)])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--engines",
        default="apple",
        help="Comma list: apple,piper,openai,nabucasa (default: apple)",
    )
    ap.add_argument(
        "--voices",
        default="",
        help="Comma list of engine-specific voices (defaults per engine)",
    )
    ap.add_argument("--play", action="store_true", help="Play samples with afplay")
    ap.add_argument("--pause", type=float, default=0.6, help="Pause between clips when playing")
    ap.add_argument("--apple-voice", default=APPLE_VOICE_DEFAULT)
    ap.add_argument("--apple-rate", default=APPLE_RATE_DEFAULT)
    ap.add_argument("--piper-voice", default=DEFAULT_PIPER_VOICE)
    ap.add_argument("--openai-model", default="gpt-4o-mini-tts", help="Or tts-1 / tts-1-hd")
    ap.add_argument("--ha-language", default="en-US")
    ap.add_argument("--openai-key", default=os.environ.get("OPENAI_API_KEY", ""))
    ap.add_argument("--ha-url", default=os.environ.get("HA_URL", ""))
    ap.add_argument("--ha-token", default=os.environ.get("HA_TOKEN", ""))
    args = ap.parse_args()

    engines = [e.strip().lower() for e in args.engines.split(",") if e.strip()]
    voice_override = [v.strip() for v in args.voices.split(",") if v.strip()]

    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    playlist: list[tuple[str, Path]] = []

    for engine in engines:
        if engine == "apple":
            voices = voice_override or [args.apple_voice]
            for v in voices:
                out = OUT_ROOT / f"apple_{v}"
                for p in gen_apple(v, args.apple_rate, out):
                    playlist.append((f"Apple {v} — {p.stem}", p))
        elif engine == "piper":
            voices = voice_override or [args.piper_voice]
            for v in voices:
                out = OUT_ROOT / f"piper_{v}"
                for p in gen_piper(v, out):
                    playlist.append((f"Piper {v} — {p.stem}", p))
        elif engine == "openai":
            if not args.openai_key:
                raise SystemExit(
                    "OpenAI needs OPENAI_API_KEY (or --openai-key). "
                    "Voices to try: alloy,shimmer"
                )
            voices = voice_override or ["alloy", "shimmer"]
            for v in voices:
                out = OUT_ROOT / f"openai_{v}"
                for p in gen_openai(v, args.openai_key, out, args.openai_model):
                    playlist.append((f"OpenAI {v} — {p.stem}", p))
        elif engine == "nabucasa":
            if not args.ha_url or not args.ha_token:
                raise SystemExit(
                    "Nabu Casa samples need HA_URL + HA_TOKEN (long-lived access token) "
                    "with Home Assistant Cloud signed in.\n"
                    "Voices to try: JennyNeural,AriaNeural\n"
                    "Or use HA UI: Settings → Home Assistant Cloud → Try TTS."
                )
            voices = voice_override or ["JennyNeural", "AriaNeural"]
            for v in voices:
                out = OUT_ROOT / f"nabucasa_{v}"
                for p in gen_nabucasa(v, args.ha_url, args.ha_token, args.ha_language, out):
                    playlist.append((f"Nabu Casa {v} — {p.stem}", p))
        else:
            raise SystemExit(f"Unknown engine: {engine}")

    print(f"\nWrote {len(playlist)} clips under {OUT_ROOT}")
    if args.play:
        print("Playing… (Ctrl-C to stop)")
        play_paths(playlist, args.pause)
    else:
        print("Pass --play to hear them, or open the WAVs in Finder.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
