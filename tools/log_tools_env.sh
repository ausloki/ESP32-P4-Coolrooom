#!/usr/bin/env bash
# Shared env bootstrap for coolroom log tools (macOS / Linux).
# Creates tools/.venv-log-tuning and installs requirements_log_tuning.txt.
# Echoes the python path on stdout (last line); diagnostics on stderr.

set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${TOOLS_DIR}/.." && pwd)"
VENV_DIR="${TOOLS_DIR}/.venv-log-tuning"
REQ="${TOOLS_DIR}/requirements_log_tuning.txt"
MARKER="${VENV_DIR}/.deps_ok"

find_python() {
  if command -v python3 >/dev/null 2>&1; then
    command -v python3
    return
  fi
  if command -v python >/dev/null 2>&1; then
    command -v python
    return
  fi
  echo "ERROR: Python 3 not found. Install Python 3.10+ from python.org or Homebrew (brew install python)." >&2
  exit 1
}

PY_SYS="$(find_python)"
# Require 3.10+
if ! "${PY_SYS}" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)'; then
  echo "ERROR: Need Python 3.10+, found: $(${PY_SYS} --version 2>&1)" >&2
  exit 1
fi

if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
  echo "[log-tools] Creating venv at ${VENV_DIR} ..." >&2
  "${PY_SYS}" -m venv "${VENV_DIR}"
fi

PY="${VENV_DIR}/bin/python"

if [[ ! -f "${MARKER}" ]] || [[ "${REQ}" -nt "${MARKER}" ]]; then
  echo "[log-tools] Installing dependencies (stdlib tools — pip check only) ..." >&2
  "${PY}" -m pip install --upgrade pip setuptools wheel >/dev/null
  "${PY}" -m pip install -r "${REQ}"
  date > "${MARKER}"
  echo "[log-tools] Environment ready." >&2
fi

# Export for callers that source this file
export COOLROOM_LOG_TOOLS_PYTHON="${PY}"
export COOLROOM_REPO_ROOT="${REPO_ROOT}"
export COOLROOM_TOOLS_DIR="${TOOLS_DIR}"

# When executed (not sourced), print python path
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "${PY}"
fi
