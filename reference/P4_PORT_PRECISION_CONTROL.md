# P4 port: Prescision control logic (2026-08-06)

Source: `ausloki/ESP32-Coolroom-Prescision` branch
`cursor/sensors-webgui-improvement-list-c3a7`.

- Provenance patch: `reference/p4_port_precision_control_2026-08-06.patch`
- Applied on `ESP32-P4-Coolrooom` branch `cursor/port-precision-control-c3a7` (from `main`)

## What was ported (passive-defrost P4)

1. **Asymmetric compressor hysteresis** — ON at `SP + diff`, OFF at `SP`
2. **Compressor min-run** — `ctl_comp_min_run_min` (default 2 min)
3. **Skip-if-cold + force-max** — scheduled skip when evap ≤ threshold; force-max safety net (default 720 min)
4. **Alarm clear hysteresis** — latch + persist; clear only after hysteresis recovery

## Apply

```bash
git checkout -b cursor/port-precision-control-c3a7 origin/main
git am reference/p4_port_precision_control_2026-08-06.patch
```

USER_MANUAL §4.1 differential wording updated to match asymmetric thresholds.
