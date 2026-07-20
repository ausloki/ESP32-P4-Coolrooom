#!/bin/bash
# Verify local ESPHome/IDF compile prerequisites for this repository.
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

ROOT_DIR="$(cd "$(/usr/bin/dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PY="$ROOT_DIR/.venv/bin/python"
FIX_ARM64_PENV="false"

if [[ "${1:-}" == "--fix-arm64-penv" ]]; then
  FIX_ARM64_PENV="true"
fi

if [[ ! -x "$VENV_PY" ]]; then
  echo "ERROR: Missing virtualenv python at $VENV_PY" >&2
  echo "Run: python3 -m venv $ROOT_DIR/.venv && $ROOT_DIR/.venv/bin/pip install esphome code-review-graph certifi" >&2
  exit 1
fi

if [[ ! -x "/usr/bin/git" ]]; then
  echo "ERROR: /usr/bin/git is not available." >&2
  exit 1
fi

if [[ ! -x "/usr/bin/lipo" ]]; then
  echo "ERROR: /usr/bin/lipo is not available." >&2
  exit 1
fi

"$VENV_PY" - <<'PY'
import importlib
mods = ["esphome", "certifi"]
missing = []
for name in mods:
  try:
    importlib.import_module(name)
  except Exception:
    missing.append(name)
if missing:
    raise SystemExit("Missing python modules in .venv: " + ", ".join(missing))
print("Python module check: OK (esphome, certifi)")
PY

CMAKE_BIN="$(command -v cmake || true)"
if [[ -n "$CMAKE_BIN" ]]; then
  echo "cmake in PATH: $CMAKE_BIN"
  echo "cmake arch: $(/usr/bin/file "$CMAKE_BIN")"
fi

HOST_ARCH="$(/usr/bin/uname -m)"

check_or_fix_penv() {
  local penv_py="$1"
  if [[ ! -x "$penv_py" ]]; then
    return 0
  fi

  local lipo_info
  lipo_info="$(/usr/bin/lipo -info "$penv_py" 2>/dev/null || true)"
  echo "IDF penv python: $penv_py"
  echo "Architecture info: ${lipo_info:-unknown}"

  local runtime_err
  runtime_err="$(mktemp)"
  if ! "$penv_py" - <<'PY' 2>"$runtime_err"
import pydantic_core
print("IDF penv runtime import check: OK (pydantic_core)")
PY
  then
    if grep -qi "incompatible architecture" "$runtime_err"; then
      if [[ "$FIX_ARM64_PENV" == "true" && "$HOST_ARCH" == "arm64" && "$lipo_info" == *"Architectures in the fat file"* && "$lipo_info" == *"x86_64"* && "$lipo_info" == *"arm64"* ]]; then
        local ts tmp backup
        ts="$(date +%Y%m%d_%H%M%S)"
        tmp="$(mktemp)"
        backup="${penv_py}.bak.${ts}"
        /usr/bin/lipo "$penv_py" -thin arm64 -output "$tmp"
        cp "$penv_py" "$backup"
        cp "$tmp" "$penv_py"
        chmod +x "$penv_py"
        rm -f "$tmp"
        echo "Applied arm64 penv fix. Backup: $backup"
        echo "New arch: $(/usr/bin/lipo -info "$penv_py" 2>/dev/null || true)"
        if ! "$penv_py" - <<'PY'
import pydantic_core
print("IDF penv runtime import check: OK (pydantic_core)")
PY
        then
          echo "ERROR: penv runtime still failing after arm64 fix." >&2
          rm -f "$runtime_err"
          return 1
        fi
      else
        echo "ERROR: IDF penv python runtime architecture mismatch detected." >&2
        echo "Run: ./tools/esphome_env_check.sh --fix-arm64-penv" >&2
        echo "Details:" >&2
        cat "$runtime_err" >&2
        rm -f "$runtime_err"
        return 1
      fi
    else
      echo "ERROR: IDF penv python runtime check failed." >&2
      cat "$runtime_err" >&2
      rm -f "$runtime_err"
      return 1
    fi
  fi
  rm -f "$runtime_err"

  if [[ "$HOST_ARCH" != "arm64" ]]; then
    return 0
  fi

  if [[ "$lipo_info" == *"Non-fat file"* && "$lipo_info" == *"x86_64"* ]]; then
    echo "WARNING: penv python is x86_64-only on arm64 host; rebuild may be required." >&2
    return 0
  fi

  if [[ "$lipo_info" == *"Architectures in the fat file"* && "$lipo_info" == *"x86_64"* && "$lipo_info" == *"arm64"* ]]; then
    if [[ "$FIX_ARM64_PENV" != "true" ]]; then
      echo "NOTE: fat binary includes x86_64+arm64. Use --fix-arm64-penv to force arm64-only penv python if arch mismatch recurs." >&2
    fi
  fi
}

check_or_fix_penv "$HOME/Library/Caches/esphome/idf/penvs/5.5.4/bin/python"
check_or_fix_penv "$HOME/.esphome-idf/penvs/5.5.4/bin/python"

echo "Environment check complete."
