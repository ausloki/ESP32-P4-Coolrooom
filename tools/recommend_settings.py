#!/usr/bin/env python3
"""Pull controller logs then recommend settings (Windows + macOS).

One-shot wrapper around pull_controller_logs.py + analyze_coolroom_logs.py.

Examples:

  python tools/recommend_settings.py --host 192.168.37.237
  python tools/recommend_settings.py --host 192.168.37.237 --days 14
  python tools/recommend_settings.py --host 192.168.37.237 --out logs/controller/run1

Requires: controller on LAN with SD card mounted and temperature/event logs present.
Does not change any settings on the device.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
_REPO = _TOOLS.parent


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Controller IP or hostname")
    p.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Directory for pulled logs (default logs/controller/<UTC stamp>)",
    )
    p.add_argument("--days", type=int, default=0, help="Only last N dated temp CSVs (0 = all)")
    p.add_argument("--kind", choices=("all", "temps", "events"), default="all")
    p.add_argument("--json-out", type=Path, default=None, help="Optional analysis JSON path")
    p.add_argument("--min-samples", type=int, default=24)
    p.add_argument("--python", default=sys.executable, help="Python interpreter to re-invoke helpers")
    return p.parse_args()


def main() -> int:
    args = _parse_args()
    out = args.out
    if out is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        out = _REPO / "logs" / "controller" / stamp
    out = out if out.is_absolute() else (_REPO / out)

    pull_cmd = [
        args.python,
        str(_TOOLS / "pull_controller_logs.py"),
        "--host",
        args.host,
        "--out",
        str(out),
        "--kind",
        args.kind,
    ]
    if args.days:
        pull_cmd.extend(["--days", str(args.days)])

    print("→", " ".join(pull_cmd))
    r = subprocess.run(pull_cmd, cwd=str(_REPO))
    if r.returncode != 0:
        return r.returncode

    json_out = args.json_out
    if json_out is None:
        json_out = out / "settings_recommendations.json"
    elif not json_out.is_absolute():
        json_out = _REPO / json_out

    analyze_cmd = [
        args.python,
        str(_TOOLS / "analyze_coolroom_logs.py"),
        str(out),
        "--host",
        args.host,
        "--json-out",
        str(json_out),
        "--min-samples",
        str(args.min_samples),
    ]
    print("→", " ".join(analyze_cmd))
    r = subprocess.run(analyze_cmd, cwd=str(_REPO))
    return r.returncode


if __name__ == "__main__":
    raise SystemExit(main())
