#!/usr/bin/env python3
"""Analyse coolroom SD logs and recommend (optionally apply) setting tweaks.

Cross-platform (Windows + macOS). Stdlib + project helpers only.

Requires ≥ 30 days of usable history before recommending:
  • Prefer events.csv (last − first parseable timestamp ≥ 30 days).
  • If events.csv is missing / unparseable, fall back to dated YYYY-MM-DD.csv
    filename span. If events.csv exists but is shorter than 30 days, exit 2
    (do not invent tweaks from a short event window).

Default is recommend-only. --apply writes only an allowlisted set of number
entities (never setpoint, never probe/sensor enables) and needs --yes.

Examples:

  # Local pulled logs (recommend only)
  python tools/analyse_logs_tune_settings.py --log-dir logs/controller/latest

  # Pull from device then analyse
  python tools/analyse_logs_tune_settings.py --host 192.168.37.237

  # HTML report
  python tools/analyse_logs_tune_settings.py --log-dir path --report out.html

  # Apply allowlisted suggestions (explicit)
  python tools/analyse_logs_tune_settings.py --host 192.168.1.50 --apply --yes

Wrappers: tools/analyse_logs_tune_settings.{cmd,ps1,sh,command}
See tools/README_LOG_TUNING.md
"""

from __future__ import annotations

import argparse
import html
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
_REPO = _TOOLS.parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from analyze_coolroom_logs import (  # noqa: E402
    MIN_HISTORY_DAYS,
    attach_current,
    build_report,
    print_report,
    report_to_dict,
)
from coolroom_http import (  # noqa: E402
    APPLY_ALLOWLIST,
    DEFAULT_HOST,
    PROFILE_2C,
    SETTING_NUMBERS,
    apply_number_settings,
    fetch_current_settings,
)


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    src = p.add_mutually_exclusive_group(required=False)
    src.add_argument(
        "--log-dir",
        type=Path,
        help="Local directory with events.csv and/or YYYY-MM-DD.csv",
    )
    src.add_argument(
        "--host",
        default=None,
        help=f"Controller IP — pull logs via GET /logs then analyse (default {DEFAULT_HOST} if used alone)",
    )
    p.add_argument(
        "--out",
        type=Path,
        default=None,
        help="When using --host, directory for pulled logs (default logs/controller/<UTC stamp>)",
    )
    p.add_argument(
        "--days",
        type=int,
        default=0,
        help="When pulling: only last N dated temp CSVs (0 = all). events.csv always pulled.",
    )
    p.add_argument(
        "--min-days",
        type=float,
        default=float(MIN_HISTORY_DAYS),
        help=f"Minimum history span in days (default {MIN_HISTORY_DAYS})",
    )
    p.add_argument("--min-samples", type=int, default=24)
    p.add_argument(
        "--settings-host",
        default=None,
        help="Host for live current-settings compare / --apply (defaults to --host)",
    )
    p.add_argument("--json-out", type=Path, default=None)
    p.add_argument(
        "--report",
        type=Path,
        default=None,
        help="Write printable HTML report (sibling style to recommended_settings_2c.html)",
    )
    p.add_argument(
        "--apply",
        action="store_true",
        help="POST allowlisted number suggestions to the controller (requires --yes)",
    )
    p.add_argument(
        "--yes",
        action="store_true",
        help="Confirm --apply without interactive prompt",
    )
    p.add_argument(
        "--python",
        default=sys.executable,
        help="Interpreter used to re-invoke pull_controller_logs.py",
    )
    # Positional alias: analyse_logs_tune_settings.py <log_dir>
    p.add_argument(
        "log_dir_positional",
        nargs="?",
        type=Path,
        default=None,
        help=argparse.SUPPRESS,
    )
    return p.parse_args()


def _pull_logs(host: str, out: Path, days: int, python: str) -> int:
    cmd = [
        python,
        str(_TOOLS / "pull_controller_logs.py"),
        "--host",
        host,
        "--out",
        str(out),
        "--kind",
        "all",
    ]
    if days:
        cmd.extend(["--days", str(days)])
    print("→", " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=str(_REPO)).returncode


def _write_html_report(path: Path, report, host: str | None) -> None:
    rows_html = []
    for rec in report.recommendations:
        sug = "—" if rec.suggested is None else f"{rec.suggested:g} {html.escape(rec.unit)}"
        cur = "—" if rec.current is None else f"{rec.current:g}"
        p2 = "—" if rec.profile_2c is None else f"{rec.profile_2c:g}"
        ap = "allowlisted" if rec.apply_safe else "manual only"
        rows_html.append(
            "<tr>"
            f"<td>{html.escape(rec.label)}</td>"
            f"<td class='val'>{html.escape(sug)}</td>"
            f"<td>{html.escape(cur)}</td>"
            f"<td>{html.escape(p2)}</td>"
            f"<td>{html.escape(rec.confidence)}</td>"
            f"<td>{ap}</td>"
            f"<td>{html.escape(rec.rationale)}</td>"
            "</tr>"
        )
    notes = "".join(f"<li>{html.escape(n)}</li>" for n in report.notes)
    hist = report.history.detail if report.history else "(none)"
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    body = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Coolroom log tune — recommendations</title>
  <style>
    :root {{ --ink:#1a1a1a; --muted:#555; --rule:#ccc; --bg:#fff; --band:#f4f4f2; --accent:#0b5fff; }}
    body {{ margin:0; color:var(--ink); background:var(--bg);
      font:11pt/1.45 "Iowan Old Style","Palatino Linotype",Palatino,Georgia,serif; }}
    .page {{ max-width:210mm; margin:0 auto; padding:18mm 16mm 20mm; }}
    h1 {{ margin:0 0 4px; font-size:18pt; }}
    .meta {{ color:var(--muted); font-size:9.5pt;
      font-family:"IBM Plex Sans","Helvetica Neue",Helvetica,Arial,sans-serif; }}
    h2 {{ margin:22px 0 8px; font-size:12.5pt; border-bottom:1px solid var(--rule); }}
    table {{ width:100%; border-collapse:collapse; font-size:9.5pt; }}
    th,td {{ border-bottom:1px solid var(--rule); padding:5px 6px; text-align:left; vertical-align:top; }}
    th {{ font-family:"IBM Plex Sans","Helvetica Neue",Helvetica,Arial,sans-serif; font-size:8.5pt; }}
    td.val {{ font-weight:600; white-space:nowrap; }}
    .callout {{ background:var(--band); border-left:3px solid var(--accent);
      padding:10px 12px; margin:12px 0; font-size:10pt; }}
    ul {{ margin:0 0 12px; padding-left:1.2em; }}
  </style>
</head>
<body>
  <div class="page">
    <header>
      <h1>Coolroom log analysis — setting recommendations</h1>
      <div class="meta">Generated {html.escape(generated)}
        · log dir {html.escape(str(report.log_dir))}
        {(" · host " + html.escape(host)) if host else ""}</div>
    </header>
    <div class="callout">
      <strong>History:</strong> {html.escape(hist)}<br>
      Default mode is recommend-only. Apply uses ESPHome REST number POSTs on the LAN
      (no secrets printed). Never auto-changes setpoint or probe enables.
      Baseline profile: Quick Start §4 / <code>recommended_settings_2c.html</code>.
    </div>
    <h2>Current vs suggested</h2>
    <table>
      <thead>
        <tr>
          <th>Setting</th><th>Suggested</th><th>Current</th><th>2 °C profile</th>
          <th>Conf</th><th>Apply</th><th>Rationale</th>
        </tr>
      </thead>
      <tbody>
        {"".join(rows_html) or "<tr><td colspan='7'>No recommendations</td></tr>"}
      </tbody>
    </table>
    <h2>Notes</h2>
    <ul>{notes or "<li>None</li>"}</ul>
    <p class="meta">Apply allowlist: {html.escape(", ".join(sorted(APPLY_ALLOWLIST)))}</p>
  </div>
</body>
</html>
"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")


def _confirm_apply(changes: dict[str, float], host: str, yes: bool) -> bool:
    if not changes:
        print("Nothing allowlisted to apply.")
        return False
    print("\nAbout to POST these allowlisted numbers to", host)
    for key, val in changes.items():
        print(f"  {SETTING_NUMBERS.get(key, key):40s}  →  {val:g}")
    if yes:
        return True
    try:
        ans = input("Type YES to apply: ").strip()
    except EOFError:
        ans = ""
    return ans == "YES"


def main() -> int:
    args = _parse_args()
    log_dir = args.log_dir or args.log_dir_positional
    host = args.host

    # Allow: analyse_logs_tune_settings.py --host X  OR bare host via wrappers
    if log_dir is None and host is None:
        # Interactive-friendly default: require an explicit source
        print(
            "ERROR: provide --log-dir PATH or --host IP\n"
            "  Example: python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_30d\n"
            "  Example: python tools/analyse_logs_tune_settings.py --host 192.168.37.237",
            file=sys.stderr,
        )
        return 1

    if host and log_dir is None:
        out = args.out
        if out is None:
            stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            out = _REPO / "logs" / "controller" / stamp
        elif not out.is_absolute():
            out = _REPO / out
        rc = _pull_logs(host, out, args.days, args.python)
        if rc != 0:
            return rc
        log_dir = out

    assert log_dir is not None
    if not log_dir.is_absolute():
        log_dir = (_REPO / log_dir).resolve()

    try:
        report = build_report(log_dir, min_samples=args.min_samples)
    except FileNotFoundError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    hist = report.history
    if hist is None or hist.span_days < args.min_days:
        detail = hist.detail if hist else "no history measured"
        span = hist.span_days if hist else 0.0
        print(
            f"ERROR: need ≥ {args.min_days:g} days of usable log history before recommending tweaks.\n"
            f"  Found {span:.1f} days ({detail}).\n"
            f"  Gate rule: prefer events.csv timestamp span; if events.csv is missing/unparseable,\n"
            f"  use dated YYYY-MM-DD.csv filename span. Short events.csv does not fall back to temps.\n"
            f"  Device prerequisites: SD card mounted, logging enabled, web reachable on LAN.",
            file=sys.stderr,
        )
        return 2

    settings_host = args.settings_host or host
    if settings_host == "auto":
        settings_host = DEFAULT_HOST
    if settings_host:
        try:
            current = fetch_current_settings(settings_host)
            attach_current(report, current)
            print(f"(Live settings from {settings_host})\n")
        except Exception as e:
            print(
                f"WARNING: could not read live settings from {settings_host}: {e}\n",
                file=sys.stderr,
            )

    print_report(report)

    if args.json_out:
        out_json = args.json_out if args.json_out.is_absolute() else _REPO / args.json_out
        out_json.parent.mkdir(parents=True, exist_ok=True)
        payload = report_to_dict(report)
        payload["profile_2c"] = PROFILE_2C
        payload["apply_allowlist"] = sorted(APPLY_ALLOWLIST)
        out_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"\nJSON report → {out_json}")

    if args.report:
        out_html = args.report if args.report.is_absolute() else _REPO / args.report
        _write_html_report(out_html, report, settings_host)
        print(f"HTML report → {out_html}")

    if args.apply:
        if not settings_host:
            print("ERROR: --apply requires --host or --settings-host", file=sys.stderr)
            return 1
        changes: dict[str, float] = {}
        for rec in report.recommendations:
            if not rec.apply_safe or rec.suggested is None:
                continue
            if rec.key not in APPLY_ALLOWLIST:
                continue
            # Skip no-ops when we know current
            if rec.current is not None and abs(rec.current - rec.suggested) < 0.05:
                continue
            changes[rec.key] = float(rec.suggested)
        if not _confirm_apply(changes, settings_host, args.yes):
            print("Apply cancelled.")
            return 3
        results = apply_number_settings(settings_host, changes)
        errors = 0
        for key, val, err in results:
            if err:
                errors += 1
                print(f"  FAIL {key}={val:g}: {err}", file=sys.stderr)
            else:
                print(f"  OK   {SETTING_NUMBERS.get(key, key)} → {val:g}")
        if errors:
            return 4
        print(
            "Applied. Values stage to NVS via the controller; reboot (not USB factory flash) to confirm persistence."
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
