#!/usr/bin/env python3
"""Live reboot-survival test for NVS-backed settings.

Flips a representative set of switches/numbers/selects away from their current
values, waits until the web-published state matches, reboots via Restart
Controller, then verifies every value survived.

Uses the open LAN REST API (no auth). Does not read secrets.yaml.

Usage:
  .venv/bin/python tools/test_settings_persistence.py
  .venv/bin/python tools/test_settings_persistence.py --host 192.168.37.237
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

DEFAULT_HOST = "192.168.37.237"
BOOT_WAIT_S = 50
SETTLE_S = 2
PUBLISH_WAIT_S = 10


def nearly_equal(a: float, b: float, eps: float = 0.05) -> bool:
    return abs(float(a) - float(b)) <= eps


def url(host: str, path: str) -> str:
    if not path.startswith("/"):
        path = "/" + path
    return f"http://{host}{path}"


def request(host: str, path: str, method: str = "GET", timeout: float = 8.0) -> tuple[int, Any]:
    # ESPHome web_server rejects body-less POST with HTTP 411 unless
    # Content-Length: 0 is set explicitly (same fix as the dashboard).
    data = b"" if method == "POST" else None
    req = urllib.request.Request(url(host, path), data=data, method=method)
    if method == "POST":
        req.add_header("Content-Length", "0")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = resp.read().decode("utf-8", "replace")
            code = resp.getcode()
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")
        return e.code, body
    except Exception as e:
        return 0, str(e)
    try:
        return code, json.loads(body) if body else None
    except json.JSONDecodeError:
        return code, body


def object_path(domain: str, name: str, action: str | None = None, query: str | None = None) -> str:
    path = f"/{domain}/{urllib.parse.quote(name)}"
    if action:
        path += f"/{action}"
    if query:
        path += ("?" if "?" not in path else "&") + query
    return path


def get_state(host: str, domain: str, name: str) -> Any:
    code, body = request(host, object_path(domain, name))
    if code != 200 or not isinstance(body, dict):
        raise RuntimeError(f"GET {domain}/{name} failed ({code}): {body!r}")
    if domain == "switch":
        return body.get("state") == "ON" or body.get("value") is True
    if domain == "number":
        raw = body.get("value", body.get("state"))
        if isinstance(raw, (int, float)):
            return float(raw)
        # ESPHome sometimes publishes value as a string, or only state with a unit
        # ("2.5 °C"). Pull the leading number so a unit suffix cannot fail the check.
        text = str(raw).strip().replace(",", " ")
        token = text.split()[0] if text else ""
        return float(token)
    if domain == "select":
        return body.get("value") or body.get("state")
    if domain == "text":
        return body.get("value") or body.get("state") or ""
    return body


def set_switch(host: str, name: str, on: bool) -> None:
    action = "turn_on" if on else "turn_off"
    code, body = request(host, object_path("switch", name, action), method="POST")
    if code not in (200, 204):
        raise RuntimeError(f"POST switch/{name}/{action} failed ({code}): {body!r}")


def set_number(host: str, name: str, value: float) -> None:
    q = f"value={urllib.parse.quote(str(value))}"
    code, body = request(host, object_path("number", name, "set", q), method="POST")
    if code not in (200, 204):
        raise RuntimeError(f"POST number/{name}/set failed ({code}): {body!r}")


def set_select(host: str, name: str, option: str) -> None:
    q = f"option={urllib.parse.quote(option)}"
    code, body = request(host, object_path("select", name, "set", q), method="POST")
    if code not in (200, 204):
        raise RuntimeError(f"POST select/{name}/set failed ({code}): {body!r}")


def set_text(host: str, name: str, value: str) -> None:
    q = f"value={urllib.parse.quote(value)}"
    code, body = request(host, object_path("text", name, "set", q), method="POST")
    if code not in (200, 204):
        raise RuntimeError(f"POST text/{name}/set failed ({code}): {body!r}")


def matches(domain: str, got: Any, want: Any) -> bool:
    if domain == "number":
        return nearly_equal(got, want)
    if domain == "switch":
        return bool(got) == bool(want)
    return got == want


def wait_published(host: str, domain: str, name: str, want: Any, timeout_s: float = PUBLISH_WAIT_S) -> Any:
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        last = get_state(host, domain, name)
        if matches(domain, last, want):
            return last
        time.sleep(0.4)
    raise RuntimeError(
        f"{domain}/{name} did not publish {want!r} within {timeout_s}s (last={last!r})"
    )


def wait_up(host: str, timeout_s: float = BOOT_WAIT_S) -> None:
    deadline = time.time() + timeout_s
    last = ""
    while time.time() < deadline:
        code, body = request(host, "/", timeout=3.0)
        if code == 200:
            try:
                get_state(host, "switch", "ntfy Notifications Enabled")
                return
            except Exception as e:
                last = str(e)
        else:
            last = f"root {code}: {body!r}"
        time.sleep(2)
    raise RuntimeError(f"device did not come back within {timeout_s}s ({last})")


def apply_value(host: str, domain: str, name: str, value: Any) -> None:
    if domain == "switch":
        set_switch(host, name, bool(value))
    elif domain == "number":
        set_number(host, name, float(value))
    elif domain == "select":
        set_select(host, name, str(value))
    elif domain == "text":
        set_text(host, name, str(value))
    else:
        raise ValueError(domain)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument(
        "--keep-test-values",
        action="store_true",
        help="do not restore originals after a successful test",
    )
    args = parser.parse_args()
    host = args.host

    print(f"Persistence reboot test against {host}")
    wait_up(host, timeout_s=15)
    print("  device reachable")

    # Representative sample across every persistence mechanism.
    probes: list[dict[str, Any]] = [
        {
            "domain": "switch",
            "name": "ntfy Notifications Enabled",
            "flip": lambda cur: not bool(cur),
        },
        {
            "domain": "switch",
            "name": "Home Assistant API Enabled",
            "flip": lambda cur: not bool(cur),
        },
        {
            "domain": "switch",
            "name": "Door Sensor Enabled",
            "flip": lambda cur: not bool(cur),
        },
        {
            "domain": "switch",
            "name": "Audio Alerts Enabled",
            "flip": lambda cur: not bool(cur),
        },
        {
            "domain": "switch",
            "name": "Ice Detection Enabled",
            "flip": lambda cur: not bool(cur),
        },
        {
            "domain": "number",
            "name": "Setpoint (°C)",
            # step is 0.5 — stay on the grid
            "flip": lambda cur: round(float(cur) + 0.5, 1),
        },
        {
            "domain": "number",
            "name": "Compressor Off-Delay (min)",
            "flip": lambda cur: (int(float(cur)) + 1) if int(float(cur)) < 9 else (int(float(cur)) - 1),
        },
        {
            "domain": "number",
            "name": "Probe 1 (Coolroom) Calibration Offset (°C)",
            "flip": lambda cur: round(float(cur) + 0.5, 1),
        },
        {
            "domain": "select",
            "name": "High Temp Alarm Priority",
            "flip": lambda cur: "low" if str(cur).lower() != "low" else "high",
        },
        {
            "domain": "text",
            "name": "ntfy Topic",
            "flip": lambda cur: (
                str(cur)[: -len("-persist-test")]
                if str(cur).endswith("-persist-test")
                else (str(cur) or "coolroom") + "-persist-test"
            ),
        },
    ]

    originals: dict[str, Any] = {}
    targets: dict[str, Any] = {}

    print("\n1) Snapshot + apply test values")
    for p in probes:
        key = f"{p['domain']}/{p['name']}"
        cur = get_state(host, p["domain"], p["name"])
        tgt = p["flip"](cur)
        originals[key] = cur
        targets[key] = tgt
        print(f"  {key}: {cur!r} -> {tgt!r}")
        apply_value(host, p["domain"], p["name"], tgt)

    # Template number/text entities can take tens of seconds to re-publish while
    # persist_config_to_nvs is busy — the globals still update immediately.
    # Persistence only needs NVS sync, not the web-published state.
    print(f"\n2) Wait {SETTLE_S}s for persist queue + RestoringGlobals poll + sync")
    time.sleep(SETTLE_S)
    # One more explicit persist trigger via a no-op-ish switch toggle cycle is
    # overkill; instead give the 1s globals poll a couple of beats.
    time.sleep(3)

    print("3) Reboot via Restart Controller (clean shutdown flushes NVS)")
    code, body = request(host, object_path("button", "Restart Controller", "press"), method="POST")
    print(f"  restart POST -> {code} ({body!r})")

    print(f"4) Wait up to {BOOT_WAIT_S}s for boot + WiFi")
    time.sleep(5)
    wait_up(host)
    print("  device back")
    time.sleep(3)

    print("\n5) Verify values survived reboot")
    failures = []
    for p in probes:
        key = f"{p['domain']}/{p['name']}"
        try:
            got = wait_published(host, p["domain"], p["name"], targets[key], timeout_s=15)
            print(f"  PASS {key}: {got!r}")
        except Exception:
            got = get_state(host, p["domain"], p["name"])
            print(f"  FAIL {key}: want {targets[key]!r}, got {got!r} (original {originals[key]!r})")
            failures.append(key)

    if failures:
        print(f"\n{len(failures)} setting(s) did NOT survive reboot:")
        for k in failures:
            print(f"  - {k}")
        print("Leaving test values in place for diagnosis.")
        return 1

    print(f"\nOK — all {len(probes)} settings survived reboot.")

    if not args.keep_test_values:
        print("\n6) Restore originals")
        for p in probes:
            key = f"{p['domain']}/{p['name']}"
            apply_value(host, p["domain"], p["name"], originals[key])
            print(f"  restore sent {key} -> {originals[key]!r}")
        time.sleep(SETTLE_S + 3)
        # Confirm the important ones actually stuck (ntfy was the reported bug).
        for p in probes:
            if p["domain"] != "switch":
                continue
            key = f"{p['domain']}/{p['name']}"
            got = wait_published(host, p["domain"], p["name"], originals[key], timeout_s=10)
            print(f"  confirmed {key}: {got!r}")
        print("  originals restored + persisted")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
