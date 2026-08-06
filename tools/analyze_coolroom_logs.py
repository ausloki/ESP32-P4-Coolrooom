#!/usr/bin/env python3
"""Recommend coolroom control settings from pulled SD logs (Windows + macOS).

Primary input: daily temperature CSVs (5-minute samples):
  timestamp,coolroom_c,evap_c,ambient_c,setpoint_c,compressor,defrost,alarm_hi,alarm_lo,probe_fault

Optional: events.csv — compressor on/off edges and alarms (better cycle timing than 5-min samples).

This does **not** auto-write settings to the controller. It prints heuristics you can
apply by hand after checking the User Manual / Quick Start.

Examples:

  python tools/analyze_coolroom_logs.py logs/controller/20260801T120000Z
  python tools/analyze_coolroom_logs.py logs/controller/latest --host 192.168.37.237
  python tools/analyze_coolroom_logs.py path/to/dir --json-out report.json
  python tools/analyze_coolroom_logs.py path/to/dir --since 2025-12-20 --until 2026-01-31
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import statistics
import sys
from dataclasses import asdict, dataclass, field
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any, Iterable

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from coolroom_http import DEFAULT_HOST, PROFILE_2C, fetch_current_settings  # noqa: E402

_DATE_CSV = re.compile(r"^(\d{4}-\d{2}-\d{2})\.csv$", re.IGNORECASE)
_COMP_DETAIL = re.compile(
    r"coolroom=(?P<coolroom>[-+]?\d+(?:\.\d+)?).*?"
    r"setpoint=(?P<setpoint>[-+]?\d+(?:\.\d+)?).*?"
    r"diff=(?P<diff>[-+]?\d+(?:\.\d+)?)",
    re.IGNORECASE,
)

# Primary gate used by analyse_logs_tune_settings.py (keep in sync).
MIN_HISTORY_DAYS = 30

# Ambient high-load heuristics (WA summer / fruit picking — external can near ~40 °C).
AMBIENT_HIGH_P95_C = 30.0
AMBIENT_VERY_HIGH_P95_C = 35.0
AMBIENT_NEAR_40_MAX_C = 38.0

# Accept these daily-CSV columns for external/ambient (first non-empty wins).
_AMBIENT_KEYS = (
    "ambient_c",
    "external_c",
    "external_temp_c",
    "ambient_temp_c",
    "outside_c",
)

# WA fruit picking season: late December → end of April (season year = April year).
PICKING_SEASON_START_MONTH = 12
PICKING_SEASON_START_DAY = 20
PICKING_SEASON_END_MONTH = 4
PICKING_SEASON_END_DAY = 30


def parse_iso_date(text: str) -> date:
    """Parse YYYY-MM-DD into a date (raises ValueError)."""
    return datetime.strptime(text.strip(), "%Y-%m-%d").date()


def picking_season_bounds(today: date | None = None) -> tuple[date, date]:
    """Return (start, end) for the WA picking season relevant to *today*.

    Season for harvest ending in year H: 20 Dec (H-1) → 30 Apr H.
    During an open season, end is clipped to *today*. Outside season (1 May–
    19 Dec), returns the most recently completed season.
    """
    today = today or date.today()
    in_open = (today.month == PICKING_SEASON_START_MONTH and today.day >= PICKING_SEASON_START_DAY) or (
        today.month <= PICKING_SEASON_END_MONTH
    )
    if in_open:
        if today.month == PICKING_SEASON_START_MONTH:
            start = date(today.year, PICKING_SEASON_START_MONTH, PICKING_SEASON_START_DAY)
            return start, today
        start = date(today.year - 1, PICKING_SEASON_START_MONTH, PICKING_SEASON_START_DAY)
        season_end = date(today.year, PICKING_SEASON_END_MONTH, PICKING_SEASON_END_DAY)
        return start, min(today, season_end)
    # 1 May … 19 Dec → last completed season (Dec 20 prior year → Apr 30 this year)
    start = date(today.year - 1, PICKING_SEASON_START_MONTH, PICKING_SEASON_START_DAY)
    end = date(today.year, PICKING_SEASON_END_MONTH, PICKING_SEASON_END_DAY)
    return start, end


def resolve_preset_window(
    preset: str,
    today: date | None = None,
) -> tuple[date, date]:
    """Map schedule presets to inclusive (since, until) dates.

    Presets: ``last30``, ``picking`` (current / last WA fruit season).
    """
    today = today or date.today()
    key = preset.strip().lower().replace("_", "").replace("-", "")
    if key in ("last30", "last30days", "30d", "30day", "30days"):
        return today - timedelta(days=29), today
    if key in ("picking", "pickingseason", "season", "wafruit", "harvest"):
        return picking_season_bounds(today)
    raise ValueError(
        f"Unknown window preset {preset!r} (use last30 or picking)"
    )


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("log_dir", type=Path, help="Directory containing pulled CSV logs")
    p.add_argument(
        "--host",
        default=None,
        help=f"If set, also read live settings from the controller (default host {DEFAULT_HOST} if 'auto')",
    )
    p.add_argument("--json-out", type=Path, default=None, help="Write full report JSON here")
    p.add_argument(
        "--min-samples",
        type=int,
        default=24,
        help="Minimum non-defrost coolroom samples before recommending (default 24 ≈ 2 h)",
    )
    p.add_argument(
        "--min-days",
        type=float,
        default=0.0,
        help="If >0, require this many days of history in the selected window (events preferred) or exit 2",
    )
    p.add_argument(
        "--since",
        default=None,
        metavar="YYYY-MM-DD",
        help="Inclusive start of analysis window (filter temps + events)",
    )
    p.add_argument(
        "--until",
        default=None,
        metavar="YYYY-MM-DD",
        help="Inclusive end of analysis window",
    )
    p.add_argument(
        "--window",
        default=None,
        metavar="PRESET",
        help="Preset window: last30 | picking (WA fruit season late Dec→Apr). Overrides --since/--until.",
    )
    return p.parse_args()


def _f(row: dict[str, str], key: str) -> float | None:
    raw = (row.get(key) or "").strip()
    if raw == "":
        return None
    try:
        v = float(raw)
    except ValueError:
        return None
    if not math.isfinite(v):
        return None
    return v


def _ambient_from_row(norm: dict[str, str]) -> float | None:
    for key in _AMBIENT_KEYS:
        v = _f(norm, key)
        if v is not None:
            return v
    return None


def _b(row: dict[str, str], key: str) -> bool:
    raw = (row.get(key) or "").strip().lower()
    return raw in ("1", "true", "on", "yes")


def _date_in_window(d: date, since: date | None, until: date | None) -> bool:
    if since is not None and d < since:
        return False
    if until is not None and d > until:
        return False
    return True


def _ts_in_window(ts: datetime | None, since: date | None, until: date | None) -> bool:
    if ts is None:
        return since is None and until is None
    return _date_in_window(ts.date(), since, until)


def _window_label(since: date | None, until: date | None) -> str:
    if since is None and until is None:
        return "full log span"
    if since is not None and until is not None:
        return f"{since.isoformat()} → {until.isoformat()} (inclusive)"
    if since is not None:
        return f"{since.isoformat()} → end"
    return f"start → {until.isoformat()}"


def _prior_equal_window(
    since: date | None,
    until: date | None,
    hist_first: date | None,
    hist_last: date | None,
) -> tuple[date, date] | None:
    """Equal-length calendar window immediately before the analysis window."""
    if since is None and until is None:
        return None
    eff_until = until or hist_last
    eff_since = since or hist_first
    if eff_since is None or eff_until is None:
        return None
    if eff_until < eff_since:
        return None
    length = (eff_until - eff_since).days + 1
    prior_until = eff_since - timedelta(days=1)
    prior_since = prior_until - timedelta(days=length - 1)
    return prior_since, prior_until


def _parse_ts(text: str) -> datetime | None:
    text = (text or "").strip()
    if not text:
        return None
    # Device p4_fmt_time uses local wall clock as DD-MM-YYYY HH:MM:SS (Australia/Perth).
    # Also accept ISO-ish and uptime placeholders from early boot events.
    if text.lower().startswith("uptime"):
        return None
    for fmt in (
        "%d-%m-%Y %H:%M:%S",
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%dT%H:%M:%S",
        "%Y/%m/%d %H:%M:%S",
        "%d/%m/%Y %H:%M:%S",
    ):
        try:
            return datetime.strptime(text, fmt)
        except ValueError:
            continue
    try:
        return datetime.fromisoformat(text.replace("Z", "+00:00")).replace(tzinfo=None)
    except ValueError:
        return None


@dataclass
class TempRow:
    ts: datetime | None
    coolroom: float | None
    evap: float | None
    ambient: float | None
    setpoint: float | None
    compressor: bool
    defrost: bool
    alarm_hi: bool
    alarm_lo: bool
    probe_fault: bool


@dataclass
class Recommendation:
    key: str
    label: str
    suggested: float | None
    unit: str
    confidence: str  # high / medium / low / n/a
    rationale: str
    current: float | None = None
    profile_2c: float | None = None
    apply_safe: bool = False


@dataclass
class HistorySpan:
    """How much usable log history is present (for the ≥30-day gate)."""

    source: str  # "events" | "temp_files" | "none"
    span_days: float
    first_ts: datetime | None
    last_ts: datetime | None
    event_rows: int
    dated_temp_files: int
    detail: str

    @property
    def ok(self) -> bool:
        return self.source != "none" and self.span_days >= MIN_HISTORY_DAYS


@dataclass
class Report:
    log_dir: str
    temp_files: list[str] = field(default_factory=list)
    event_file: str | None = None
    sample_count: int = 0
    non_defrost_count: int = 0
    span_hours: float | None = None
    history: HistorySpan | None = None
    window: dict[str, Any] = field(default_factory=dict)
    ambient: dict[str, Any] = field(default_factory=dict)
    cycle_rate: dict[str, Any] = field(default_factory=dict)
    stats: dict[str, Any] = field(default_factory=dict)
    event_stats: dict[str, Any] = field(default_factory=dict)
    recommendations: list[Recommendation] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    current_settings: dict[str, float | None] = field(default_factory=dict)


def load_temp_rows(paths: Iterable[Path]) -> list[TempRow]:
    rows: list[TempRow] = []
    for path in paths:
        with path.open("r", encoding="utf-8", errors="replace", newline="") as f:
            reader = csv.DictReader(f)
            for raw in reader:
                # Accept either exact header names or case-insensitive match
                norm = {((k or "").strip().lower()): (v or "") for k, v in raw.items()}
                rows.append(
                    TempRow(
                        ts=_parse_ts(norm.get("timestamp", "")),
                        coolroom=_f(norm, "coolroom_c"),
                        evap=_f(norm, "evap_c"),
                        ambient=_ambient_from_row(norm),
                        setpoint=_f(norm, "setpoint_c"),
                        compressor=_b(norm, "compressor"),
                        defrost=_b(norm, "defrost"),
                        alarm_hi=_b(norm, "alarm_hi"),
                        alarm_lo=_b(norm, "alarm_lo"),
                        probe_fault=_b(norm, "probe_fault"),
                    )
                )
    rows.sort(key=lambda r: r.ts or datetime.min)
    return rows


def filter_temp_rows(
    rows: list[TempRow],
    since: date | None,
    until: date | None,
) -> list[TempRow]:
    if since is None and until is None:
        return rows
    return [r for r in rows if _ts_in_window(r.ts, since, until)]


def load_event_stats(
    path: Path,
    since: date | None = None,
    until: date | None = None,
) -> dict[str, Any]:
    counts: dict[str, int] = {}
    on_offs: list[tuple[datetime, str, dict[str, float]]] = []
    timestamps: list[datetime] = []
    with path.open("r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for raw in reader:
            norm = {((k or "").strip().lower()): (v or "") for k, v in raw.items()}
            ev = (norm.get("event") or "").strip().upper()
            if not ev:
                continue
            ts = _parse_ts(norm.get("timestamp", ""))
            if since is not None or until is not None:
                if not _ts_in_window(ts, since, until):
                    continue
            counts[ev] = counts.get(ev, 0) + 1
            if ts is not None:
                timestamps.append(ts)
            if ev in ("COMPRESSOR_ON", "COMPRESSOR_OFF"):
                detail = norm.get("detail") or ""
                meta: dict[str, float] = {}
                m = _COMP_DETAIL.search(detail)
                if m:
                    meta = {k: float(v) for k, v in m.groupdict().items()}
                if ts is not None:
                    on_offs.append((ts, ev, meta))

    on_offs.sort(key=lambda x: x[0])
    on_durations_min: list[float] = []
    off_durations_min: list[float] = []
    last_on: datetime | None = None
    last_off: datetime | None = None
    logged_diff: list[float] = []
    for ts, ev, meta in on_offs:
        if "diff" in meta:
            logged_diff.append(meta["diff"])
        if ev == "COMPRESSOR_ON":
            if last_off is not None:
                off_durations_min.append((ts - last_off).total_seconds() / 60.0)
            last_on = ts
        elif ev == "COMPRESSOR_OFF":
            if last_on is not None:
                on_durations_min.append((ts - last_on).total_seconds() / 60.0)
            last_off = ts

    def _pct(vals: list[float], p: float) -> float | None:
        if not vals:
            return None
        s = sorted(vals)
        if len(s) == 1:
            return s[0]
        idx = min(len(s) - 1, max(0, int(round((p / 100.0) * (len(s) - 1)))))
        return s[idx]

    first_ts = min(timestamps) if timestamps else None
    last_ts = max(timestamps) if timestamps else None
    span_days = (
        (last_ts - first_ts).total_seconds() / 86400.0
        if first_ts is not None and last_ts is not None
        else 0.0
    )
    starts = int(counts.get("COMPRESSOR_ON", 0))
    starts_per_day = (starts / span_days) if span_days > 0 else None

    return {
        "counts": counts,
        "compressor_transitions": len(on_offs),
        "compressor_starts": starts,
        "starts_per_day": starts_per_day,
        "event_rows": sum(counts.values()),
        "first_ts": first_ts.isoformat(sep=" ") if first_ts else None,
        "last_ts": last_ts.isoformat(sep=" ") if last_ts else None,
        "span_days": span_days,
        "on_duration_min": {
            "n": len(on_durations_min),
            "median": statistics.median(on_durations_min) if on_durations_min else None,
            "p10": _pct(on_durations_min, 10),
            "p90": _pct(on_durations_min, 90),
        },
        "off_duration_min": {
            "n": len(off_durations_min),
            "median": statistics.median(off_durations_min) if off_durations_min else None,
            "p10": _pct(off_durations_min, 10),
            "p90": _pct(off_durations_min, 90),
        },
        "logged_differential_c": {
            "n": len(logged_diff),
            "median": statistics.median(logged_diff) if logged_diff else None,
        },
    }


def list_log_paths(log_dir: Path) -> tuple[list[Path], Path | None]:
    """Return (dated+nodate temp CSV paths, events.csv path or None)."""
    temp_paths = sorted(
        [
            p
            for p in log_dir.iterdir()
            if p.is_file() and (_DATE_CSV.match(p.name) or p.name.lower() == "nodate.csv")
        ],
        key=lambda p: p.name,
    )
    event_path = next(
        (p for p in log_dir.iterdir() if p.is_file() and p.name.lower() == "events.csv"),
        None,
    )
    return temp_paths, event_path


def filter_temp_paths(
    temp_paths: list[Path],
    since: date | None,
    until: date | None,
) -> list[Path]:
    """Keep dated CSVs whose filename date intersects the window; always keep nodate.csv."""
    if since is None and until is None:
        return temp_paths
    out: list[Path] = []
    for p in temp_paths:
        m = _DATE_CSV.match(p.name)
        if not m:
            out.append(p)  # nodate.csv — still load; row timestamps are filtered later
            continue
        try:
            d = datetime.strptime(m.group(1), "%Y-%m-%d").date()
        except ValueError:
            out.append(p)
            continue
        if _date_in_window(d, since, until):
            out.append(p)
    return out


def measure_history(
    log_dir: Path,
    since: date | None = None,
    until: date | None = None,
) -> HistorySpan:
    """Measure usable history for the ≥30-day gate (within the selected window).

    Clear rule (documented in tools/README_LOG_TUNING.md):
      1. Prefer events.csv: span = last − first parseable timestamp **in the window**.
         If events.csv exists with ≥2 parseable timestamps in-window but span < min days,
         that is a hard fail — do not fall back to temp files to invent tweaks.
      2. If events.csv is missing or has <2 parseable timestamps in-window, fall back to
         dated YYYY-MM-DD.csv filenames intersecting the window:
         span = (latest − earliest) calendar days.
    """
    temp_paths, event_path = list_log_paths(log_dir)
    dated = []
    for p in temp_paths:
        m = _DATE_CSV.match(p.name)
        if m:
            try:
                d = datetime.strptime(m.group(1), "%Y-%m-%d").date()
            except ValueError:
                continue
            if _date_in_window(d, since, until):
                dated.append(d)

    win = _window_label(since, until)
    win_note = f" [window {win}]" if since is not None or until is not None else ""

    if event_path is not None:
        stats = load_event_stats(event_path, since=since, until=until)
        span = float(stats.get("span_days") or 0.0)
        first = stats.get("first_ts")
        last = stats.get("last_ts")
        first_dt = _parse_ts(first) if isinstance(first, str) else None
        last_dt = _parse_ts(last) if isinstance(last, str) else None
        rows = int(stats.get("event_rows") or 0)
        if first_dt is not None and last_dt is not None and rows >= 2:
            return HistorySpan(
                source="events",
                span_days=span,
                first_ts=first_dt,
                last_ts=last_dt,
                event_rows=rows,
                dated_temp_files=len(dated),
                detail=(
                    f"events.csv: {rows} rows from {first_dt.date()} → {last_dt.date()} "
                    f"({span:.1f} days){win_note}"
                ),
            )
        # events present but unusable clock in window → try temps, note the issue
        if dated:
            span_d = float((max(dated) - min(dated)).days)
            return HistorySpan(
                source="temp_files",
                span_days=span_d,
                first_ts=datetime.combine(min(dated), datetime.min.time()),
                last_ts=datetime.combine(max(dated), datetime.min.time()),
                event_rows=rows,
                dated_temp_files=len(dated),
                detail=(
                    f"events.csv present but <2 parseable timestamps in window "
                    f"(rows={rows}); using dated temp files {min(dated)} → {max(dated)} "
                    f"({span_d:.0f} calendar days){win_note}"
                ),
            )
        return HistorySpan(
            source="none",
            span_days=0.0,
            first_ts=None,
            last_ts=None,
            event_rows=rows,
            dated_temp_files=0,
            detail=f"events.csv has no usable timestamps in window and no dated temp CSVs{win_note}",
        )

    if dated:
        span_d = float((max(dated) - min(dated)).days)
        return HistorySpan(
            source="temp_files",
            span_days=span_d,
            first_ts=datetime.combine(min(dated), datetime.min.time()),
            last_ts=datetime.combine(max(dated), datetime.min.time()),
            event_rows=0,
            dated_temp_files=len(dated),
            detail=(
                f"No events.csv; dated temp files {min(dated)} → {max(dated)} "
                f"({span_d:.0f} calendar days). Event-based heuristics will be limited.{win_note}"
            ),
        )

    return HistorySpan(
        source="none",
        span_days=0.0,
        first_ts=None,
        last_ts=None,
        event_rows=0,
        dated_temp_files=0,
        detail=f"No events.csv and no dated YYYY-MM-DD.csv temperature logs in window{win_note}",
    )


def _percentile(vals: list[float], p: float) -> float | None:
    if not vals:
        return None
    s = sorted(vals)
    if len(s) == 1:
        return s[0]
    idx = min(len(s) - 1, max(0, int(round((p / 100.0) * (len(s) - 1)))))
    return s[idx]


def _clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


def _round_setting(v: float, step: float) -> float:
    return round(v / step) * step


def summarise_ambient(rows: list[TempRow]) -> dict[str, Any]:
    """Summarise ambient/external temps from daily CSV rows (no invention if missing)."""
    paired = [(r.ts, r.ambient) for r in rows if r.ambient is not None]
    if not paired:
        return {
            "present": False,
            "n": 0,
            "mean_c": None,
            "p95_c": None,
            "max_c": None,
            "trend": None,
            "high_load": False,
            "detail": (
                "No ambient/external temperature column in the selected window "
                f"(looked for {', '.join(_AMBIENT_KEYS)}). Ambient trends skipped."
            ),
        }
    vals = [v for _, v in paired]
    mean_c = statistics.mean(vals)
    p95_c = _percentile(vals, 95)
    max_c = max(vals)
    # Simple half-window trend on chronological samples with timestamps
    timed = [(ts, v) for ts, v in paired if ts is not None]
    trend = None
    if len(timed) >= 4:
        timed.sort(key=lambda x: x[0])
        mid = len(timed) // 2
        first = statistics.mean(v for _, v in timed[:mid])
        second = statistics.mean(v for _, v in timed[mid:])
        delta = second - first
        if abs(delta) < 1.0:
            trend = "stable"
        elif delta > 0:
            trend = "rising"
        else:
            trend = "falling"
    high_load = bool(
        (p95_c is not None and p95_c >= AMBIENT_HIGH_P95_C)
        or (max_c is not None and max_c >= AMBIENT_NEAR_40_MAX_C)
    )
    very_high = bool(p95_c is not None and p95_c >= AMBIENT_VERY_HIGH_P95_C) or (
        max_c is not None and max_c >= AMBIENT_NEAR_40_MAX_C
    )
    detail = (
        f"Ambient n={len(vals)}: mean {mean_c:.1f} °C, p95 {p95_c:.1f} °C, "
        f"max {max_c:.1f} °C"
        + (f", trend {trend}" if trend else "")
    )
    if high_load:
        detail += (
            " — WA summer / picking-season style load (p95≥30 °C or max near 40 °C)."
            if very_high
            else " — elevated external ambient (p95≥30 °C)."
        )
    return {
        "present": True,
        "n": len(vals),
        "mean_c": mean_c,
        "p95_c": p95_c,
        "max_c": max_c,
        "trend": trend,
        "high_load": high_load,
        "very_high": very_high,
        "detail": detail,
    }


def build_cycle_rate_report(
    event_stats: dict[str, Any] | None,
    *,
    label: str = "window",
) -> dict[str, Any]:
    """Compressor starts/day + ON/OFF duration percentiles from event stats."""
    if not event_stats:
        return {
            "label": label,
            "present": False,
            "starts": 0,
            "starts_per_day": None,
            "span_days": 0.0,
            "on_duration_min": {},
            "off_duration_min": {},
            "detail": "No events.csv cycle timing in this window.",
        }
    on = dict(event_stats.get("on_duration_min") or {})
    off = dict(event_stats.get("off_duration_min") or {})
    starts = int(event_stats.get("compressor_starts") or 0)
    span = float(event_stats.get("span_days") or 0.0)
    spd = event_stats.get("starts_per_day")
    if spd is None and span > 0:
        spd = starts / span
    detail_parts = [f"{label}: {starts} compressor starts"]
    if spd is not None:
        detail_parts.append(f"{spd:.2f}/day over {span:.1f} d")
    if on.get("n"):
        detail_parts.append(
            f"ON median/p10/p90={on.get('median'):.1f}/{on.get('p10'):.1f}/{on.get('p90'):.1f} min"
        )
    if off.get("n"):
        detail_parts.append(
            f"OFF median/p10/p90={off.get('median'):.1f}/{off.get('p10'):.1f}/{off.get('p90'):.1f} min"
        )
    return {
        "label": label,
        "present": True,
        "starts": starts,
        "starts_per_day": spd,
        "span_days": span,
        "on_duration_min": on,
        "off_duration_min": off,
        "detail": "; ".join(detail_parts),
    }


def _compare_cycle_rates(current: dict[str, Any], prior: dict[str, Any] | None) -> str | None:
    if not prior or not prior.get("present") or not current.get("present"):
        return None
    cur_spd = current.get("starts_per_day")
    pri_spd = prior.get("starts_per_day")
    if cur_spd is None or pri_spd is None or pri_spd <= 0:
        return None
    pct = ((cur_spd - pri_spd) / pri_spd) * 100.0
    if abs(pct) < 0.5:
        return (
            f"Cycle rate vs prior window: {cur_spd:.2f}/day now vs {pri_spd:.2f}/day before "
            "(≈ unchanged)."
        )
    direction = "higher" if pct > 0 else "lower"
    return (
        f"Cycle rate vs prior window: {cur_spd:.2f}/day now vs {pri_spd:.2f}/day before "
        f"({abs(pct):.0f}% {direction})."
    )


def _apply_ambient_bias(report: Report, ambient: dict[str, Any]) -> None:
    """Bias notes / rationales when external ambient is high (WA summer load)."""
    if not ambient.get("present"):
        report.notes.append(ambient.get("detail") or "Ambient column missing — not invented.")
        return
    report.notes.append(ambient["detail"])
    if not ambient.get("high_load"):
        return
    report.notes.append(
        "WA fruit-picking / summer load: watch compressor differential and cycle rate, "
        "defrost interval / ice risk, and no-cool / high-alarm nuisance as ambient climbs "
        "toward ~40 °C. Keep setpoint at product need (2.0 °C profile) — re-evaluate "
        "diff / defrost / alarms monthly or per harvest block."
    )
    # Soften high-alarm / no-cool messaging when ambient is brutal
    for rec in report.recommendations:
        if rec.key == "differential_c" and ambient.get("very_high"):
            rec.rationale += (
                " High external ambient raises load — prefer confirming cycle-rate before "
                "widening differential further."
            )
        if rec.key == "defrost_interval_min":
            rec.rationale += (
                " Hot/humid ingress during picking increases frost risk — do not lengthen "
                "interval without checking ICE_ALARM / coil condition."
            )
        if rec.key in ("alarm_high_delta_c", "alarm_persist_min", "no_cool_timeout_min"):
            rec.rationale += (
                " Summer ambient can cause brief pull-down lag after door/load — treat "
                "nuisance HI / no-cool trips cautiously before widening further."
            )
    counts = (report.event_stats or {}).get("counts") or {}
    if counts.get("NO_COOL_ALARM"):
        report.notes.append(
            "NO_COOL_ALARM present under high ambient — verify capacity/airflow before "
            "shortening No-Cool Timeout; hot weather alone is not a timeout fix."
        )


def _rec(
    key: str,
    label: str,
    suggested: float | None,
    unit: str,
    confidence: str,
    rationale: str,
    *,
    apply_safe: bool | None = None,
) -> Recommendation:
    from coolroom_http import APPLY_ALLOWLIST

    safe = (key in APPLY_ALLOWLIST) if apply_safe is None else apply_safe
    return Recommendation(
        key=key,
        label=label,
        suggested=suggested,
        unit=unit,
        confidence=confidence,
        rationale=rationale,
        profile_2c=PROFILE_2C.get(key),
        apply_safe=safe and suggested is not None,
    )


def _add_cycle_recommendations(report: Report, event_stats: dict[str, Any]) -> None:
    """Off-delay / min-run heuristics from COMPRESSOR_ON/OFF edges."""
    off = event_stats.get("off_duration_min") or {}
    on = event_stats.get("on_duration_min") or {}
    if off.get("n", 0) >= 5 and off.get("p10") is not None:
        p10_off = float(off["p10"])
        if p10_off < 4.0:
            suggested_lock = _clamp(_round_setting(max(3.0, p10_off), 0.5), 0.0, 10.0)
            why = (
                f"Short compressor OFF gaps (p10) ≈ {p10_off:.1f} min from events — "
                "raise Off-Delay toward that floor to reduce short-cycling (never below manufacturer min)."
            )
            conf = "medium"
        else:
            suggested_lock = 3.0
            why = (
                f"OFF gaps (p10) ≈ {p10_off:.1f} min — room already rests longer than the default "
                "3 min lockout; leave Off-Delay at 3 unless the compressor maker requires more."
            )
            conf = "medium"
        report.recommendations.append(
            _rec("off_delay_min", "Compressor Off-Delay (min)", suggested_lock, "min", conf, why)
        )
    else:
        report.recommendations.append(
            _rec(
                "off_delay_min",
                "Compressor Off-Delay (min)",
                3.0,
                "min",
                "low",
                "Not enough COMPRESSOR_ON/OFF edges to measure restart gaps — leave at default 3 min.",
            )
        )

    if on.get("n", 0) >= 5 and on.get("p10") is not None:
        p10_on = float(on["p10"])
        if p10_on < 3.0:
            suggested_min_run = _clamp(_round_setting(max(2.0, p10_on), 0.5), 0.0, 30.0)
            why = (
                f"Very short ON runs (p10) ≈ {p10_on:.1f} min — keep Min Run near that so brief "
                "pulses still get a useful ON window."
            )
        else:
            suggested_min_run = 2.0
            why = (
                f"ON runs (p10) ≈ {p10_on:.1f} min — longer than the default Min Run; leave at 2 min "
                "so cut-out is not delayed unnecessarily."
            )
        report.recommendations.append(
            _rec(
                "min_run_min",
                "Compressor Min Run Time (min)",
                suggested_min_run,
                "min",
                "medium",
                why,
            )
        )
    else:
        report.recommendations.append(
            _rec(
                "min_run_min",
                "Compressor Min Run Time (min)",
                2.0,
                "min",
                "low",
                "Insufficient ON-duration samples — leave at default 2 min.",
            )
        )


def _add_event_pattern_recommendations(report: Report, event_stats: dict[str, Any]) -> None:
    """Door / defrost / alarm / probe patterns from events.csv counts."""
    counts: dict[str, int] = dict(event_stats.get("counts") or {})
    span_days = float(event_stats.get("span_days") or 0.0) or None
    per_day = (lambda n: (n / span_days) if span_days and span_days > 0 else None)

    alarm_hi = counts.get("ALARM_HI", 0)
    alarm_lo = counts.get("ALARM_LO", 0)
    door = counts.get("DOOR_ALARM", 0)
    defrost_starts = counts.get("DEFROST_START", 0)
    ice = counts.get("ICE_ALARM", 0)
    probe = counts.get("PROBE_FAULT", 0)
    temp_offline = counts.get("TEMP_BOARD_OFFLINE", 0)
    no_cool = counts.get("NO_COOL_ALARM", 0)

    hi_pd = per_day(alarm_hi)
    lo_pd = per_day(alarm_lo)
    door_pd = per_day(door)
    defrost_pd = per_day(defrost_starts)
    ice_pd = per_day(ice)

    # Frequent HI/LO alarms → widen delta slightly and/or lengthen persist
    if hi_pd is not None and hi_pd >= 0.5:
        # Toward profile 2.5 or a bit wider
        sug = 3.0 if hi_pd >= 1.0 else 2.5
        report.recommendations.append(
            _rec(
                "alarm_high_delta_c",
                "High Temp Alarm Delta (°C)",
                sug,
                "°C",
                "medium",
                f"ALARM_HI ≈ {hi_pd:.2f}/day over {span_days:.0f} d ({alarm_hi} total). "
                "Widen delta modestly and/or raise Persist before chasing plant faults.",
            )
        )
        report.recommendations.append(
            _rec(
                "alarm_persist_min",
                "Alarm Persist Time (min)",
                8.0 if hi_pd >= 1.0 else 5.0,
                "min",
                "medium",
                "Frequent high alarms — lengthen persist so door/load blips need more dwell.",
            )
        )
    if lo_pd is not None and lo_pd >= 0.3:
        sug = 2.5 if lo_pd >= 0.8 else 2.0
        report.recommendations.append(
            _rec(
                "alarm_low_delta_c",
                "Low Temp Alarm Delta (°C)",
                sug,
                "°C",
                "medium",
                f"ALARM_LO ≈ {lo_pd:.2f}/day ({alarm_lo} total). "
                "Slightly wider low delta reduces nuisance freeze-guard trips near setpoint.",
            )
        )

    # Door-open storms
    if door_pd is not None and door_pd >= 0.5:
        # Raise delay toward 600 s if storms; leave at 300 if mild
        sug = 600.0 if door_pd >= 1.5 else 420.0
        report.recommendations.append(
            _rec(
                "door_alarm_delay_s",
                "Door Alarm Delay (s)",
                sug,
                "s",
                "medium",
                f"DOOR_ALARM ≈ {door_pd:.2f}/day ({door} total). "
                "Lengthen delay for busy loading; do not disable the door sensor. "
                "Hold-compressor stays operator opt-in (not auto-applied).",
            )
        )
    elif door == 0 and span_days and span_days >= MIN_HISTORY_DAYS:
        report.recommendations.append(
            _rec(
                "door_alarm_delay_s",
                "Door Alarm Delay (s)",
                300.0,
                "s",
                "low",
                "No DOOR_ALARM events in the window — leave delay at 300 s (2 °C profile).",
            )
        )

    # Defrost cadence vs ice
    if defrost_pd is not None:
        if ice_pd is not None and ice_pd >= 0.2:
            # Ice despite defrosts → shorter interval
            sug = 300.0 if defrost_pd < 5 else 360.0
            report.recommendations.append(
                _rec(
                    "defrost_interval_min",
                    "Defrost Interval (min)",
                    sug,
                    "min",
                    "medium",
                    f"ICE_ALARM ≈ {ice_pd:.2f}/day with DEFROST_START ≈ {defrost_pd:.2f}/day. "
                    "Shorten fixed interval and confirm Smart/Dew-Point triggers are on "
                    "(see recommended_settings_2c.html) — those switches are not auto-applied.",
                )
            )
        elif defrost_pd > 6.0:
            report.recommendations.append(
                _rec(
                    "defrost_interval_min",
                    "Defrost Interval (min)",
                    480.0,
                    "min",
                    "low",
                    f"Very frequent DEFROST_START ≈ {defrost_pd:.1f}/day — interval or smart "
                    "triggers may be aggressive; lengthen toward 8 h if coil stays clear.",
                )
            )
        elif defrost_pd < 2.0 and span_days and span_days >= 14:
            report.recommendations.append(
                _rec(
                    "defrost_interval_min",
                    "Defrost Interval (min)",
                    360.0,
                    "min",
                    "low",
                    f"Only ≈ {defrost_pd:.1f} DEFROST_START/day — 2 °C food profile uses 360 min; "
                    "shorten further only if frost/ice appears between cycles.",
                )
            )

    if probe or temp_offline:
        report.notes.append(
            f"Probe/board faults logged: PROBE_FAULT={probe}, TEMP_BOARD_OFFLINE={temp_offline}. "
            "Check RS485 / RTD wiring — do **not** auto-disable probes; leave enables as fitted."
        )
    if no_cool:
        report.notes.append(
            f"Saw {no_cool} NO_COOL_ALARM event(s) — usually plant/airflow/refrigerant, "
            "not a setpoint tweak. Inspect before shortening No-Cool Timeout."
        )


def analyze(
    rows: list[TempRow],
    event_stats: dict[str, Any] | None,
    min_samples: int,
    *,
    prior_event_stats: dict[str, Any] | None = None,
) -> Report:
    report = Report(log_dir="")
    report.notes.append(
        "Temperature CSV is sampled every 5 minutes — short compressor cycles are under-resolved; "
        "prefer events.csv for on/off timing when present."
    )
    report.notes.append(
        "Recommendations are heuristics for holding the logged setpoint, cross-checked against the "
        "2 °C food profile (Quick Start §4 / recommended_settings_2c.html). Confirm before applying."
    )

    ambient = summarise_ambient(rows)
    report.ambient = ambient
    cycle = build_cycle_rate_report(event_stats, label="analysis window")
    prior_cycle = None
    if prior_event_stats is not None:
        prior_cycle = build_cycle_rate_report(prior_event_stats, label="prior window")
        cmp = _compare_cycle_rates(cycle, prior_cycle)
        if cmp:
            cycle["vs_prior"] = cmp
            report.notes.append(cmp)
    cycle["prior"] = prior_cycle
    report.cycle_rate = cycle

    usable = [r for r in rows if r.coolroom is not None]
    report.sample_count = len(usable)
    non_defrost = [r for r in usable if not r.defrost and not r.probe_fault]
    report.non_defrost_count = len(non_defrost)

    ts_ok = [r.ts for r in rows if r.ts is not None]
    if len(ts_ok) >= 2:
        report.span_hours = (max(ts_ok) - min(ts_ok)).total_seconds() / 3600.0

    if rows:
        pf = sum(1 for r in rows if r.probe_fault) / len(rows)
        empty_cool = sum(1 for r in rows if r.coolroom is None) / len(rows)
        report.stats["probe_fault_fraction"] = pf
        report.stats["empty_coolroom_fraction"] = empty_cool
        if empty_cool > 0.5:
            report.notes.append(
                f"{empty_cool:.0%} of temperature rows have empty coolroom_c "
                f"(probe_fault fraction {pf:.0%}) — typical when the RTD Modbus board is offline. "
                "Recommendations need live Probe 1 samples."
            )

    if event_stats:
        report.event_stats = event_stats
        off = event_stats.get("off_duration_min") or {}
        on = event_stats.get("on_duration_min") or {}
        if off.get("n", 0) >= 5 or on.get("n", 0) >= 5:
            report.notes.append(
                "Compressor on/off timing below is from events.csv even though coolroom samples "
                "may be missing or sparse."
            )

    if len(non_defrost) < min_samples:
        report.notes.append(
            f"Only {len(non_defrost)} non-defrost samples with a coolroom reading "
            f"(need ≥ {min_samples}). Pull more days once RS485 RTD data is present."
        )
        if event_stats:
            _add_cycle_recommendations(report, event_stats)
            _add_event_pattern_recommendations(report, event_stats)
        _dedupe_recommendations(report)
        _apply_ambient_bias(report, ambient)
        return report

    cools = [r.coolroom for r in non_defrost if r.coolroom is not None]
    sets = [r.setpoint for r in non_defrost if r.setpoint is not None]
    errs = [
        r.coolroom - r.setpoint
        for r in non_defrost
        if r.coolroom is not None and r.setpoint is not None
    ]
    setpoint = statistics.median(sets) if sets else None
    mean_err = statistics.mean(errs) if errs else None
    abs_errs = [abs(e) for e in errs] if errs else []
    p5 = _percentile(cools, 5)
    p95 = _percentile(cools, 95)
    swing = (p95 - p5) if (p5 is not None and p95 is not None) else None

    duty = None
    if non_defrost:
        duty = sum(1 for r in non_defrost if r.compressor) / len(non_defrost)

    alarm_hi_frac = sum(1 for r in non_defrost if r.alarm_hi) / len(non_defrost)
    alarm_lo_frac = sum(1 for r in non_defrost if r.alarm_lo) / len(non_defrost)
    defrost_frac = sum(1 for r in usable if r.defrost) / max(1, len(usable))

    hi_excursions = [e for e in errs if e > 0]
    lo_excursions = [-e for e in errs if e < 0]
    p95_hi = _percentile(hi_excursions, 95) if hi_excursions else 0.0
    p95_lo = _percentile(lo_excursions, 95) if lo_excursions else 0.0

    report.stats.update(
        {
            "setpoint_median_c": setpoint,
            "coolroom_mean_c": statistics.mean(cools),
            "coolroom_stdev_c": statistics.pstdev(cools) if len(cools) > 1 else 0.0,
            "mean_error_c": mean_err,
            "mae_c": statistics.mean(abs_errs) if abs_errs else None,
            "coolroom_p5_c": p5,
            "coolroom_p95_c": p95,
            "swing_p5_p95_c": swing,
            "compressor_duty": duty,
            "alarm_hi_fraction": alarm_hi_frac,
            "alarm_lo_fraction": alarm_lo_frac,
            "defrost_fraction": defrost_frac,
            "p95_high_excursion_c": p95_hi,
            "p95_low_excursion_c": p95_lo,
        }
    )
    if event_stats:
        report.event_stats = event_stats

    # ── Recommendations ──────────────────────────────────────────────────
    if setpoint is not None:
        report.recommendations.append(
            _rec(
                "setpoint_c",
                "Setpoint (°C)",
                _round_setting(setpoint, 0.1),
                "°C",
                "high",
                "Median logged setpoint — analyzer holds this target; change only if product needs differ.",
                apply_safe=False,
            )
        )

    if swing is not None:
        suggested_diff = _clamp(_round_setting(max(0.5, min(swing, 2.0)), 0.1), 0.5, 10.0)
        # Prefer staying near 2 °C profile (1.0) when swing is modest
        if swing <= 1.5:
            suggested_diff = 1.0
        conf = "medium" if report.span_hours and report.span_hours >= 12 else "low"
        report.recommendations.append(
            _rec(
                "differential_c",
                "Compressor Differential (°C)",
                suggested_diff,
                "°C",
                conf,
                (
                    f"Non-defrost coolroom p5–p95 swing is {swing:.2f} °C. "
                    "A differential near that band reduces hunting; 2 °C profile uses 1.0 °C."
                ),
            )
        )

    if event_stats:
        _add_cycle_recommendations(report, event_stats)
    else:
        report.recommendations.append(
            _rec(
                "off_delay_min",
                "Compressor Off-Delay (min)",
                3.0,
                "min",
                "low",
                "No events.csv — leave Off-Delay at default 3 min.",
            )
        )
        report.recommendations.append(
            _rec(
                "min_run_min",
                "Compressor Min Run Time (min)",
                2.0,
                "min",
                "low",
                "No events.csv — leave Min Run at default 2 min.",
            )
        )

    # Alarm deltas from temp excursions (may be overridden by event-rate heuristics below)
    hi_delta = _clamp(_round_setting(max(1.0, (p95_hi or 0.0) + 0.5), 0.1), 0.5, 20.0)
    lo_delta = _clamp(_round_setting(max(1.0, (p95_lo or 0.0) + 0.5), 0.1), 0.5, 20.0)
    hi_conf = "medium" if alarm_hi_frac < 0.05 else "low"
    lo_conf = "medium" if alarm_lo_frac < 0.05 else "low"
    if alarm_hi_frac >= 0.05:
        hi_delta = _clamp(_round_setting(max(hi_delta, (p95_hi or 0.0) + 1.0), 0.1), 0.5, 20.0)
        hi_conf = "medium"
    if alarm_lo_frac >= 0.05:
        lo_delta = _clamp(_round_setting(max(lo_delta, (p95_lo or 0.0) + 1.0), 0.1), 0.5, 20.0)
        lo_conf = "medium"
    # Nudge toward 2 °C profile when excursions are mild
    if (p95_hi or 0) <= 2.0 and alarm_hi_frac < 0.02:
        hi_delta = 2.5
    if (p95_lo or 0) <= 1.5 and alarm_lo_frac < 0.02:
        lo_delta = 2.0

    report.recommendations.append(
        _rec(
            "alarm_high_delta_c",
            "High Temp Alarm Delta (°C)",
            hi_delta,
            "°C",
            hi_conf,
            (
                f"p95 high excursion above setpoint ≈ {p95_hi:.2f} °C "
                f"(alarm_hi fraction {alarm_hi_frac:.1%} of samples). "
                "2 °C profile uses 2.5 °C."
            ),
        )
    )
    report.recommendations.append(
        _rec(
            "alarm_low_delta_c",
            "Low Temp Alarm Delta (°C)",
            lo_delta,
            "°C",
            lo_conf,
            (
                f"p95 low excursion below setpoint ≈ {p95_lo:.2f} °C "
                f"(alarm_lo fraction {alarm_lo_frac:.1%}). 2 °C profile uses 2.0 °C."
            ),
        )
    )

    if alarm_hi_frac >= 0.02 or alarm_lo_frac >= 0.02:
        report.recommendations.append(
            _rec(
                "alarm_persist_min",
                "Alarm Persist Time (min)",
                8.0,
                "min",
                "low",
                "Alarms appear often in the temp log — lengthen persist before widening deltas further.",
            )
        )
    else:
        report.recommendations.append(
            _rec(
                "alarm_persist_min",
                "Alarm Persist Time (min)",
                5.0,
                "min",
                "low",
                "Alarm fraction is low — default 5 min persist is fine (2 °C profile).",
            )
        )

    if report.span_hours and report.span_hours >= 24 and defrost_frac > 0:
        report.recommendations.append(
            _rec(
                "defrost_interval_min",
                "Defrost Interval (min)",
                360.0,
                "min",
                "low",
                (
                    f"Defrost active in {defrost_frac:.1%} of samples over "
                    f"{report.span_hours:.1f} h. Start from 360 min (2 °C profile); "
                    "tune from frost/ice events and Smart/Dew-Point switches."
                ),
            )
        )

    if mean_err is not None and abs(mean_err) > 0.8:
        report.notes.append(
            f"Mean coolroom−setpoint error is {mean_err:+.2f} °C. "
            "Sustained warm bias → check cooling capacity / door / defrost; "
            "cold bias → differential or min-run may be holding too long, or setpoint vs product mismatch."
        )

    if duty is not None and duty > 0.85:
        report.notes.append(
            f"Compressor duty ≈ {duty:.0%} outside defrost — plant may be undersized for this load/setpoint."
        )
    if duty is not None and duty < 0.05 and (swing or 0) < 0.3:
        report.notes.append(
            "Very low compressor duty and tiny swing — either the room is unloaded / offline RTD, "
            "or logs are mostly from a bench without a live coolroom."
        )

    if event_stats:
        _add_event_pattern_recommendations(report, event_stats)

    _dedupe_recommendations(report)
    _apply_ambient_bias(report, ambient)
    return report


def _dedupe_recommendations(report: Report) -> None:
    """Keep the last recommendation per key (event heuristics run after temp and win)."""
    by_key: dict[str, Recommendation] = {}
    order: list[str] = []
    for rec in report.recommendations:
        if rec.key not in by_key:
            order.append(rec.key)
        by_key[rec.key] = rec
    report.recommendations = [by_key[k] for k in order]

def attach_current(report: Report, current: dict[str, float | None]) -> None:
    report.current_settings = current
    for rec in report.recommendations:
        if rec.key in current:
            rec.current = current[rec.key]


def print_report(report: Report) -> None:
    print(f"Log dir: {report.log_dir}")
    if report.window:
        print(f"Window: {report.window.get('label', _window_label(None, None))}")
    print(f"Temp files: {', '.join(report.temp_files) or '(none)'}")
    print(f"Events: {report.event_file or '(none)'}")
    if report.history is not None:
        print(f"History: {report.history.detail} [source={report.history.source}]")
    print(
        f"Samples: {report.sample_count} total, {report.non_defrost_count} non-defrost"
        + (f", span {report.span_hours:.1f} h" if report.span_hours is not None else "")
    )
    if report.ambient:
        amb = report.ambient
        if amb.get("present"):
            print("\nAmbient (external):")
            print(f"  {amb.get('detail')}")
        else:
            print(f"\nAmbient: {amb.get('detail', 'not present')}")

    if report.cycle_rate and report.cycle_rate.get("present"):
        cr = report.cycle_rate
        print("\nCycle rate:")
        print(f"  {cr.get('detail')}")
        prior = cr.get("prior") or {}
        if prior.get("present"):
            print(f"  prior: {prior.get('detail')}")
        if cr.get("vs_prior"):
            print(f"  {cr['vs_prior']}")

    if report.stats and report.stats.get("coolroom_mean_c") is not None:
        s = report.stats
        print("\nHold quality (non-defrost):")
        print(f"  setpoint median : {s.get('setpoint_median_c')}")
        print(f"  coolroom mean   : {s['coolroom_mean_c']:.2f} °C")
        if s.get("mean_error_c") is not None:
            print(f"  mean error      : {s['mean_error_c']:+.2f} °C")
        if s.get("mae_c") is not None:
            print(f"  MAE             : {s['mae_c']:.2f} °C")
        if s.get("swing_p5_p95_c") is not None:
            print(f"  p5…p95 swing    : {s['swing_p5_p95_c']:.2f} °C")
        if s.get("compressor_duty") is not None:
            print(f"  compressor duty : {s['compressor_duty']:.0%}")
        print(
            f"  alarm_hi / lo   : {s.get('alarm_hi_fraction', 0):.1%} / "
            f"{s.get('alarm_lo_fraction', 0):.1%}"
        )
    elif report.stats.get("probe_fault_fraction") is not None:
        print(
            f"\nTemp log coverage: empty_coolroom={report.stats.get('empty_coolroom_fraction', 0):.0%} "
            f"probe_fault={report.stats.get('probe_fault_fraction', 0):.0%}"
        )

    if report.event_stats:
        es = report.event_stats
        print("\nEvent timing:")
        print(f"  transitions: {es.get('compressor_transitions')}")
        if es.get("span_days") is not None:
            print(f"  event span : {es.get('span_days'):.1f} days")
        if es.get("starts_per_day") is not None:
            print(f"  starts/day : {es.get('starts_per_day'):.2f}")
        od, fd = es.get("on_duration_min") or {}, es.get("off_duration_min") or {}
        if od.get("n"):
            print(
                f"  ON  min median/p10/p90: "
                f"{od.get('median'):.1f} / {od.get('p10'):.1f} / {od.get('p90'):.1f} ({od.get('n')} runs)"
            )
        if fd.get("n"):
            print(
                f"  OFF min median/p10/p90: "
                f"{fd.get('median'):.1f} / {fd.get('p10'):.1f} / {fd.get('p90'):.1f} ({fd.get('n')} gaps)"
            )
        interesting = {
            k: v
            for k, v in (es.get("counts") or {}).items()
            if k.endswith("_ALARM")
            or k
            in (
                "PROBE_FAULT",
                "DEFROST_START",
                "NO_COOL_ALARM",
                "ICE_ALARM",
                "DOOR_ALARM",
                "TEMP_BOARD_OFFLINE",
            )
        }
        if interesting:
            print(f"  notable counts: {interesting}")

    if report.recommendations:
        print("\nSuggested settings (review before applying):")
        print(
            f"  {'Setting':32s}  {'Suggest':>8s}  {'Current':>8s}  {'2°C':>6s}  "
            f"{'Conf':6s}  Apply  Why"
        )
        for rec in report.recommendations:
            sug = "—" if rec.suggested is None else f"{rec.suggested:g}"
            cur = "—" if rec.current is None else f"{rec.current:g}"
            p2 = "—" if rec.profile_2c is None else f"{rec.profile_2c:g}"
            ap = "yes" if rec.apply_safe else "no"
            print(
                f"  {rec.label:32s}  {sug:>8s}  {cur:>8s}  {p2:>6s}  "
                f"{rec.confidence:6s}  {ap:5s}  {rec.rationale}"
            )

    if report.notes:
        print("\nNotes:")
        for n in report.notes:
            print(f"  • {n}")


def report_to_dict(report: Report) -> dict[str, Any]:
    hist = None
    if report.history is not None:
        hist = {
            "source": report.history.source,
            "span_days": report.history.span_days,
            "first_ts": report.history.first_ts.isoformat(sep=" ") if report.history.first_ts else None,
            "last_ts": report.history.last_ts.isoformat(sep=" ") if report.history.last_ts else None,
            "event_rows": report.history.event_rows,
            "dated_temp_files": report.history.dated_temp_files,
            "detail": report.history.detail,
        }
    return {
        "log_dir": report.log_dir,
        "temp_files": report.temp_files,
        "event_file": report.event_file,
        "sample_count": report.sample_count,
        "non_defrost_count": report.non_defrost_count,
        "span_hours": report.span_hours,
        "window": report.window,
        "history": hist,
        "ambient": report.ambient,
        "cycle_rate": report.cycle_rate,
        "stats": report.stats,
        "event_stats": report.event_stats,
        "notes": report.notes,
        "current_settings": report.current_settings,
        "recommendations": [asdict(r) for r in report.recommendations],
    }


def build_report(
    log_dir: Path,
    min_samples: int = 24,
    since: date | None = None,
    until: date | None = None,
) -> Report:
    """Load a pulled log directory and run analysis (no host fetch)."""
    if not log_dir.is_dir():
        raise FileNotFoundError(f"not a directory: {log_dir}")
    temp_paths_all, event_path = list_log_paths(log_dir)
    temp_paths = filter_temp_paths(temp_paths_all, since, until)
    history = measure_history(log_dir, since=since, until=until)
    rows = filter_temp_rows(load_temp_rows(temp_paths), since, until)
    event_stats = load_event_stats(event_path, since=since, until=until) if event_path else None

    prior_event_stats = None
    hist_first = history.first_ts.date() if history.first_ts else None
    hist_last = history.last_ts.date() if history.last_ts else None
    # For prior bounds, prefer configured window; else use measured history ends.
    full_hist = measure_history(log_dir) if (since is not None or until is not None) else history
    full_first = full_hist.first_ts.date() if full_hist.first_ts else hist_first
    full_last = full_hist.last_ts.date() if full_hist.last_ts else hist_last
    prior_bounds = _prior_equal_window(since, until, full_first, full_last)
    if prior_bounds is not None and event_path is not None:
        p_since, p_until = prior_bounds
        # Only compare if prior intersects available history
        if full_first is not None and p_until >= full_first:
            clip_since = max(p_since, full_first)
            prior_event_stats = load_event_stats(event_path, since=clip_since, until=p_until)
            if int(prior_event_stats.get("event_rows") or 0) < 2:
                prior_event_stats = None

    report = analyze(
        rows,
        event_stats,
        min_samples=min_samples,
        prior_event_stats=prior_event_stats,
    )
    report.log_dir = str(log_dir)
    report.temp_files = [p.name for p in temp_paths]
    report.event_file = event_path.name if event_path else None
    report.history = history
    report.window = {
        "since": since.isoformat() if since else None,
        "until": until.isoformat() if until else None,
        "label": _window_label(since, until),
        "prior": (
            {"since": prior_bounds[0].isoformat(), "until": prior_bounds[1].isoformat()}
            if prior_bounds
            else None
        ),
    }
    return report


def main() -> int:
    args = _parse_args()
    since: date | None = None
    until: date | None = None
    if args.window:
        try:
            since, until = resolve_preset_window(args.window)
        except ValueError as e:
            print(f"ERROR: {e}", file=sys.stderr)
            return 1
    else:
        try:
            if args.since:
                since = parse_iso_date(args.since)
            if args.until:
                until = parse_iso_date(args.until)
        except ValueError as e:
            print(f"ERROR: bad date ({e})", file=sys.stderr)
            return 1
    if since is not None and until is not None and until < since:
        print("ERROR: --until must be ≥ --since", file=sys.stderr)
        return 1

    log_dir = args.log_dir
    try:
        report = build_report(log_dir, min_samples=args.min_samples, since=since, until=until)
    except FileNotFoundError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if args.min_days and args.min_days > 0:
        hist = report.history
        if hist is None or hist.span_days < args.min_days:
            detail = hist.detail if hist else "no history measured"
            print(
                f"ERROR: need ≥ {args.min_days:g} days of usable log history in the selected window; "
                f"found {hist.span_days if hist else 0:.1f} days ({detail}).",
                file=sys.stderr,
            )
            print(
                "Pull more SD logs (events.csv preferred), widen --since/--until, or wait until the "
                f"card has ≥ {args.min_days:g} days in-window before recommending tweaks.",
                file=sys.stderr,
            )
            return 2

    host = args.host
    if host == "auto":
        host = DEFAULT_HOST
    if host:
        try:
            current = fetch_current_settings(host)
            attach_current(report, current)
            print(f"(Live settings from {host})\n")
        except Exception as e:
            print(f"WARNING: could not read live settings from {host}: {e}\n", file=sys.stderr)

    print_report(report)

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report_to_dict(report), indent=2) + "\n", encoding="utf-8")
        print(f"\nJSON report → {args.json_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
