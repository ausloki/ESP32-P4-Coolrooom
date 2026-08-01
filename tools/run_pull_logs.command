#!/bin/bash
cd "$(dirname "$0")" || exit 1
./run_pull_logs.sh "$@"
echo ""
read -r -p "Done. Press Enter to close." _ || true
