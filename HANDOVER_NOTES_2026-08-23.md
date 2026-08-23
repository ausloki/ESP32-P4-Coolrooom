# Handover Notes — 2026-08-23

**Branch:** `cursor/wifi-sdmmc-slot-fix`  
**Status:** Site incident logged — **no firmware change yet**; verify when operator confirms device is back online.

## Incident — ~18:18 local (2026-08-23)

Operator received a burst of **ntfy** alerts (likely during or just after a **power-off / power-on** event; device was not reachable for live diagnosis at log time):

| Alert (approx.) | Meaning |
| --- | --- |
| RTD fault — RTD board probe failed at configured address **100** | Modbus RTD board (2CH PT100) not responding or returning invalid data |
| Critical probe fault alarm — both coolroom and backup sensors failed | Probe-fault latch active; control safety path engaged |
| Probe sensor fault — Probe1 data invalid (NaN) | CH1 coolroom reading stale/invalid; check wiring/sensor health |

**Hypothesis:** RS485 / RTD not ready immediately after boot (or bus power lost with mains). Same class of nuisance alert as cold-start Modbus timeouts — **not necessarily** a permanent hardware fault.

## When back online (on-site checklist)

- [ ] Confirm device reachable (web dashboard / ping last known IP).
- [ ] **Hardware status** page: RS485 relay addr **1** and RTD addr **100** both **online**.
- [ ] Home gauge: coolroom (Probe 1) and evaporator (Probe 2) show plausible temps, not `--` / NaN.
- [ ] **Events** log (web or SD): look for `TEMP_BOARD_OFFLINE` / `TEMP_BOARD_ONLINE` around the incident time; note whether recovery was automatic.
- [ ] If RTD stays offline after several minutes: check H10 RS485 wiring, RTD board power, and addr **100** DIP/jumper (see relay/RTD refs under `reference/`).
- [ ] If temps recover but alerts repeat on every reboot: treat **startup ntfy gate** (below) as the fix to implement.

**Do not reflash for diagnosis** unless chasing a firmware bug — prefer reboot; serial `esphome upload` wipes NVS.

## Follow-up work (deferred until on-site)

### Startup ntfy gate

**Startup Alarm Grace Floor** (`ctl_startup_grace_min`, default **15 min**) already suppresses **high/low/no-cool/ice** alarms and their ntfy pushes during boot pulldown. It does **not** currently gate:

- **Probe fault** ntfy (`ntfy_probe_fault_request` in `p4_ctl_tick.h`)
- **Hardware offline** ntfy (RTD board, relay board, I2C humidity/ambient)

Those paths fire as soon as Wi-Fi is up and ntfy is enabled, even while Modbus/I2C is still coming online after power cycle.

**Desired behavior (operator request):** Hold probe-fault and hardware-offline ntfy until startup grace completes (or until the relevant bus has been online at least once post-boot), so power events do not spam critical alerts.

**Implementation sketch (when ready):** In the ntfy block of `p4_ctl_tick.h`, require `!in_grace` (and/or a “bus seen online since boot” latch) before triggering probe-fault and `ntfy_*_board_offline` / sensor-offline requests; keep SD event logging as-is or gate similarly per product decision.

## Code refs

- Startup grace: `p4_ctl_startup_grace_active()` — `p4_control.h`; used for temp alarms in `p4_ctl_tick.h` (`if (!fault && !in_grace)`).
- Ungated ntfy: probe fault ~L762; RTD offline ~L828–854 — `p4_ctl_tick.h`.
- Manual: `reference/USER_MANUAL.md` §4.5 **Startup Alarm Grace Floor** (temp alarms only; probe fault intentionally still runs).
