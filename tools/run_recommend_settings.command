#!/bin/bash
cd "$(dirname "$0")" || exit 1
./run_recommend_settings.sh "$@"
echo ""
read -r -p "Done. Press Enter to close." _ || true
