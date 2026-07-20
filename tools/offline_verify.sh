#!/bin/bash
# Verify offline readiness (dependency cache + local toolchain assets).
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

ROOT_DIR="$(cd "$(/usr/bin/dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PY="$ROOT_DIR/.venv/bin/python"

if [[ ! -x "$VENV_PY" ]]; then
  echo "ERROR: Missing virtualenv python at $VENV_PY" >&2
  exit 1
fi

"$VENV_PY" "$ROOT_DIR/tools/dependency_check.py" --check-offline

IDF_CACHE_A="$HOME/Library/Caches/esphome/idf"
IDF_CACHE_B="$HOME/.esphome-idf"
IDF_CACHE=""
if [[ -d "$IDF_CACHE_A" ]]; then
  IDF_CACHE="$IDF_CACHE_A"
elif [[ -d "$IDF_CACHE_B" ]]; then
  IDF_CACHE="$IDF_CACHE_B"
fi

if [[ -z "$IDF_CACHE" ]]; then
  echo "ERROR: No local ESP-IDF cache found. Run ./tools/offline_prepare.sh while online." >&2
  exit 1
fi

if [[ ! -d "$IDF_CACHE/penvs/5.5.4" ]]; then
  echo "ERROR: Missing IDF Python env cache at $IDF_CACHE/penvs/5.5.4" >&2
  exit 1
fi

if [[ ! -d "$IDF_CACHE/tools/cmake/3.30.2" ]]; then
  echo "ERROR: Missing IDF CMake cache at $IDF_CACHE/tools/cmake/3.30.2" >&2
  exit 1
fi

echo "Offline verify: PASS"
