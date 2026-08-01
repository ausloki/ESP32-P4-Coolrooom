#!/usr/bin/env bash
# Interactive menu for coolroom log pull / settings recommend (macOS / Linux).
# Double-click CoolroomLogTools.command in Finder, or run: ./tools/CoolroomLogTools.sh

set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=log_tools_env.sh
source "${TOOLS_DIR}/log_tools_env.sh"

cd "${COOLROOM_REPO_ROOT}"
PY="${COOLROOM_LOG_TOOLS_PYTHON}"
DEFAULT_HOST="192.168.37.237"

read_default() {
  local prompt="$1"
  local default="$2"
  local value=""
  read -r -p "${prompt} [${default}]: " value || true
  if [[ -z "${value}" ]]; then
    echo "${default}"
  else
    echo "${value}"
  fi
}

echo ""
echo "=== ESP32-P4 Coolroom — Log tools (macOS) ==="
echo "Python: ${PY}"
echo "Repo:   ${COOLROOM_REPO_ROOT}"
echo ""
echo "  1) Recommend settings (pull logs + analyze)"
echo "  2) Pull logs only"
echo "  3) List SD files on controller"
echo "  4) Analyze existing local log folder"
echo "  Q) Quit"
echo ""
read -r -p "Choose: " choice || true

case "$(echo "${choice}" | tr '[:lower:]' '[:upper:]')" in
  1)
    host="$(read_default "Controller host/IP" "${DEFAULT_HOST}")"
    days="$(read_default "Days of dated temp logs (0 = all)" "14")"
    "${PY}" "${TOOLS_DIR}/recommend_settings.py" --host "${host}" --days "${days}"
    ;;
  2)
    host="$(read_default "Controller host/IP" "${DEFAULT_HOST}")"
    days="$(read_default "Days of dated temp logs (0 = all)" "14")"
    "${PY}" "${TOOLS_DIR}/pull_controller_logs.py" --host "${host}" --days "${days}"
    ;;
  3)
    host="$(read_default "Controller host/IP" "${DEFAULT_HOST}")"
    "${PY}" "${TOOLS_DIR}/pull_controller_logs.py" --host "${host}" --list-only
    ;;
  4)
    latest="$(ls -1dt "${COOLROOM_REPO_ROOT}/logs/controller"/*/ 2>/dev/null | head -1 || true)"
    latest="${latest%/}"
    default_dir="${latest:-${COOLROOM_REPO_ROOT}/logs/controller}"
    dir="$(read_default "Log directory to analyze" "${default_dir}")"
    host="$(read_default "Controller host for live settings (blank to skip)" "${DEFAULT_HOST}")"
    if [[ -n "${host}" ]]; then
      "${PY}" "${TOOLS_DIR}/analyze_coolroom_logs.py" "${dir}" --host "${host}"
    else
      "${PY}" "${TOOLS_DIR}/analyze_coolroom_logs.py" "${dir}"
    fi
    ;;
  Q|"")
    exit 0
    ;;
  *)
    echo "Unknown choice: ${choice}" >&2
    exit 1
    ;;
esac

echo ""
read -r -p "Done. Press Enter to close." _ || true
