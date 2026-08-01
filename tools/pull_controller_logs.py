#!/usr/bin/env python3
"""Download coolroom SD logs from a live controller (Windows + macOS).

Uses GET /logs and GET /logs/download?file=… (p4_log_manager.h). Stdlib only.

Examples (same on both OSes once Python 3 is on PATH):

  python tools/pull_controller_logs.py --host 192.168.37.237
  python tools/pull_controller_logs.py --host 192.168.37.237 --out logs/controller
  python tools/pull_controller_logs.py --list-only
  python tools/pull_controller_logs.py --kind temps
  python tools/pull_controller_logs.py --kind events
  python tools/pull_controller_logs.py --days 7
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

# Allow `python tools/pull_controller_logs.py` without installing a package.
_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from coolroom_http import DEFAULT_HOST, download_log_file, list_logs  # noqa: E402

_DATE_CSV = re.compile(r"^(\d{4}-\d{2}-\d{2})\.csv$", re.IGNORECASE)


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default=DEFAULT_HOST, help=f"Controller IP or hostname (default {DEFAULT_HOST})")
    p.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Output directory (default: logs/controller/<UTC stamp>)",
    )
    p.add_argument(
        "--kind",
        choices=("all", "temps", "events", "backup"),
        default="all",
        help="Which files to download (default all)",
    )
    p.add_argument(
        "--days",
        type=int,
        default=0,
        help="Only dated temp CSVs from the last N calendar days (0 = all)",
    )
    p.add_argument("--list-only", action="store_true", help="Print /logs listing and exit")
    p.add_argument("--timeout", type=float, default=30.0, help="List request timeout seconds")
    p.add_argument("--download-timeout", type=float, default=120.0, help="Per-file download timeout")
    return p.parse_args()


def _want_file(name: str, kind: str, days: int, today: date) -> bool:
    lower = name.lower()
    is_events = lower == "events.csv"
    is_backup = lower == "backup.json"
    is_nodate = lower == "nodate.csv"
    m = _DATE_CSV.match(name)
    is_dated = m is not None

    if kind == "events":
        return is_events
    if kind == "backup":
        return is_backup
    if kind == "temps":
        if not (is_dated or is_nodate):
            return False
    elif kind == "all":
        pass
    else:
        return False

    if days > 0 and is_dated and m is not None:
        try:
            d = date.fromisoformat(m.group(1))
        except ValueError:
            return False
        return d >= (today - timedelta(days=days - 1))
    return True


def main() -> int:
    args = _parse_args()
    try:
        listing = list_logs(args.host, timeout=args.timeout)
    except Exception as e:
        print(f"ERROR: could not reach {args.host} /logs — {e}", file=sys.stderr)
        print("Is the controller on the LAN? Is an SD card mounted?", file=sys.stderr)
        return 1

    if not listing.get("mounted"):
        print("ERROR: SD card not mounted on controller.", file=sys.stderr)
        print(json.dumps(listing, indent=2))
        return 2

    files = listing.get("files") or []
    print(
        f"Controller {args.host}: card={listing.get('card')!r} "
        f"free={listing.get('free_mb')} MB / total={listing.get('total_mb')} MB "
        f"({len(files)} file(s))"
    )

    if args.list_only:
        for f in files:
            print(
                f"  {f.get('name'):20s}  {int(f.get('size') or 0):10d} B  "
                f"kind={f.get('kind')}  mtime={f.get('mtime')}"
            )
        return 0

    today = datetime.now(timezone.utc).date()
    selected = [f for f in files if _want_file(str(f.get("name") or ""), args.kind, args.days, today)]
    if not selected:
        print("No matching files to download.")
        return 0

    out = args.out
    if out is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        out = Path("logs") / "controller" / stamp
    out.mkdir(parents=True, exist_ok=True)

    meta = {
        "host": args.host,
        "pulled_at_utc": datetime.now(timezone.utc).isoformat(),
        "card": listing,
        "downloaded": [],
    }

    errors = 0
    for f in selected:
        name = str(f.get("name") or "")
        dest = out / name
        try:
            data = download_log_file(args.host, name, timeout=args.download_timeout)
            dest.write_bytes(data)
            meta["downloaded"].append({"name": name, "bytes": len(data), "path": str(dest)})
            print(f"  saved {name} ({len(data)} bytes) → {dest}")
        except Exception as e:
            errors += 1
            print(f"  FAILED {name}: {e}", file=sys.stderr)

    meta_path = out / "pull_manifest.json"
    meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print(f"Manifest → {meta_path}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
