#!/usr/bin/env bash
# Analyze a local pulled log folder (macOS / Linux).
# Usage: ./tools/run_analyze_logs.sh [log_dir] [host]

set -euo pipefail
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=log_tools_env.sh
source "${TOOLS_DIR}/log_tools_env.sh"
cd "${COOLROOM_REPO_ROOT}"

latest="$(ls -1dt "${COOLROOM_REPO_ROOT}/logs/controller"/*/ 2>/dev/null | head -1 || true)"
latest="${latest%/}"
default_dir="${latest:-${COOLROOM_REPO_ROOT}/logs/controller}"

DIR="${1:-}"
HOST="${2:-}"
if [[ -z "${DIR}" ]]; then
  read -r -p "Log directory [${default_dir}]: " DIR || true
  DIR="${DIR:-${default_dir}}"
fi
if [[ -z "${HOST}" ]]; then
  read -r -p "Controller host for live settings (blank to skip) [192.168.37.237]: " HOST || true
  HOST="${HOST:-192.168.37.237}"
fi

if [[ -n "${HOST}" ]]; then
  exec "${COOLROOM_LOG_TOOLS_PYTHON}" \
    "${TOOLS_DIR}/analyze_coolroom_logs.py" "${DIR}" --host "${HOST}"
else
  exec "${COOLROOM_LOG_TOOLS_PYTHON}" \
    "${TOOLS_DIR}/analyze_coolroom_logs.py" "${DIR}"
fi
