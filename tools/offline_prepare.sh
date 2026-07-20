#!/bin/bash
# Prepare this repository for offline development and compile.
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

ROOT_DIR="$(cd "$(/usr/bin/dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PY="$ROOT_DIR/.venv/bin/python"
WHEEL_DIR="$ROOT_DIR/tools/offline/wheels"
CONFIG_PATH="${1:-esp32-p4-coolroom.yaml}"
SKIP_COMPILE="${OFFLINE_SKIP_COMPILE:-0}"

mkdir -p "$WHEEL_DIR"

if [[ ! -x "$VENV_PY" ]]; then
  python3 -m venv "$ROOT_DIR/.venv"
fi

"$VENV_PY" "$ROOT_DIR/tools/dependency_check.py" --install
"$VENV_PY" -m pip download -r "$ROOT_DIR/requirements.txt" -d "$WHEEL_DIR"

if [[ "$SKIP_COMPILE" != "1" ]]; then
  "$ROOT_DIR/tools/esphome_compile.sh" "$CONFIG_PATH"
fi

echo "Offline prep complete."
echo "Wheel cache: $WHEEL_DIR"
if [[ "$SKIP_COMPILE" == "1" ]]; then
  echo "Compile warm-up skipped (OFFLINE_SKIP_COMPILE=1)."
fi
