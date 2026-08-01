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
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from coolroom_http import DEFAULT_HOST, fetch_current_settings  # noqa: E402

_DATE_CSV = re.compile(r"^\d{4}-\d{2}-\d{2}\.csv$", re.IGNORECASE)
_COMP_DETAIL = re.compile(
    r"coolroom=(?P<coolroom>[-+]?\d+(?:\.\d+)?).*?"
    r"setpoint=(?P<setpoint>[-+]?\d+(?:\.\d+)?).*?"
    r"diff=(?P<diff>[-+]?\d+(?:\.\d+)?)",
    re.IGNORECASE,
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


def _b(row: dict[str, str], key: str) -> bool:
    raw = (row.get(key) or "").strip().lower()
    return raw in ("1", "true", "on", "yes")


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


@dataclass
class Report:
    log_dir: str
    temp_files: list[str] = field(default_factory=list)
    event_file: str | None = None
    sample_count: int = 0
    non_defrost_count: int = 0
    span_hours: float | None = None
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
                        ambient=_f(norm, "ambient_c"),
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


def load_event_stats(path: Path) -> dict[str, Any]:
    counts: dict[str, int] = {}
    on_offs: list[tuple[datetime, str, dict[str, float]]] = []
    with path.open("r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for raw in reader:
            norm = {((k or "").strip().lower()): (v or "") for k, v in raw.items()}
            ev = (norm.get("event") or "").strip().upper()
            if not ev:
                continue
            counts[ev] = counts.get(ev, 0) + 1
            if ev in ("COMPRESSOR_ON", "COMPRESSOR_OFF"):
                ts = _parse_ts(norm.get("timestamp", ""))
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

    return {
        "counts": counts,
        "compressor_transitions": len(on_offs),
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
            Recommendation(
                key="off_delay_min",
                label="Compressor Off-Delay (min)",
                suggested=suggested_lock,
                unit="min",
                confidence=conf,
                rationale=why,
            )
        )
    else:
        report.recommendations.append(
            Recommendation(
                key="off_delay_min",
                label="Compressor Off-Delay (min)",
                suggested=3.0,
                unit="min",
                confidence="low",
                rationale="Not enough COMPRESSOR_ON/OFF edges to measure restart gaps — leave at default 3 min.",
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
            Recommendation(
                key="min_run_min",
                label="Compressor Min Run Time (min)",
                suggested=suggested_min_run,
                unit="min",
                confidence="medium",
                rationale=why,
            )
        )
    else:
        report.recommendations.append(
            Recommendation(
                key="min_run_min",
                label="Compressor Min Run Time (min)",
                suggested=2.0,
                unit="min",
                confidence="low",
                rationale="Insufficient ON-duration samples — leave at default 2 min.",
            )
        )


def analyze(rows: list[TempRow], event_stats: dict[str, Any] | None, min_samples: int) -> Report:
    report = Report(log_dir="")
    report.notes.append(
        "Temperature CSV is sampled every 5 minutes — short compressor cycles are under-resolved; "
        "prefer events.csv for on/off timing when present."
    )
    report.notes.append(
        "Recommendations are heuristics for holding the logged setpoint, not product-science "
        "optima. Confirm against USER_MANUAL.md / QUICK_START_GUIDE.md before changing plant settings."
    )

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
        # Still emit lockout/min-run heuristics from events even without temps.
        off = event_stats.get("off_duration_min") or {}
        on = event_stats.get("on_duration_min") or {}
        if off.get("n", 0) >= 5 or on.get("n", 0) >= 5:
            report.notes.append(
                "Compressor on/off timing below is from events.csv even though coolroom samples "
                "are missing or sparse."
            )

    if len(non_defrost) < min_samples:
        report.notes.append(
            f"Only {len(non_defrost)} non-defrost samples with a coolroom reading "
            f"(need ≥ {min_samples}). Pull more days once RS485 RTD data is present."
        )
        # Still recommend off-delay/min-run from events if we can.
        if event_stats:
            _add_cycle_recommendations(report, event_stats)
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

    # Peak excursions relative to setpoint (for alarm deltas)
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
            Recommendation(
                key="setpoint_c",
                label="Setpoint (°C)",
                suggested=_round_setting(setpoint, 0.1),
                unit="°C",
                confidence="high",
                rationale="Median logged setpoint — analyzer holds this target; change only if product needs differ.",
            )
        )

    if swing is not None:
        # Symmetric hysteresis band ≈ observed swing; clamp to firmware range 0.5–10
        suggested_diff = _clamp(_round_setting(max(0.5, swing), 0.1), 0.5, 10.0)
        conf = "medium" if report.span_hours and report.span_hours >= 12 else "low"
        report.recommendations.append(
            Recommendation(
                key="differential_c",
                label="Compressor Differential (°C)",
                suggested=suggested_diff,
                unit="°C",
                confidence=conf,
                rationale=(
                    f"Non-defrost coolroom p5–p95 swing is {swing:.2f} °C. "
                    "A differential near that band reduces hunting; widen further if starts are still frequent."
                ),
            )
        )

    if event_stats:
        _add_cycle_recommendations(report, event_stats)
    else:
        report.recommendations.append(
            Recommendation(
                key="off_delay_min",
                label="Compressor Off-Delay (min)",
                suggested=3.0,
                unit="min",
                confidence="low",
                rationale="No events.csv — leave Off-Delay at default 3 min.",
            )
        )
        report.recommendations.append(
            Recommendation(
                key="min_run_min",
                label="Compressor Min Run Time (min)",
                suggested=2.0,
                unit="min",
                confidence="low",
                rationale="No events.csv — leave Min Run at default 2 min.",
            )
        )

    # Alarm deltas: above observed p95 excursion + small margin, within firmware ranges
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

    report.recommendations.append(
        Recommendation(
            key="alarm_high_delta_c",
            label="High Temp Alarm Delta (°C)",
            suggested=hi_delta,
            unit="°C",
            confidence=hi_conf,
            rationale=(
                f"p95 high excursion above setpoint ≈ {p95_hi:.2f} °C "
                f"(alarm_hi fraction {alarm_hi_frac:.1%} of samples). "
                "Delta should sit above normal swing so door/load blips need Persist Time, not a wider band alone."
            ),
        )
    )
    report.recommendations.append(
        Recommendation(
            key="alarm_low_delta_c",
            label="Low Temp Alarm Delta (°C)",
            suggested=lo_delta,
            unit="°C",
            confidence=lo_conf,
            rationale=(
                f"p95 low excursion below setpoint ≈ {p95_lo:.2f} °C "
                f"(alarm_lo fraction {alarm_lo_frac:.1%})."
            ),
        )
    )

    persist = 5.0
    if alarm_hi_frac >= 0.02 or alarm_lo_frac >= 0.02:
        persist = 8.0
        report.recommendations.append(
            Recommendation(
                key="alarm_persist_min",
                label="Alarm Persist Time (min)",
                suggested=persist,
                unit="min",
                confidence="low",
                rationale="Alarms appear often in the temp log — lengthen persist before widening deltas further.",
            )
        )
    else:
        report.recommendations.append(
            Recommendation(
                key="alarm_persist_min",
                label="Alarm Persist Time (min)",
                suggested=5.0,
                unit="min",
                confidence="low",
                rationale="Alarm fraction is low — default 5 min persist is fine.",
            )
        )

    # Defrost interval hint from defrost fraction (very rough)
    if report.span_hours and report.span_hours >= 24 and defrost_frac > 0:
        # Rough: if defrost is active ~duration/interval of wall time
        report.recommendations.append(
            Recommendation(
                key="defrost_interval_min",
                label="Defrost Interval (min)",
                suggested=None,
                unit="min",
                confidence="n/a",
                rationale=(
                    f"Defrost active in {defrost_frac:.1%} of samples over "
                    f"{report.span_hours:.1f} h. Tune interval from frost/ice events and coil behaviour, "
                    "not from this fraction alone — enable dew-point/smart triggers if humidity is high."
                ),
            )
        )

    no_cool_count = ((event_stats or {}).get("counts") or {}).get("NO_COOL_ALARM", 0)
    if no_cool_count:
        report.notes.append(
            f"Saw {no_cool_count} NO_COOL_ALARM event(s) — that usually means plant/airflow/refrigerant, "
            "not a setpoint tweak. Inspect before shortening No-Cool Timeout."
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

    return report


def attach_current(report: Report, current: dict[str, float | None]) -> None:
    report.current_settings = current
    for rec in report.recommendations:
        if rec.key in current:
            rec.current = current[rec.key]


def print_report(report: Report) -> None:
    print(f"Log dir: {report.log_dir}")
    print(f"Temp files: {', '.join(report.temp_files) or '(none)'}")
    print(f"Events: {report.event_file or '(none)'}")
    print(
        f"Samples: {report.sample_count} total, {report.non_defrost_count} non-defrost"
        + (f", span {report.span_hours:.1f} h" if report.span_hours is not None else "")
    )
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
            if k.endswith("_ALARM") or k in ("PROBE_FAULT", "DEFROST_START", "NO_COOL_ALARM", "ICE_ALARM")
        }
        if interesting:
            print(f"  notable counts: {interesting}")

    if report.recommendations:
        print("\nSuggested settings (review before applying):")
        print(f"  {'Setting':32s}  {'Suggest':>8s}  {'Current':>8s}  Conf    Why")
        for rec in report.recommendations:
            sug = "—" if rec.suggested is None else f"{rec.suggested:g}"
            cur = "—" if rec.current is None else f"{rec.current:g}"
            print(f"  {rec.label:32s}  {sug:>8s}  {cur:>8s}  {rec.confidence:6s}  {rec.rationale}")

    if report.notes:
        print("\nNotes:")
        for n in report.notes:
            print(f"  • {n}")


def main() -> int:
    args = _parse_args()
    log_dir = args.log_dir
    if not log_dir.is_dir():
        print(f"ERROR: not a directory: {log_dir}", file=sys.stderr)
        return 1

    temp_paths = sorted(
        [p for p in log_dir.iterdir() if p.is_file() and (_DATE_CSV.match(p.name) or p.name.lower() == "nodate.csv")],
        key=lambda p: p.name,
    )
    event_path = next((p for p in log_dir.iterdir() if p.is_file() and p.name.lower() == "events.csv"), None)

    rows = load_temp_rows(temp_paths)
    event_stats = load_event_stats(event_path) if event_path else None
    report = analyze(rows, event_stats, min_samples=args.min_samples)
    report.log_dir = str(log_dir)
    report.temp_files = [p.name for p in temp_paths]
    report.event_file = event_path.name if event_path else None

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
        payload = {
            "log_dir": report.log_dir,
            "temp_files": report.temp_files,
            "event_file": report.event_file,
            "sample_count": report.sample_count,
            "non_defrost_count": report.non_defrost_count,
            "span_hours": report.span_hours,
            "stats": report.stats,
            "event_stats": report.event_stats,
            "notes": report.notes,
            "current_settings": report.current_settings,
            "recommendations": [asdict(r) for r in report.recommendations],
        }
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"\nJSON report → {args.json_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
