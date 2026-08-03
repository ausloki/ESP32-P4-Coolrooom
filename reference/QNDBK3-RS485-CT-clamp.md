# QNDBK3/RS485 — Split-core CT clamp (optional)

**Product:** Nanjing Qineng **QNDBK3/RS485** (switchable single-phase AC digital current
transformer with RS-485 Modbus RTU output).

**Sources (local):**

| File | Content |
| --- | --- |
| `reference/QNDBK3-RS485-CT-clamp.pdf` | Full vendor manual (copied from operator download) |
| `reference/QNDBK3-RS485-CT-specs.png` | Technical parameters (50 Hz), DC 12 V, rated current options |
| `reference/QNDBK3-RS485-CT-mechanical.png` | Aperture sizes / Power+/Power−/A/B terminals |
| `reference/QNDBK3-RS485-CT-protocol-frame.png` | Modbus frame format + read example from `0x1000` |
| `reference/QNDBK3-RS485-CT-register-table.png` | Register description table (`0x1002` / `0x100B` / `0x100C`) |
| `reference/QNDBK3-RS485-CT-config-examples.png` | Baud / address write examples + alternate current-read example |

---

## Wiring (this coolroom)

| Clamp terminal | Connect to |
| --- | --- |
| **Power+** | **DC 12 V** (same 12 V DIN PSU rail as relay / RTD modules) |
| **Power−** | Common ground (bond with controller GND / other RS485 GND — see `hardware_pins.md`) |
| **A** | RS485 **A** (board header H10 / item 22) |
| **B** | RS485 **B** |

Shares the **same RS485 bus** as the Waveshare relay board and the 2CH PT100 module.
No new GPIO — UART remains **GPIO27 TX / GPIO26 RX @ 9600 8N1**.

## Install intent (coolroom plant)

Clamp the CT on the **whole plant AC feed** that supplies the compressor, evaporator
fans, and this controller — **not** the compressor motor lead alone.

At this plant, expect roughly **~64 W idle** (fans + controller, compressor off) vs
**~190 W when the compressor is running**. Logging that band in SD column `ct_a` is
enough for operators today; a future **run-proof** (confirm compressor actually draws
when the relay is on) can key off idle vs run without changing the wiring. Run-proof
is **not** implemented in firmware yet.



---

## Bus address & baud (coolroom defaults)

| Setting | Coolroom firmware default | Notes |
| --- | --- | --- |
| UART baud | **9600** (existing bus) | Clamp factory default is also **9600** — leave baud code **1** |
| Slave address | **110** (`modbus_ct_address`) | Must **not** collide with relay **1**, RTD **100**, or RTD universal **249** |
| Broadcast | `0xFF` | Used only for address/baud config writes (FC 0x10) |

Factory examples in the manual often use slave **1**, which **collides with the relay
board**. Before hanging the clamp on the live bus, set its ID to **110** (or whatever
`modbus_ct_address` is compiled to) via broadcast write to register `0x100B`.

Baud codes (register `0x100C`): `0`=4800, `1`=9600, `2`=19200, `3`=38400. Do **not**
change the clamp baud away from 9600 unless the controller UART is changed in lockstep
(that would break relay + RTD).

---

## Modbus protocol (summary)

- **RTU**, CRC16 with **LSB first**
- Read: function **0x03**; multi-register write: function **0x10**
- Address range **1–250**; broadcast **0xFF** for config

### Register map (firmware uses these)

| Address | R/W | Meaning | Firmware use |
| --- | --- | --- | --- |
| **`0x1002`** | R | Current sample; **LSB = one decimal place** → Amps = raw / 10 | `ct_clamp_current` (primary) |
| `0x100B` | R/W | Device ID (1–250) | Configure offline / via broadcast — not written by coolroom firmware |
| `0x100C` | R/W | Baud code (default 9600) | Leave at 9600 to match bus |

### Ambiguity: `0x1001` (2 regs) vs `0x1002` (÷10)

The **register-description table** lists current at **`0x1002`** with one decimal place
(range note “0–60000A” in the sheet — treat as 0.1 A resolution, not a claim that this
clamp measures 60 kA).

A separate **worked example** reads **two** holding registers starting at **`0x1001`**
(`01 03 10 01 00 02 …`) and shows a 32-bit payload (`00 00 14 F3`). That may be an
alternate packing or a documentation inconsistency.

**Coolroom firmware prefers the table: FC 03, one register at `0x1002`, divide by 10.**
If live hardware returns nonsense at `0x1002`, re-check against the `0x1001` 32-bit
example before changing the map.

---

## Firmware entities (optional — default **OFF**)

| Entity | Role |
| --- | --- |
| `switch.ct_clamp_enabled` | Master enable (`input_ct_clamp_enabled`, NVS, default false) |
| `sensor.ct_clamp_current` | Amps (NaN when disabled or offline) |
| `binary_sensor.rs485_ct_clamp_online` | Online only when **enabled** and Modbus answers |

When disabled: no CT offline events / voice / ntfy; published current stays NaN.
SD daily CSV column: **`ct_a`** (empty when disabled/offline).
