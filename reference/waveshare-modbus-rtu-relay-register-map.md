# Waveshare Modbus RTU Relay — Register Map & Command Reference
# Source: https://www.waveshare.com/wiki/Modbus_RTU_Relay
# Applicable to: Modbus-RTU-Relay (4-CH, 8-CH variants — same protocol)
# Protocol version: V3 (backwards compatible with V2)

## Default Communication Parameters
- Baud rate: 9600
- Data bits: 8
- Stop bits: 1
- Parity: None (8N1)
- Default slave address: 0x01

## Function Codes Supported
| Code | Description                |
|------|----------------------------|
| 0x01 | Read coil status           |
| 0x03 | Read holding register      |
| 0x05 | Write single coil          |
| 0x06 | Write single register      |
| 0x0F | Write multiple coils       |

## Coil (Relay) Addresses — Function codes 0x01, 0x05, 0x0F
| Address (hex) | Description                      |
|---------------|----------------------------------|
| 0x0000–0x0007 | Channel 1–8 relay on/off         |
| 0x00FF        | All relays (broadcast control)   |
| 0x0100–0x0107 | Channel 1–8 toggle               |
| 0x01FF        | All relays toggle                |
| 0x0200–0x0207 | Channel 1–8 flash ON (delay×100ms)|
| 0x0400–0x0407 | Channel 1–8 flash OFF (delay×100ms)|

### Coil Write Values
| Value  | Meaning       |
|--------|---------------|
| 0xFF00 | Relay ON      |
| 0x0000 | Relay OFF     |
| 0x5500 | Toggle relay  |

## Holding Registers — Function codes 0x03, 0x06
| Address (hex) | Content         | R/W |
|---------------|-----------------|-----|
| 0x2000        | UART parameters (hi byte = parity 0x00/0x01/0x02, lo byte = baud index 0x00–0x07) | R/W |
| 0x4000        | Device address (0x0001–0x00FF) | R/W |
| 0x8000        | Software version (decimal /100 = version, e.g. 0x012C = V3.00) | R   |

### Baud Rate Index (register 0x2000 low byte)
| Value | Baud   |
|-------|--------|
| 0x00  | 4800   |
| 0x01  | 9600   |
| 0x02  | 19200  |
| 0x03  | 38400  |
| 0x04  | 57600  |
| 0x05  | 115200 |
| 0x06  | 128000 |
| 0x07  | 256000 |

## Example Raw Commands (address 0x01, relay 0-indexed)

```
# Single relay control
Relay 0 ON:   01 05 00 00 FF 00 8C 3A
Relay 0 OFF:  01 05 00 00 00 00 CD CA
Relay 1 ON:   01 05 00 01 FF 00 DD FA
Relay 1 OFF:  01 05 00 01 00 00 9C 0A
Relay 2 ON:   01 05 00 02 FF 00 2D FA
Relay 2 OFF:  01 05 00 02 00 00 6C 0A
Relay 3 ON:   01 05 00 03 FF 00 7C 3A
Relay 3 OFF:  01 05 00 03 00 00 3D CA

# All relays
All ON:       01 05 00 FF FF 00 BC 0A
All OFF:      01 05 00 FF 00 00 FD FA
All toggle:   01 05 00 FF 55 00 C2 AA

# Read all relay status (8 channels)
Send:         01 01 00 00 00 08 3D CC
Response:     01 01 01 <byte> CRC CRC
  — <byte> bit 0 = relay 0, bit 1 = relay 1, etc.

# Set device address to 0x01 (broadcast address 0x00)
              00 06 40 00 00 01 5C 1B
```

## ESPHome Modbus Controller Mapping

In the ESPHome config this project uses:
- `register_type: coil`
- Relay 0 (coil addr 0) → compressor
- Relay 1 (coil addr 1) → defrost
- Relay 2 (coil addr 2) → light
- Relay 3 (coil addr 3) → siren

Write coil via modbus_controller uses function code 0x05 automatically.
Read coil via modbus_controller uses function code 0x01 automatically.

## Temperature Range
-15 °C to +70 °C, 40–85% RH (non-condensing)

## Power
- Supply: 5 V via DC jack or screw terminals (do not use both simultaneously)
- Standby (all relays off): ≈0.18 W
- All relays on: ≈2.9 W
