# Active Context — Current Session State

**Date:** 2026-08-06
**Session:** Full closeout for Prescision→P4 control port on `cursor/port-precision-control-c3a7` (PR #1 to main).
**Status:** Closeout in progress / completing — asymmetric band docs aligned; compile/flash/graph/push per checklist.

## Focus

- Carel-style asymmetric compressor band (ON at SP+diff, OFF at SP) + min-run / defrost skip/force-max / alarm clear.
- Documentation consistency for USER_MANUAL / QUICK_START / logic diagrams.
- Do **not** merge PR #1 unless asked. Leave ntfy alone.

## Immediate Next Actions

1. Operator: confirm PR #1 review + merge when ready.
2. End-to-end coolroom control still hardware-gated (RS485 / RTD not on bench).
3. Optional: add missing `check_dashboard_coverage.py` / NVS-safe `esphome_flash.sh` if closeout tooling is standardized later.
