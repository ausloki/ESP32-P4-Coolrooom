#!/usr/bin/env python3
"""Print door / alarm state transitions from the LAN web API.

Bench aid for checking the home-bell soft-mute contract: mute freezes the
alarm mask, and a *new* alarm bit (e.g. door) must lift it again.

Usage:
  .venv/bin/python tools/watch_alarm_mute.py --host 192.168.37.237 --seconds 180
"""

from __future__ import annotations

import argparse
import json
import time
import urllib.parse
import urllib.request

WATCH = [
    ("binary_sensor", "Door Reed Sensor"),
    ("binary_sensor", "Door Open Alarm"),
    ("binary_sensor", "Probe Fault - Stale or Invalid"),
    ("binary_sensor", "Any Alarm Active"),
    ("switch", "Siren Relay"),
]


def get_state(host: str, domain: str, name: str) -> str:
    url = f"http://{host}/{domain}/{urllib.parse.quote(name)}"
    try:
        with urllib.request.urlopen(url, timeout=3) as resp:
            return json.loads(resp.read().decode()).get("state", "?")
    except Exception as e:
        return f"ERR {e}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="192.168.37.237")
    ap.add_argument("--seconds", type=float, default=180.0)
    ap.add_argument("--interval", type=float, default=0.5)
    args = ap.parse_args()

    last: dict[str, str] = {}
    deadline = time.time() + args.seconds
    print(f"watching {args.host} for {args.seconds:.0f}s", flush=True)
    while time.time() < deadline:
        for domain, name in WATCH:
            state = get_state(args.host, domain, name)
            if last.get(name) != state:
                if name in last:
                    print(f"{time.strftime('%H:%M:%S')}  {name}: {last[name]} -> {state}", flush=True)
                else:
                    print(f"{time.strftime('%H:%M:%S')}  {name} = {state}", flush=True)
                last[name] = state
        time.sleep(args.interval)
    print("done", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
