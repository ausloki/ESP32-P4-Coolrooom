# Carel / control decisions (2026-08-01)

Product choices vs a Carel IR33-style baseline. Operator detail lives in the manuals;
this file is the decision log.

| Item | Decision | Where operators configure it |
|------|----------|------------------------------|
| **A** Symmetric vs asymmetric hysteresis | **Asymmetric** — ON at `setpoint + differential`, OFF at `setpoint` (Carel-style; room runs at/above SP on average). | User Manual §4.1 · Quick Start §5 |
| **B** Compressor min ON-time | **Already implemented** (`Compressor Min Run Time`, default **2 min**). | User Manual §4.1 · Quick Start worked example |
| **D** Door open → hold compressor | **Implemented** as opt-in **Hold Compressor While Door Open** (default **off**). | Touchscreen Door (6/8) · Web **Door** · User Manual §4.6 |
| **C** Fan control | **Implemented** (Modbus **coil 0** = Fan Relay, enable default **off**). **Plant today:** evaporator fans run **continuously** (hardwired); keep enable off. Coil 0 is reserved so fans can later be moved onto controller control (follow compressor; off during defrost + drip) without redesign. Defrost is always **passive**. | Touchscreen Compressor (1/8) · Web **Compressor** + **Hardware** · User Manual §4.13 |

## Setup implications

1. **Do not expect coil 0 to run the fans today** — they are continuous hardwired. Leave
   **Fan Relay Enabled** off. When/if you rewire fans through coil 0, confirm it is a fan
   (not a heater) before enabling (default off also protects against a miswired heater).
2. Leave **Hold Compressor While Door Open** off unless the door reed is reliable — a
   stuck-open reed would starve cooling for as long as it reads open.
3. Passive defrost still runs the full cycle state machine (timer, early termination,
   drip, flame UI, logging); only the heater output is absent on this controller.

See also: `reference/USER_MANUAL.md`, `reference/QUICK_START_GUIDE.md`,
`reference/control_logic_ns_diagram.md`, `reference/waveshare-modbus-rtu-relay-register-map.md`.
