#!/bin/bash
# Serial flash that preserves saved settings.
#
# `esphome upload` over USB writes firmware.factory.bin from offset 0x0. That
# merged image is padded with 0xFF across the whole range it covers, including
# the NVS partition at 0x9000-0x15000 — so a serial upload silently erases every
# stored setting and the next boot falls back to each global's initial_value.
# The classic symptom is "I disabled ntfy (or the HA API toggle) and it came
# back on its own".
#
# This script flashes the four real images at their own offsets instead, leaving
# the NVS window untouched:
#
#   0x2000   bootloader.bin
#   0x8000   partition-table.bin
#   -------- 0x9000-0x15000 NVS: deliberately skipped --------
#   0x16000  ota_data_initial.bin
#   0x20000  <app>.bin
#
# Usage:
#   ./tools/esphome_flash.sh [--device /dev/cu.usbmodemXXXX] [--erase-settings]
#
#   --erase-settings   fall back to the plain factory flash, wiping NVS. Use
#                      when you actually want defaults (or after a partition
#                      table change).
#
# Prefer OTA where possible (`esphome upload <yaml> --device <ip>`): the OTA
# image only touches the app partition and never puts settings at risk.
# Confirmed 2026-08-03: ESPHome OTA to 192.168.37.237 (~8.3s); NVS preserved.

set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH:-}"

ROOT_DIR="$(cd "$(/usr/bin/dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/.esphome/build/esp32-p4-coolroom/build"
VENV_PY="$ROOT_DIR/.venv/bin/python"

DEVICE=""
ERASE_SETTINGS="false"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --device) DEVICE="$2"; shift 2 ;;
    --erase-settings) ERASE_SETTINGS="true"; shift ;;
    *) echo "ERROR: unknown argument '$1'" >&2; exit 2 ;;
  esac
done

if [[ -z "$DEVICE" ]]; then
  # bash 3.2 on macOS has no mapfile/readarray — keep this portable.
  DEVICE="$(ls /dev/cu.usbmodem* 2>/dev/null | /usr/bin/head -1 || true)"
  if [[ -z "$DEVICE" ]]; then
    echo "ERROR: no /dev/cu.usbmodem* found — pass --device explicitly" >&2
    exit 1
  fi
  echo "Using $DEVICE (override with --device)"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "ERROR: no build at $BUILD_DIR — run ./tools/esphome_compile.sh first" >&2
  exit 1
fi

if [[ "$ERASE_SETTINGS" == "true" ]]; then
  echo "WARNING: flashing the factory image — every saved setting will be erased."
  "$VENV_PY" -m esptool --chip esp32p4 --port "$DEVICE" --baud 460800 \
    write_flash 0x0 "$BUILD_DIR/firmware.factory.bin"
  exit $?
fi

# Offsets come from the build's own flasher_args.json so a partition-table
# change can't leave this script writing to stale addresses. The helper also
# refuses outright if any image would reach into the NVS window.
FLASH_ARGS="$("$VENV_PY" - "$BUILD_DIR" <<'PY'
import json, sys, pathlib
build = pathlib.Path(sys.argv[1])
data = json.loads((build / "flasher_args.json").read_text())
NVS_START, NVS_END = 0x9000, 0x15000
parts = []
for off, name in sorted(data["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
    addr = int(off, 16)
    path = build / name
    if not path.exists():
        sys.exit(f"missing image: {path}")
    end = addr + path.stat().st_size
    if addr < NVS_END and end > NVS_START:
        sys.exit(f"refusing to flash {name} at {off}: it overlaps NVS "
                 f"({NVS_START:#x}-{NVS_END:#x}) and would erase saved settings")
    parts.append(f"{off} {path}")
print(" ".join(parts))
PY
)"

echo "Flashing (NVS at 0x9000-0x15000 left intact):"
echo "$FLASH_ARGS" | /usr/bin/tr ' ' '\n' | /usr/bin/paste - - | /usr/bin/sed 's/^/  /'

# shellcheck disable=SC2086 # deliberate word splitting: offset/path pairs
"$VENV_PY" -m esptool --chip esp32p4 --port "$DEVICE" --baud 460800 \
  write_flash $FLASH_ARGS

echo
echo "Done — saved settings preserved. Use --erase-settings to reset to defaults."
