# Control Logic — Nassi-Schneiderman (NS) Diagram
# ESP32-P4 Coolroom Controller — Phase 3

Each `10s interval` tick runs the full control loop in sequence.
All decisions are made in `p4_control.h` C++ functions; YAML lambda orchestrates.

---

## Control Loop (10 s interval)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  READ: coolroom_t = probe1_temp.state                                       │
│  READ: evap_t     = probe2_temp.state                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│  p4_ctl_probe_fault():  !rtd_valid(t) OR sample age > probe_stale_ms?       │
│  ┌──────── YES ──────────────────┐  ┌──────── NO ───────────────────────── │
│  │ ctl_probe_fault = true        │  │ ctl_probe_fault = false               │
│  │ relay_compressor → OFF        │  └──────────────────────────────────────┘│
│  │ relay_defrost    → OFF        │                                           │
│  │ relay_siren      → ON         │                                           │
│  └───────────────────────────────┘                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│  COMPRESSOR CONTROL  (skip if probe_fault OR defrost active)                │
│  p4_ctl_compressor_eval(t, setpoint, diff, compressor_on, last_ms, stale)  │
│  ┌────── result == +1 ──────┐  ┌──── result == -1 ────┐  ┌── result == 0 ─┐│
│  │ relay_compressor → ON    │  │ relay_compressor → OFF│  │  (hold state)  ││
│  └──────────────────────────┘  └──────────────────────┘  └────────────────┘│
├─────────────────────────────────────────────────────────────────────────────┤
│  ALARM EVALUATION                                                           │
│  hi = p4_ctl_alarm_high(t, setpoint, alarm_high_delta)                     │
│  lo = p4_ctl_alarm_low(t, setpoint, alarm_low_delta)                       │
│  ┌────────── hi OR lo OR probe_fault ─────────────┐  ┌───── all clear ────┐│
│  │ ctl_alarm_high/low_active = hi/lo               │  │ clear alarm flags  ││
│  │ relay_siren → ON (if not already)               │  │ relay_siren → OFF  ││
│  └─────────────────────────────────────────────────┘  └────────────────────┘│
├─────────────────────────────────────────────────────────────────────────────┤
│  DEFROST SCHEDULING  (skip if probe_fault OR compressor just commanded ON)  │
│  ┌────────── relay_defrost currently OFF ───────────┐                       │
│  │  p4_ctl_defrost_due(last_end_ms, interval_ms)?   │                       │
│  │  ┌──── YES ────────────────────────────────────┐ │                       │
│  │  │ relay_compressor → OFF                      │ │                       │
│  │  │ relay_defrost    → ON                       │ │                       │
│  │  │ ctl_defrost_on_since_ms = millis()           │ │                       │
│  │  └─────────────────────────────────────────────┘ │                       │
│  └──────────────────────────────────────────────────┘                       │
│  ┌────────── relay_defrost currently ON ────────────┐                       │
│  │  p4_ctl_defrost_timeout(on_since_ms, max_ms)?    │                       │
│  │  ┌──── YES ────────────────────────────────────┐ │                       │
│  │  │ relay_defrost          → OFF                │ │                       │
│  │  │ ctl_defrost_last_end_ms = millis()           │ │                       │
│  │  └─────────────────────────────────────────────┘ │                       │
│  └──────────────────────────────────────────────────┘                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Compressor Hysteresis Band

```
Temperature
    │
    │  ┄┄┄┄ setpoint + alarm_high_delta ┄┄┄┄  ← HIGH ALARM
    │
    │  ──── setpoint + diff/2 ────────────────  ← compressor turns ON
    │                                           (if currently OFF)
    │
    │  ════ SETPOINT ═════════════════════════
    │
    │  ──── setpoint − diff/2 ────────────────  ← compressor turns OFF
    │                                           (if currently ON)
    │
    │  ┄┄┄┄ setpoint − alarm_low_delta  ┄┄┄┄  ← LOW ALARM
    │
```

| Parameter           | Default  | Configurable Via       |
|---------------------|----------|------------------------|
| setpoint            | 2.0 °C   | Number entity / HA     |
| compressor_diff     | 1.0 °C   | Number entity / HA     |
| alarm_high_delta    | 3.0 °C   | Number entity / HA     |
| alarm_low_delta     | 3.0 °C   | Number entity / HA     |
| defrost_interval_h  | 8 h      | YAML substitution      |
| defrost_max_min     | 30 min   | YAML substitution      |
| probe_stale_ms      | 30 000 ms| YAML substitution      |

---

## Relay Mutual Exclusion

```
relay_compressor.write_lambda: if (x) → relay_defrost.turn_off()
relay_defrost.write_lambda:    if (x) → relay_compressor.turn_off()
Control loop:                  if probe_fault → both OFF immediately
```

---

## State Flags (globals)

| ID                      | Type     | Purpose                               |
|-------------------------|----------|---------------------------------------|
| `ctl_setpoint`          | float    | Target temperature (°C), NVS          |
| `ctl_comp_diff`         | float    | Hysteresis band (°C), NVS             |
| `ctl_alarm_high_delta`  | float    | High alarm delta (°C), NVS            |
| `ctl_alarm_low_delta`   | float    | Low alarm delta (°C), NVS             |
| `ctl_probe_fault`       | bool     | True when probe 1 stale/invalid       |
| `ctl_alarm_high_active` | bool     | High temp alarm state                 |
| `ctl_alarm_low_active`  | bool     | Low temp alarm state                  |
| `ctl_defrost_on_since_ms`| uint32_t| millis() when defrost relay turned ON |
| `ctl_defrost_last_end_ms`| uint32_t| millis() when last defrost ended      |
| `hw_rs485_relay_ok`     | bool     | Relay board comms health              |
| `hw_rtc_ok`             | bool     | PCF8563 RTC health                    |
| `ctl_comp_lockout_min`  | float    | Phase 6: off-delay lockout time (min)  |
| `ctl_comp_last_off_ms`  | uint32_t | Phase 6: millis() when compressor last turned OFF |
| `ctl_comp_on_since_ms`  | uint32_t | Phase 6: millis() when compressor last turned ON  |
| `ctl_high_alarm_since_ms`| uint32_t| Phase 6: millis() when hi alarm first detected    |
| `ctl_low_alarm_since_ms` | uint32_t| Phase 6: millis() when lo alarm first detected    |
| `ctl_no_cool_alarm_active`| bool   | Phase 6: no-cool alarm state                      |
| `ctl_ice_alarm_active`  | bool     | Phase 6: ice detection alarm state                |
| `ctl_door_alarm_active` | bool     | Phase 6: door open alarm state                    |
| `ctl_defrost_dripping`  | bool     | Phase 6: drip phase active                        |
| `ctl_sensor_fallback_active`| bool | Phase 6: duty-cycle fallback running              |
| `ctl_manual_defrost_req`| bool     | Phase 6: manual defrost button pressed            |
| `ctl_defrost_delta_since_ms`| uint32_t| Phase 6: smart defrost delta tracking           |
| `input_smart_defrost_enabled`| bool | Phase 6: smart delta defrost enabled             |
| `input_defrost_term_temp_enabled`| bool | Phase 6: terminate defrost by evap temp      |
| `input_defrost_drip_enabled`| bool | Phase 6: enable drip phase after defrost          |
| `input_fallback_enabled`| bool     | Phase 6: sensor fallback duty mode enabled        |
| `input_siren_enabled`   | bool     | Phase 6: siren output enabled                     |
