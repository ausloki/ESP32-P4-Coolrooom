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

derive_build_root() {
  local config_name
  config_name="$1"
  config_name="$(${ROOT_DIR}/.venv/bin/python - <<'PY' "$config_name"
from pathlib import Path
import sys
print(Path(sys.argv[1]).stem)
PY
)"
  printf '%s/.esphome/build/%s' "$ROOT_DIR" "$config_name"
}

patch_generated_src_cmakelists() {
  local build_root="$1"
  local cmake_file="$build_root/src/CMakeLists.txt"

  if [[ ! -f "$cmake_file" ]]; then
    echo "ERROR: Expected generated CMake file not found: $cmake_file" >&2
    return 1
  fi

  if /usr/bin/grep -q 'esp_http_server esp_ringbuf' "$cmake_file"; then
    return 0
  fi

  /usr/bin/perl -0pi -e 's/REQUIRES \$\{ESPHOME_PROJECT_BUILTIN_COMPONENTS\}/REQUIRES \$\{ESPHOME_PROJECT_BUILTIN_COMPONENTS\} esp_http_server esp_ringbuf/' "$cmake_file"
}

retry_known_reconfigure_failure() {
  local config_path="$1"
  local compile_log="$2"
  local build_root build_dir idf_version idf_env_exports ninja_bin
  local -a ninja_cmd

  if ! /usr/bin/grep -q 'requirements list of "src"' "$compile_log"; then
    return 1
  fi

  if ! /usr/bin/grep -Eq 'esp_http_server|esp_ringbuf' "$compile_log"; then
    return 1
  fi

  build_root="$(derive_build_root "$config_path")"
  build_dir="$build_root/build"
  idf_version="$(${ROOT_DIR}/.venv/bin/python - <<'PY' "$compile_log"
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding='utf-8', errors='ignore')
match = re.search(r'Checking ESP-IDF\s+([0-9][0-9.]+)', text)
print(match.group(1) if match else '5.5.4')
PY
)"
  idf_env_exports="$(${ROOT_DIR}/.venv/bin/python - <<'PY' "$idf_version"
import os
import shlex
import sys

from esphome.espidf.framework import get_framework_env
from esphome.espidf.toolchain import _get_esphome_esp_idf_paths

env = get_framework_env(*_get_esphome_esp_idf_paths(sys.argv[1]), env={"PATH": os.environ.get("PATH", "")})
for key in ("IDF_PATH", "IDF_TOOLS_PATH", "IDF_PYTHON_ENV_PATH", "ESP_IDF_VERSION", "PATH", "CCACHE_DIR", "OPENOCD_SCRIPTS"):
    value = env.get(key)
    if value:
        print(f'export {key}={shlex.quote(value)}')
PY
)"
  # Prefer the IDF's own ninja: it is a universal binary, whereas a Homebrew
  # ninja on an Apple Silicon host is often x86_64-only, which fails outright
  # under the `arch -arm64` wrapper below ("Bad CPU type in executable").
  ninja_bin=""
  for candidate in "$HOME"/Library/Caches/esphome/idf/tools/ninja/*/ninja; do
    if [[ -x "$candidate" ]]; then
      ninja_bin="$candidate"
      break
    fi
  done
  if [[ -z "$ninja_bin" ]]; then
    ninja_bin="$(command -v ninja || true)"
  fi

  if [[ -z "$ninja_bin" ]]; then
    echo "ERROR: ninja is required for compile retry but was not found in PATH" >&2
    return 1
  fi

  ninja_cmd=("$ninja_bin")
  if [[ "$(/usr/bin/uname -m)" == "arm64" ]]; then
    ninja_cmd=(/usr/bin/arch -arm64 "$ninja_bin")
  fi

  echo "Detected ESPHome native IDF REQUIRES omission during reconfigure; patching generated src/CMakeLists.txt and retrying build..." >&2
  patch_generated_src_cmakelists "$build_root"
  eval "$idf_env_exports"

  "${ninja_cmd[@]}" -C "$build_dir" all
  "${ninja_cmd[@]}" -C "$build_dir" size
}

cd "$ROOT_DIR"

COMPILE_LOG="$(/usr/bin/mktemp "${TMPDIR:-/tmp}/esphome_compile.XXXXXX.log")"
trap '/bin/rm -f "$COMPILE_LOG"' EXIT

if esphome compile "$CONFIG_PATH" 2>&1 | /usr/bin/tee "$COMPILE_LOG"; then
  exit 0
fi

if retry_known_reconfigure_failure "$CONFIG_PATH" "$COMPILE_LOG"; then
  exit 0
fi

exit 1
