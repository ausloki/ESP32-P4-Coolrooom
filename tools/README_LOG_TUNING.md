# Coolroom log pull & settings tune tools

Cross-platform helpers (Windows + macOS) to download SD logs from the controller
and recommend control-setting tweaks from ≥ **30 days** of history.

## Prerequisites

| Requirement | Notes |
|---|---|
| **Python 3.10+** | On PATH (`python3` / `py -3`). Launchers create `tools/.venv-log-tuning` (stdlib only). |
| **LAN to controller** | For live pull / live settings / `--apply`. REST API has **no HTTP Basic Auth** — LAN is the trust boundary. Dashboard “Login” only unlocks the web UI. |
| **SD card on device** | Mounted; `events.csv` and/or daily `YYYY-MM-DD.csv` present. |
| **≥ 30 days history** | See gate rule below. |

Offline analysis needs only a local folder of previously downloaded CSVs (no device).

## Primary command

```bash
# macOS / Linux
./tools/analyse_logs_tune_settings.sh --log-dir tools/testdata/log_tune_30d
./tools/analyse_logs_tune_settings.sh --host 192.168.37.237

# Windows (cmd)
tools\analyse_logs_tune_settings.cmd --log-dir tools\testdata\log_tune_30d
tools\analyse_logs_tune_settings.cmd --host 192.168.37.237

# Or any OS once the venv exists:
tools/.venv-log-tuning/bin/python tools/analyse_logs_tune_settings.py --help   # macOS
tools\.venv-log-tuning\Scripts\python.exe tools\analyse_logs_tune_settings.py --help  # Windows
```

Interactive menu: `CoolroomLogTools.sh` / `CoolroomLogTools.cmd`.

## 30-day history gate

Tweaks are refused (exit code **2**) unless usable history spans ≥ 30 days:

1. **Prefer `events.csv`**: span = last − first parseable timestamp.
2. If `events.csv` is **missing** or has &lt;2 parseable timestamps → fall back to dated
   `YYYY-MM-DD.csv` **filename** span (latest − earliest calendar days).
3. If `events.csv` exists but spans **&lt; 30 days**, that is a **hard fail** — the tool does
   not invent recommendations from a short event window even when more temp days exist.

Override only for experiments: `--min-days 0` (not recommended on a live plant).

## Device HTTP routes used (existing firmware)

| Route | Purpose |
|---|---|
| `GET /logs` | List SD files (`p4_log_manager.h`) |
| `GET /logs/download?file=NAME` | Download allowlisted log names |
| `GET /number/&lt;Name&gt;` | Read live settings |
| `POST /number/&lt;Name&gt;/set?value=…` | Apply allowlisted tweaks (`--apply`) |

No new firmware protocol. Entity POSTs need `Content-Length: 0` (empty body).

## Apply allowlist (opt-in)

Default is **recommend-only**. `--apply` requires `--yes` (or typing `YES`) and only posts:

- Compressor Differential (°C)
- Compressor Off-Delay (min)
- Compressor Min Run Time (min)
- High / Low Temp Alarm Delta (°C)
- Alarm Persist Time (min)
- No-Cool Alarm Timeout (min)
- Defrost Interval (min)
- Door Alarm Delay (s)

**Never** auto-applied: setpoint, probe/sensor enables, door-hold, smart-defrost switches.

## Related scripts

| Script | Role |
|---|---|
| `pull_controller_logs.py` | Download only |
| `analyze_coolroom_logs.py` | Analyse a local folder (optional `--min-days`) |
| `recommend_settings.py` | Thin pull + analyse wrapper (legacy entry) |
| `analyse_logs_tune_settings.py` | **Preferred** pull/analyse/report/apply entry |

## Fixtures

```bash
# Should succeed (exit 0)
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_30d

# Should refuse (exit 2)
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_short
```

## Profile baseline

Recommendations are cross-checked against Quick Start §4 /
`reference/recommended_settings_2c.html` (2 °C food coolroom).
