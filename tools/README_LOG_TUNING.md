# Coolroom log pull & settings tune tools

Cross-platform helpers (Windows + macOS) to download SD logs from the controller
and recommend control-setting tweaks from ≥ **30 days** of history (in the
**selected analysis window**).

## Prerequisites

| Requirement | Notes |
|---|---|
| **Python 3.10+** | On PATH (`python3` / `py -3`). Launchers create `tools/.venv-log-tuning` (stdlib only). |
| **LAN to controller** | For live pull / live settings / `--apply`. REST API has **no HTTP Basic Auth** — LAN is the trust boundary. Dashboard “Login” only unlocks the web UI. |
| **SD card on device** | Mounted; `events.csv` and/or daily `YYYY-MM-DD.csv` present. |
| **≥ 30 days in window** | See gate rule below (override with `--min-days`). |

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

Interactive menu: `CoolroomLogTools.sh` / `CoolroomLogTools.cmd` (item **5** = seasonal /
windowed recommend-only → `logs/tune_reports/`).

## Analysis window (`--since` / `--until` / `--window`)

By default the tools use the **full** log span. For harvest months, pass an inclusive
calendar window — the history gate and all stats (temps, events, ambient, cycle rate)
apply to that window only:

| Flag | Meaning |
|---|---|
| `--since YYYY-MM-DD` | Inclusive start |
| `--until YYYY-MM-DD` | Inclusive end |
| `--window last30` | Rolling last 30 calendar days (through today) |
| `--window picking` | Current (or last completed) **WA fruit picking** season |

### WA fruit picking season (profile notes)

Western Australian fruit picking is typically **late December → April**. External ambient
can climb toward **~40 °C**. During that load:

- Keep **setpoint at product need** (2.0 °C food profile) — the tuner **never** auto-applies setpoint.
- Re-run the log tune **monthly** or **per harvest block**.
- Re-evaluate **differential**, **defrost interval / ice risk**, and **no-cool / high-alarm**
  nuisance as ambient rises (the report biases notes when ambient p95 ≥ ~30 °C or max near 40 °C).
- If daily CSVs lack `ambient_c` (or an alternate external column), the tool says so — it does
  not invent ambient.

Example windows:

```bash
# January harvest block
python tools/analyse_logs_tune_settings.py --log-dir logs/controller/latest \
  --since 2026-01-01 --until 2026-01-31 --report logs/tune_reports/jan.html

# Full open season to date (preset)
python tools/analyse_logs_tune_settings.py --host 192.168.37.237 --window picking

# Dec 20 → end of April (explicit completed season)
python tools/analyse_logs_tune_settings.py --log-dir path \
  --since 2025-12-20 --until 2026-04-30
```

Preset `picking` resolves to **20 Dec → 30 Apr** (season year = April year). During an open
season the end date is clipped to today; outside season it returns the last completed season.

## 30-day history gate

Tweaks are refused (exit code **2**) unless usable history in the **selected window**
spans ≥ 30 days (default; `--min-days` overrides):

1. **Prefer `events.csv`**: span = last − first parseable timestamp **in the window**.
2. If `events.csv` is **missing** or has &lt;2 parseable timestamps in-window → fall back to dated
   `YYYY-MM-DD.csv` **filename** span intersecting the window (latest − earliest calendar days).
3. If `events.csv` exists but spans **&lt; min-days** in-window, that is a **hard fail** — the tool does
   not invent recommendations from a short event window even when more temp days exist.

Override only for experiments: `--min-days 0` (not recommended on a live plant).

## Ambient trends & cycle rate

When daily CSVs include `ambient_c` (also accepts `external_c` / `external_temp_c` / …), the
CLI and HTML report summarise **mean / p95 / max** and a simple half-window trend. High ambient
adds WA summer-load notes (differential / cycle rate, defrost/ice, no-cool / HI nuisance).

**Cycle rate** (from `events.csv`): compressor starts/day, median/p10/p90 ON and OFF minutes.
When `--since` / `--until` / `--window` defines a window of length N, the tool also compares to
the **prior equal-length** window immediately before (if that prior has usable events).

## Scheduled runner (recommend-only)

`tools/run_seasonal_log_tune.{sh,cmd,ps1}` pulls (optional `--host`), analyses a configured
window, and writes dated HTML under `logs/tune_reports/`. It does **not** pass `--apply`.

```bash
# macOS / Linux — last 30 days
./tools/run_seasonal_log_tune.sh --host 192.168.37.237 --window last30

# Current WA picking season
./tools/run_seasonal_log_tune.sh --host 192.168.37.237 --window picking

# Offline
./tools/run_seasonal_log_tune.sh --log-dir logs/controller/latest --window picking
```

```bat
REM Windows
tools\run_seasonal_log_tune.cmd --host 192.168.37.237 --window last30
tools\run_seasonal_log_tune.cmd --log-dir logs\controller\latest --window picking
```

### macOS — launchd (example)

Save as `~/Library/LaunchAgents/com.coolroom.seasonal-log-tune.plist` (edit paths / host):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>com.coolroom.seasonal-log-tune</string>
  <key>ProgramArguments</key>
  <array>
    <string>/Volumes/Scratch/Documents/ESP32-P4-Coolroom/tools/run_seasonal_log_tune.sh</string>
    <string>--host</string><string>192.168.37.237</string>
    <string>--window</string><string>last30</string>
  </array>
  <key>StartCalendarInterval</key>
  <dict><key>Weekday</key><integer>1</integer><key>Hour</key><integer>6</integer><key>Minute</key><integer>0</integer></dict>
  <key>StandardOutPath</key><string>/tmp/coolroom-seasonal-tune.log</string>
  <key>StandardErrorPath</key><string>/tmp/coolroom-seasonal-tune.err</string>
</dict>
</plist>
```

```bash
launchctl load ~/Library/LaunchAgents/com.coolroom.seasonal-log-tune.plist
```

Cron alternative (Mondays 06:00):

```cron
0 6 * * 1 /path/to/ESP32-P4-Coolroom/tools/run_seasonal_log_tune.sh --host 192.168.37.237 --window last30
```

### Windows — Task Scheduler (example)

```powershell
# From an elevated PowerShell, or use Task Scheduler GUI:
$action = New-ScheduledTaskAction `
  -Execute 'powershell.exe' `
  -Argument '-NoProfile -ExecutionPolicy Bypass -File C:\path\to\ESP32-P4-Coolroom\tools\run_seasonal_log_tune.ps1 -HostAddress 192.168.37.237 -Window last30'
$trigger = New-ScheduledTaskTrigger -Weekly -DaysOfWeek Monday -At 6am
Register-ScheduledTask -TaskName 'CoolroomSeasonalLogTune' -Action $action -Trigger $trigger
```

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
| `analyze_coolroom_logs.py` | Analyse a local folder (optional `--min-days`, `--since`/`--until`/`--window`) |
| `recommend_settings.py` | Thin pull + analyse wrapper (legacy entry) |
| `analyse_logs_tune_settings.py` | **Preferred** pull/analyse/report/apply entry |
| `run_seasonal_log_tune.{sh,cmd,ps1}` | Scheduled recommend-only HTML under `logs/tune_reports/` |

## Fixtures

```bash
# Should succeed (exit 0)
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_30d

# Should refuse (exit 2)
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_short

# Short seasonal / high-ambient smoke (--min-days 0)
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_seasonal \
  --since 2026-01-05 --until 2026-01-18 --min-days 0

# Window shorter than default gate → refuse
python tools/analyse_logs_tune_settings.py --log-dir tools/testdata/log_tune_30d \
  --since 2026-06-28 --until 2026-07-02
```

## Profile baseline

Recommendations are cross-checked against Quick Start §4 /
`reference/recommended_settings_2c.html` (2 °C food coolroom).
