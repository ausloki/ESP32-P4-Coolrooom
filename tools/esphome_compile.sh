#!/bin/bash
# Compile helper that applies the repository's ESPHome environment setup.
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

ROOT_DIR="$(cd "$(/usr/bin/dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PY="$ROOT_DIR/.venv/bin/python"

# On Apple Silicon, force this helper to run in native arm64 context.
if [[ "$(/usr/bin/uname -m)" == "arm64" ]]; then
  if [[ "${ESPHOME_COMPILE_ARM64_REEXEC:-0}" != "1" ]]; then
    if /usr/sbin/sysctl -in sysctl.proc_translated 2>/dev/null | /usr/bin/grep -q '^1$'; then
      export ESPHOME_COMPILE_ARM64_REEXEC=1
      exec /usr/bin/arch -arm64 /bin/bash "$0" "$@"
    fi
  fi
fi

ESPHOME_CMAKE_BIN="$HOME/Library/Caches/esphome/idf/tools/cmake/3.30.2/CMake.app/Contents/bin"
if [[ -x "$ESPHOME_CMAKE_BIN/cmake" ]]; then
  export PATH="$ESPHOME_CMAKE_BIN:$PATH"
fi

FIX_ARM64_PENV="false"
OFFLINE_MODE="false"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --fix-arm64-penv)
      FIX_ARM64_PENV="true"
      shift
      ;;
    --offline)
      OFFLINE_MODE="true"
      shift
      ;;
    *)
      break
      ;;
  esac
done

CONFIG_PATH="${1:-esp32-p4-coolroom.yaml}"

if [[ ! -x "$VENV_PY" ]]; then
  echo "ERROR: Missing virtualenv python at $VENV_PY" >&2
  exit 1
fi

if [[ "$FIX_ARM64_PENV" == "true" ]]; then
  "$ROOT_DIR/tools/esphome_env_check.sh" --fix-arm64-penv
else
  "$ROOT_DIR/tools/esphome_env_check.sh"
fi

if [[ "$OFFLINE_MODE" == "true" ]]; then
  "$ROOT_DIR/tools/offline_verify.sh"
fi

export SSL_CERT_FILE
SSL_CERT_FILE="$($VENV_PY -c 'import certifi; print(certifi.where())')"
export PATH="$ROOT_DIR/.venv/bin:/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

cd "$ROOT_DIR"
esphome compile "$CONFIG_PATH"
