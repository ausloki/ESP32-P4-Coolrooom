#!/usr/bin/env bash
# Scheduled / one-shot seasonal log tune (recommend-only).
# Pulls logs (optional), analyses a window, writes dated HTML under logs/tune_reports/.
# Never passes --apply.
#
# Usage:
#   ./tools/run_seasonal_log_tune.sh --host 192.168.37.237
#   ./tools/run_seasonal_log_tune.sh --log-dir logs/controller/latest --window picking
#   ./tools/run_seasonal_log_tune.sh --log-dir tools/testdata/log_tune_30d --window last30 --min-days 0
#
# Env overrides:
#   COOLROOM_HOST, COOLROOM_WINDOW (last30|picking), COOLROOM_REPORT_DIR,
#   COOLROOM_MIN_DAYS, COOLROOM_LOG_DIR
#
# See tools/README_LOG_TUNING.md (WA picking season + launchd/cron examples).

set -euo pipefail
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=log_tools_env.sh
source "${TOOLS_DIR}/log_tools_env.sh"
cd "${COOLROOM_REPO_ROOT}"

HOST="${COOLROOM_HOST:-}"
WINDOW="${COOLROOM_WINDOW:-last30}"
REPORT_DIR="${COOLROOM_REPORT_DIR:-logs/tune_reports}"
MIN_DAYS="${COOLROOM_MIN_DAYS:-30}"
LOG_DIR="${COOLROOM_LOG_DIR:-}"
EXTRA=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) HOST="${2:-}"; shift 2 ;;
    --window) WINDOW="${2:-}"; shift 2 ;;
    --log-dir) LOG_DIR="${2:-}"; shift 2 ;;
    --report-dir) REPORT_DIR="${2:-}"; shift 2 ;;
    --min-days) MIN_DAYS="${2:-}"; shift 2 ;;
    --since|--until)
      # Pass explicit dates through; clear preset window
      EXTRA+=("$1" "${2:-}")
      WINDOW=""
      shift 2
      ;;
    -h|--help)
      sed -n '1,20p' "$0"
      exit 0
      ;;
    *)
      EXTRA+=("$1")
      shift
      ;;
  esac
done

mkdir -p "${REPORT_DIR}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
REPORT_PATH="${REPORT_DIR}/tune_${STAMP}.html"

ARGS=(--min-days "${MIN_DAYS}" --report "${REPORT_PATH}")
if [[ -n "${WINDOW}" ]]; then
  ARGS+=(--window "${WINDOW}")
fi
if [[ -n "${HOST}" ]]; then
  ARGS+=(--host "${HOST}")
elif [[ -n "${LOG_DIR}" ]]; then
  ARGS+=(--log-dir "${LOG_DIR}")
else
  echo "ERROR: provide --host IP or --log-dir PATH (or COOLROOM_HOST / COOLROOM_LOG_DIR)" >&2
  exit 1
fi

echo "→ seasonal tune (recommend-only) window=${WINDOW:-custom} report=${REPORT_PATH}"
exec "${COOLROOM_LOG_TOOLS_PYTHON}" \
  "${TOOLS_DIR}/analyse_logs_tune_settings.py" \
  "${ARGS[@]}" \
  "${EXTRA[@]}"
