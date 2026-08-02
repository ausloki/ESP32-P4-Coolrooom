# Synthetic coolroom log fixtures for analyse_logs_tune_settings.py

| Folder | Purpose |
|---|---|
| `log_tune_30d/` | `events.csv` spanning ~31 days + three dated temp CSVs — should pass the ≥30-day gate. |
| `log_tune_short/` | ~4 days of events — must exit 2 with a clear insufficient-history message. |

Timestamps use the device format `DD-MM-YYYY HH:MM:SS` (same as `p4_fmt_time`).
