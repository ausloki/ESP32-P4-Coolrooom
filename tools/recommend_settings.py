#!/usr/bin/env python3
"""Pull controller logs then recommend settings (Windows + macOS).

Thin wrapper around analyse_logs_tune_settings.py (preferred entry point).
Enforces the ≥30-day history gate; does not change device settings unless you
pass through --apply --yes yourself via that script.

Examples:

  python tools/recommend_settings.py --host 192.168.37.237
  python tools/recommend_settings.py --host 192.168.37.237 --days 60
  python tools/recommend_settings.py --host 192.168.37.237 --out logs/controller/run1

Requires: controller on LAN with SD card mounted and ≥30 days of events/temps.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
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
    p.add_argument("--json-out", type=Path, default=None, help="Optional analysis JSON path")
    p.add_argument("--report", type=Path, default=None, help="Optional HTML report path")
    p.add_argument("--min-samples", type=int, default=24)
    p.add_argument("--min-days", type=float, default=30.0)
    p.add_argument("--python", default=sys.executable, help="Python interpreter to re-invoke helpers")
    return p.parse_args()


def main() -> int:
    args = _parse_args()
    cmd = [
        args.python,
        str(_TOOLS / "analyse_logs_tune_settings.py"),
        "--host",
        args.host,
        "--min-samples",
        str(args.min_samples),
        "--min-days",
        str(args.min_days),
        "--python",
        args.python,
    ]
    if args.out is not None:
        cmd.extend(["--out", str(args.out)])
    if args.days:
        cmd.extend(["--days", str(args.days)])
    if args.json_out is not None:
        cmd.extend(["--json-out", str(args.json_out)])
    if args.report is not None:
        cmd.extend(["--report", str(args.report)])

    print("→", " ".join(cmd))
    return subprocess.run(cmd, cwd=str(_REPO)).returncode


if __name__ == "__main__":
    raise SystemExit(main())