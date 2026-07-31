#!/usr/bin/env python3
"""Check the web dashboard against the device's entity list.

The settings UI in ``assets/dashboard.html`` is a hand-maintained JavaScript
array (``ADVANCED_SETTINGS_GROUPS``) plus some hand-written panels. Nothing
generates it from the ESPHome config, so a setting added to
``esp32-p4-coolroom.yaml`` silently fails to appear in the web GUI unless
someone also edits the dashboard. This script catches that drift.

It also re-checks the persistence contract this project relies on: switches
declared ``restore_mode: DISABLED`` and all ``number`` entities keep their
value in a global with ``restore_value: yes`` and write it through
``persist_config_to_nvs``. Miss either half and the setting resets on reboot.

Usage:
  python tools/check_dashboard_coverage.py
  python tools/check_dashboard_coverage.py --verbose

Exit status is 1 when an unexplained gap is found, so it can gate closeout.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "esp32-p4-coolroom.yaml"
DASHBOARD = ROOT / "assets" / "dashboard.html"

CONTROL_DOMAINS = ("switch", "number", "select", "text", "button")

# Entities intentionally absent from the custom dashboard. Keep the reason with
# the entry — an unexplained name here defeats the point of the check.
EXPECTED_ABSENT = {
    "New WiFi SSID": "dashboard posts to /api/wifi/connect instead of the text entity",
    "New WiFi Password (submit after SSID)": "see New WiFi SSID",
}


class _Loader(yaml.SafeLoader):
    """SafeLoader that tolerates ESPHome's !lambda / !secret / !include tags."""


def _passthrough(loader, suffix, node):
    if isinstance(node, yaml.ScalarNode):
        return loader.construct_scalar(node)
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)


_Loader.add_multi_constructor("!", _passthrough)


def object_id(name: str) -> str:
    """Mirror ESPHome's object_id sanitisation ([a-z0-9_-] kept, rest -> _)."""
    return "".join(c if (c.isalnum() or c in "_-") else "_" for c in name.lower())


def load_config() -> dict:
    with CONFIG.open() as fh:
        return yaml.load(fh, Loader=_Loader)


def entities(doc: dict, domain: str) -> list[dict]:
    items = doc.get(domain) or []
    if isinstance(items, dict):
        items = [items]
    return [e for e in items if isinstance(e, dict)]


def check_coverage(doc: dict, html: str, verbose: bool) -> list[str]:
    problems = []
    exposed = 0
    for domain in CONTROL_DOMAINS:
        for e in entities(doc, domain):
            name = e.get("name")
            if not name or e.get("internal"):
                continue
            if name in html or object_id(name) in html:
                exposed += 1
                continue
            if name in EXPECTED_ABSENT:
                if verbose:
                    print(f"  skip [{domain}] {name} — {EXPECTED_ABSENT[name]}")
                continue
            problems.append(
                f"[{domain}] {name!r} (object_id={object_id(name)}) is not referenced "
                f"in the dashboard — add it to ADVANCED_SETTINGS_GROUPS or a hand-written "
                f"panel, or list it in EXPECTED_ABSENT with a reason"
            )
    if verbose:
        print(f"  {exposed} controls exposed in the dashboard")
    return problems


def check_persistence(doc: dict, verbose: bool) -> list[str]:
    problems = []
    globals_by_id = {g.get("id"): g for g in (doc.get("globals") or []) if isinstance(g, dict)}

    def restores(gid) -> bool:
        return globals_by_id.get(gid, {}).get("restore_value") in (True, "yes")

    def backing_globals(*chunks) -> list[str]:
        blob = "".join(json.dumps(c, default=str) for c in chunks)
        return [g for g in globals_by_id if g and g in blob]

    def persisted(*chunks) -> bool:
        blob = "".join(json.dumps(c, default=str) for c in chunks)
        return "persist_config_to_nvs" in blob

    checked = 0
    for e in entities(doc, "switch"):
        if e.get("restore_mode") != "DISABLED":
            continue
        checked += 1
        on, off = e.get("turn_on_action"), e.get("turn_off_action")
        backed = [g for g in backing_globals(on, off, e.get("lambda")) if restores(g)]
        if not backed:
            problems.append(
                f"[switch] {e.get('name')!r} uses restore_mode: DISABLED but writes no "
                f"global with restore_value: yes — it will reset on reboot"
            )
        if not (persisted(on) and persisted(off)):
            problems.append(
                f"[switch] {e.get('name')!r} does not call persist_config_to_nvs on both "
                f"turn_on_action and turn_off_action — one direction will not survive reboot"
            )

    for e in entities(doc, "number"):
        if e.get("internal"):
            continue
        checked += 1
        action = e.get("set_action")
        backed = [g for g in backing_globals(action) if restores(g)]
        if not backed:
            problems.append(
                f"[number] {e.get('name')!r} writes no global with restore_value: yes — "
                f"it will reset on reboot"
            )
        if not persisted(action):
            problems.append(
                f"[number] {e.get('name')!r} does not call persist_config_to_nvs in "
                f"set_action — it will reset on reboot"
            )

    if verbose:
        print(f"  {checked} NVS-backed entities checked")
    return problems


def check_persist_script(doc: dict, verbose: bool) -> list[str]:
    """Every restoring global must be staged by persist_config_to_nvs.

    ESPHome's RestoringGlobalsComponent only writes its preference from its own
    1 s poll, which runs *after* the action that changed the value returns. The
    script forces each one to stage its current value and then flushes NVS, so a
    global missing from that hand-maintained list can be lost if the controller
    reboots inside the poll window.
    """
    restoring = [
        g.get("id")
        for g in (doc.get("globals") or [])
        if isinstance(g, dict) and g.get("restore_value") in (True, "yes")
    ]
    scripts = [s for s in (doc.get("script") or []) if isinstance(s, dict)]
    persist = next((s for s in scripts if s.get("id") == "persist_config_to_nvs"), None)
    if persist is None:
        return ["script persist_config_to_nvs not found — settings cannot be flushed to NVS"]

    body = json.dumps(persist, default=str)
    problems = [
        f"[global] {gid!r} has restore_value: yes but is never staged by "
        f"persist_config_to_nvs — a reboot soon after changing it can lose the value"
        for gid in restoring
        if gid and f"{gid}->update()" not in body
    ]
    if "global_preferences->sync()" not in body.replace("\\n", "\n"):
        problems.append(
            "persist_config_to_nvs never calls global_preferences->sync() — staged "
            "values are not committed to NVS"
        )
    if verbose:
        print(f"  {len(restoring)} restoring globals, "
              f"{len(restoring) - len(problems)} staged by persist_config_to_nvs")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--verbose", action="store_true", help="show per-entity detail")
    args = parser.parse_args()

    if not CONFIG.exists() or not DASHBOARD.exists():
        print(f"ERROR: expected {CONFIG} and {DASHBOARD}", file=sys.stderr)
        return 2

    doc = load_config()
    html = DASHBOARD.read_text()

    print("Dashboard coverage:")
    coverage = check_coverage(doc, html, args.verbose)
    print("Setting persistence:")
    persistence = check_persistence(doc, args.verbose)
    print("NVS flush coverage:")
    flush = check_persist_script(doc, args.verbose)

    problems = coverage + persistence + flush
    if not problems:
        print("\nOK — every control is reachable from the dashboard and every "
              "NVS-backed setting survives a reboot.")
        return 0

    print(f"\n{len(problems)} problem(s):\n")
    for p in problems:
        print(f"  - {p}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
