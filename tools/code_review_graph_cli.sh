#!/usr/bin/env bash
# code_review_graph_cli.sh — wrapper to run the code-review graph tool
# from the project-local virtualenv, regardless of working directory.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PY="$ROOT_DIR/.venv/bin/python"

if [[ ! -x "$PY" ]]; then
  echo "ERROR: Virtualenv not found at $ROOT_DIR/.venv" >&2
  echo "Run: python3 -m venv $ROOT_DIR/.venv && $ROOT_DIR/.venv/bin/pip install esphome code-review-graph" >&2
  exit 1
fi

exec "$PY" -m code_review_graph "$@"
