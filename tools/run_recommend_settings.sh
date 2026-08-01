#!/usr/bin/env bash
# Pull controller logs + recommend settings (macOS / Linux).
# Usage: ./tools/run_recommend_settings.sh [host] [--days N]
# Double-click: run_recommend_settings.command

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
  "${TOOLS_DIR}/recommend_settings.py" \
  --host "${HOST}" \
  "$@"
