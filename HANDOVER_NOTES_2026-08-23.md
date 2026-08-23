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

## Incoming hardware

- **RS485 CT clamp — Qineng QNDBK3-10** (Modbus addr **110**) — **expected ~7 Sep 2026**.
  **10 mm** aperture, vendor rated **5–75 A**; same RS485/Modbus map as other QNDBK3 sizes.
  Still not fitted; leave **CT Clamp Enabled** off until installed and wired on H10.
  Verify the plant feed fits **10 mm** before clamping. After fit-up: enable CT, confirm
  Hardware status shows CT online, then exercise CT run-proof settings on bench before
  relying on fail-to-start / stuck-on alarms.

### Address programming (Mac bench, before joining live bus)

Program the clamp to slave **110** @ **9600 8N1** so it does not collide with the
relay (**1**) or RTD (**100**). Factory units often ship at address **1**.

**Tool:** pip package **`modbus-cli`** in repo **`.venv`** — CLI command is **`modbus`**.
From repo root: `source tools/project_env.sh` (or `direnv allow` once) puts it on PATH;
otherwise use `.venv/bin/modbus`. Register map / broadcast write:
`reference/QNDBK3-RS485-CT-clamp.md`.

**Hardware:** USB RS485 adapter on this Mac → clamp **A** / **B** only. Prefer
programming **off the live plant bus** (CT + 12 V + adapter only) so a broadcast
address write cannot disturb the relay or RTD.

```bash
# 1) Find the adapter (typical macOS names)
ls /dev/cu.usb*

# 0) Repo PATH (once per shell)
source tools/project_env.sh

# 2) Optional — read factory address (often 1)
modbus /dev/cu.usbserial-XXXX -b 9600 -P n -s 1 h@0x100B

# 3) Broadcast new slave ID 110 → holding register 0x100B (broadcast = 255 / 0xFF)
modbus /dev/cu.usbserial-XXXX -b 9600 -P n -s 255 h@0x100B=110

# 4) Verify at new address — current register (may read 0 with no AC load)
modbus /dev/cu.usbserial-XXXX -b 9600 -P n -s 110 h@0x1002

# 5) Confirm baud code still 1 (= 9600; do not change unless UART changes)
modbus /dev/cu.usbserial-XXXX -b 9600 -P n -s 110 h@0x100C
```

Then wire onto H10 with relay + RTD, power CT from **12 V**, bond grounds per
`reference/hardware_pins.md`, and only then turn **CT Clamp Enabled** on in firmware.

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
