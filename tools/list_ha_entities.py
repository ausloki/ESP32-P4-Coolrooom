#!/usr/bin/env python3
"""List the entities this controller offers over the ESPHome native API.

This is what Home Assistant sees after it completes the encrypted handshake, so
it separates "the device is not exposing anything" from "Home Assistant has a
stale config entry". Entities marked ``internal: true`` never reach the API and
so will not appear here.

    .venv/bin/python tools/list_ha_entities.py --host 192.168.37.237

The encryption key is read from secrets.yaml and never printed.
"""

from __future__ import annotations

import argparse
import asyncio
import re
import sys
from collections import Counter
from pathlib import Path

from aioesphomeapi import APIClient

REPO = Path(__file__).resolve().parent.parent
SECRETS = REPO / "secrets.yaml"


def read_secret(name: str) -> str:
    if not SECRETS.exists():
        sys.exit(f"secrets.yaml not found at {SECRETS}")
    for line in SECRETS.read_text(encoding="utf-8").splitlines():
        m = re.match(rf"^\s*{re.escape(name)}\s*:\s*(.+?)\s*$", line)
        if m:
            return m.group(1).strip().strip('"').strip("'")
    sys.exit(f"secret '{name}' not found in secrets.yaml")


async def run(host: str, port: int, verbose: bool) -> int:
    client = APIClient(host, port, None, noise_psk=read_secret("api_encryption_key"))
    try:
        await client.connect(login=True)
    except Exception as exc:  # noqa: BLE001 - surface any handshake failure verbatim
        print(f"FAILED to connect/handshake with {host}:{port}: {exc}")
        return 2

    try:
        device = await client.device_info()
        entities, services = await client.list_entities_services()
    finally:
        await client.disconnect()

    kinds = Counter(type(e).__name__.replace("Info", "").lower() for e in entities)

    print(f"Device      : {device.name} (ESPHome {device.esphome_version})")
    print(f"Model       : {device.model}")
    print(f"Entities    : {len(entities)}")
    print(f"User services: {len(services)}")
    print()
    for kind, count in sorted(kinds.items()):
        print(f"  {kind:<16} {count}")

    if verbose:
        print()
        for e in sorted(entities, key=lambda e: (type(e).__name__, e.name)):
            kind = type(e).__name__.replace("Info", "").lower()
            print(f"  [{kind}] {e.name}  (object_id={e.object_id})")

    return 0 if entities else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", required=True, help="controller IP or hostname")
    ap.add_argument("--port", type=int, default=6053)
    ap.add_argument("-v", "--verbose", action="store_true", help="list every entity")
    args = ap.parse_args()
    return asyncio.run(run(args.host, args.port, args.verbose))


if __name__ == "__main__":
    raise SystemExit(main())
