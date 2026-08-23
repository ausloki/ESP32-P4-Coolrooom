#!/bin/bash
# Project PATH + SSL cert for interactive shells and one-off commands.
# Usage (from repo root):  source tools/project_env.sh
set -euo pipefail

if [[ -n "${BASH_SOURCE[0]:-}" ]]; then
  _SELF="${BASH_SOURCE[0]}"
elif [[ -n "${ZSH_VERSION:-}" ]]; then
  # shellcheck disable=SC2296
  _SELF="${(%):-%x}"
else
  _SELF="$0"
fi

ROOT_DIR="$(cd "$(dirname "$_SELF")/.." && pwd)"
export PATH="$ROOT_DIR/.venv/bin:/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

if [[ -x "$ROOT_DIR/.venv/bin/python" ]]; then
  export SSL_CERT_FILE="$("$ROOT_DIR/.venv/bin/python" -c "import certifi; print(certifi.where())")"
fi
