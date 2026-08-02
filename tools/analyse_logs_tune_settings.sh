#!/usr/bin/env bash
# Analyse coolroom logs / recommend settings (macOS / Linux).
# Usage:
#   ./tools/analyse_logs_tune_settings.sh --log-dir tools/testdata/log_tune_30d
#   ./tools/analyse_logs_tune_settings.sh --host 192.168.37.237
# Double-click: analyse_logs_tune_settings.command

set -euo pipefail
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=log_tools_env.sh
source "${TOOLS_DIR}/log_tools_env.sh"
cd "${COOLROOM_REPO_ROOT}"

exec "${COOLROOM_LOG_TOOLS_PYTHON}" \
  "${TOOLS_DIR}/analyse_logs_tune_settings.py" \
  "$@"
