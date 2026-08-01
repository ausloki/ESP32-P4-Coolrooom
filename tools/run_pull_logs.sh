#!/usr/bin/env bash
# Download SD logs from the coolroom controller (macOS / Linux).
# Usage: ./tools/run_pull_logs.sh [host] [--days N | --list-only]

set -euo pipefail
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=log_tools_env.sh
source "${TOOLS_DIR}/log_tools_env.sh"
cd "${COOLROOM_REPO_ROOT}"

HOST="${1:-}"
shift || true
if [[ -z "${HOST}" ]]; then
  read -r -p "Controller host/IP [192.168.37.237]: " HOST || true
  HOST="${HOST:-192.168.37.237}"
fi

exec "${COOLROOM_LOG_TOOLS_PYTHON}" \
  "${TOOLS_DIR}/pull_controller_logs.py" \
  --host "${HOST}" \
  "$@"
