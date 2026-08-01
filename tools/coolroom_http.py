#!/usr/bin/env python3
"""Shared LAN HTTP helpers for the ESP32-P4 coolroom controller.

Stdlib only — works on Windows and macOS with the same Python 3.10+ command.

The device web_server has no HTTP Basic Auth; the LAN is the trust boundary.
Log downloads use the custom handlers in p4_log_manager.h:

  GET  /logs
  GET  /logs/download?file=NAME
  GET  /api/events?since=N   (RAM ring this boot; optional)

Entity reads use ESPHome REST paths, e.g. GET /number/Setpoint%20(%C2%B0C)
"""

from __future__ import annotations

import json
import re
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

DEFAULT_HOST = "192.168.37.237"

# Published number entities used when comparing recommendations to live settings.
SETTING_NUMBERS: dict[str, str] = {
    "setpoint_c": "Setpoint (°C)",
    "differential_c": "Compressor Differential (°C)",
    "off_delay_min": "Compressor Off-Delay (min)",
    "min_run_min": "Compressor Min Run Time (min)",
    "alarm_high_delta_c": "High Temp Alarm Delta (°C)",
    "alarm_low_delta_c": "Low Temp Alarm Delta (°C)",
    "alarm_persist_min": "Alarm Persist Time (min)",
    "no_cool_timeout_min": "No-Cool Alarm Timeout (min)",
    "defrost_interval_min": "Defrost Interval (min)",
}


def base_url(host: str) -> str:
    host = host.strip()
    if host.startswith("http://") or host.startswith("https://"):
        return host.rstrip("/")
    return f"http://{host}"


def request(
    host: str,
    path: str,
    method: str = "GET",
    timeout: float = 30.0,
    raw: bool = False,
) -> tuple[int, Any]:
    """HTTP request. Returns (status, parsed JSON | text | bytes if raw)."""
    if not path.startswith("/"):
        path = "/" + path
    url = base_url(host) + path
    data = b"" if method == "POST" else None
    req = urllib.request.Request(url, data=data, method=method)
    if method == "POST":
        req.add_header("Content-Length", "0")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = resp.read()
            code = resp.getcode()
    except urllib.error.HTTPError as e:
        body = e.read()
        code = e.code
    except Exception as e:
        return 0, str(e)

    if raw:
        return code, body
    text = body.decode("utf-8", "replace")
    try:
        return code, json.loads(text) if text else None
    except json.JSONDecodeError:
        return code, text


def get_json(host: str, path: str, timeout: float = 30.0) -> Any:
    code, body = request(host, path, timeout=timeout)
    if code != 200:
        raise RuntimeError(f"GET {path} failed ({code}): {body!r}")
    return body


def download_bytes(host: str, path: str, timeout: float = 120.0) -> bytes:
    code, body = request(host, path, timeout=timeout, raw=True)
    if code != 200:
        text = body.decode("utf-8", "replace") if isinstance(body, (bytes, bytearray)) else body
        raise RuntimeError(f"GET {path} failed ({code}): {text!r}")
    if not isinstance(body, (bytes, bytearray)):
        raise RuntimeError(f"GET {path}: expected bytes, got {type(body)}")
    return bytes(body)


def list_logs(host: str, timeout: float = 30.0) -> dict[str, Any]:
    """Return the /logs JSON object (mounted, files, card stats)."""
    data = get_json(host, "/logs", timeout=timeout)
    if not isinstance(data, dict):
        raise RuntimeError(f"/logs returned unexpected payload: {data!r}")
    return data


def download_log_file(host: str, filename: str, timeout: float = 120.0) -> bytes:
    q = urllib.parse.urlencode({"file": filename})
    return download_bytes(host, f"/logs/download?{q}", timeout=timeout)


def object_path(domain: str, name: str) -> str:
    return f"/{domain}/{urllib.parse.quote(name)}"


def get_number(host: str, name: str, timeout: float = 8.0) -> float:
    code, body = request(host, object_path("number", name), timeout=timeout)
    if code != 200 or not isinstance(body, dict):
        raise RuntimeError(f"GET number/{name} failed ({code}): {body!r}")
    raw = body.get("value", body.get("state"))
    if isinstance(raw, (int, float)):
        return float(raw)
    text = str(raw).strip().replace(",", " ")
    token = text.split()[0] if text else ""
    return float(token)


def fetch_current_settings(host: str) -> dict[str, float | None]:
    """Best-effort read of live control numbers. Missing entities → None."""
    out: dict[str, float | None] = {}
    for key, entity in SETTING_NUMBERS.items():
        try:
            out[key] = get_number(host, entity)
        except Exception:
            out[key] = None
    return out


_NUM_RE = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)")


def parse_leading_float(text: str) -> float | None:
    m = _NUM_RE.search(text or "")
    if not m:
        return None
    try:
        return float(m.group(0))
    except ValueError:
        return None
