# Control Logic — Nassi-Schneiderman (NS) Diagram
# ESP32-P4 Coolroom Controller — Phases 3 & 4

**Phase 3:** Control logic (10s interval loop)  
**Phase 4:** LVGL touchscreen display (real-time sensor updates)

---

## Phase 3: Control Loop (10 s interval)

Each `10s interval` tick runs the full control loop in sequence.
All decisions are made in `p4_control.h` C++ functions; YAML lambda orchestrates.

Network behavior is optional:
- Wi-Fi and HA connectivity do not gate compressor/defrost/alarm logic.
- `wifi.reboot_timeout` and `api.reboot_timeout` are set to `0s` to avoid connectivity-triggered reboots.
- ntfy push delivery is only attempted when Wi-Fi is connected.

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
│  ┌────────── ctl_defrost_active == false ───────────┐                       │
│  │  p4_ctl_defrost_due(last_end_ms, interval_ms)?   │                       │
│  │  ┌──── YES ────────────────────────────────────┐ │                       │
│  │  │ relay_compressor → OFF                      │ │                       │
│  │  │ ctl_defrost_active     = true               │ │                       │
│  │  │ ctl_defrost_on_since_ms = millis()          │ │                       │
│  │  │ relay_defrost → ON *only if relay enabled*  │ │                       │
│  │  └─────────────────────────────────────────────┘ │                       │
│  │  Auto starts (interval / smart / dew) require    │                       │
│  │  input_defrost_enabled. Manual start always OK.  │                       │
│  └──────────────────────────────────────────────────┘                       │
│  ┌────────── ctl_defrost_active == true ────────────┐                       │
│  │  p4_ctl_defrost_timeout(on_since_ms, max_ms)?    │                       │
│  │  (or evap reached ctl_defrost_end_c)             │                       │
│  │  ┌──── YES ────────────────────────────────────┐ │                       │
│  │  │ ctl_defrost_active      = false             │ │                       │
│  │  │ ctl_defrost_on_since_ms = 0                 │ │                       │
│  │  │ relay_defrost           → OFF               │ │                       │
│  │  │ ctl_defrost_last_end_ms = millis()          │ │                       │
│  │  └─────────────────────────────────────────────┘ │                       │
│  └──────────────────────────────────────────────────┘                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Passive defrost — cycle state is not the coil

`ctl_defrost_active` is the single source of truth for "a defrost cycle is running".
The relay is an *output* of that state, not the state itself:

| Situation | `ctl_defrost_active` | `relay_defrost` | Flame icon / FX |
|---|---|---|---|
| Normal defrost, relay enabled | true | ON | animated |
| **Passive defrost** — Defrost Relay Enabled off, or heat supplied outside this controller | true | held OFF | **animated** |
| Relay re-enabled mid-cycle | true | picks up ON at next tick | animated |
| Idle | false | OFF | grey, still |

Why it matters beyond cosmetics: the cycle-end test (`p4_ctl_defrost_timeout` /
evap termination) used to read the coil. With the relay disabled, the coil never
read back as on, so the cycle could never terminate and the start condition
re-fired every tick. Compressor hold-off, drip phase, duration/termination
timing, countdown sensors and `DEFROST_START` / `DEFROST_END` logging all key off
`ctl_defrost_active` for the same reason.

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
| `ctl_defrost_active`    | bool     | Defrost cycle running (relay-independent — see Passive defrost) |
| `ctl_defrost_on_since_ms`| uint32_t| millis() when the defrost cycle started; 0 when idle |
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

---

## Phase 4: LVGL Home Page Display (Real-Time Updates)

**Architecture:** Three concentric horseshoe arc gauges with real-time temperature visualization  
**Update Rates:** Probe1 2s, Probe3 10s, Setpoint on-change  
**Layout:** Left sidebar (status icons) + center meter + right sidebar (secondary readings)

The background motion layer is driven from the same 1s LVGL update path as the clock and diagnostics. It reads compressor/defrost/alarm flags, increments a small animation counter, and then updates the bottom-layer snowflake and flame widgets. Fault or alarm states suppress the motion by hiding those widgets.

### Display Loop Execution

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROBE1 TEMP UPDATE (every 2s)                                              │
│  ├─ on_value trigger                                                         │
│  ├─ VALIDATION: p4_rtd_valid(x)?                                             │
│  │  ├─ YES: calculate arc value = (int)((x + 20) / 35 * 100)%               │
│  │  │       update home_temp_arc widget                                      │
│  │  │       update lbl_coolroom_temp_large label                             │
│  │  └─ NO:  arc value = 0, label = "--°C"                                   │
│  └─ Update interval: 2000 ms                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│  EXTERNAL TEMP UPDATE (every 30s, SHT20 I2C — probe_external_temp)          │
│  ├─ on_value trigger                                                         │
│  ├─ VALIDATION: isnan(x)? (NaN when disabled or sensor missing)             │
│  │  ├─ NO:  calculate arc value = (int)((x + 20) / 35 * 100)%               │
│  │  │       update home_ambient_arc widget                                   │
│  │  │       update lbl_ext_humidity_large label (combined temp+RH)          │
│  │  └─ YES: arc value = 0                                                    │
│  └─ Update interval: 30000 ms                                               │
│  (formerly probe3_temp / dedicated ambient RTD board — decommissioned;      │
│   see reference/hardware_pins.md)                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│  SETPOINT NUMBER CHANGE (on user interaction)                               │
│  ├─ set_action trigger                                                       │
│  ├─ STORE: id(ctl_setpoint) = x, sync NVS preferences                       │
│  ├─ CALCULATE: arc value = (int)((x + 20) / 35 * 100)%                     │
│  ├─ UPDATE: home_setpoint_arc widget                                         │
│  ├─ FORMAT: snprintf "Set: %.1f°C"                                           │
│  ├─ UPDATE: lbl_setpoint_status label                                        │
│  └─ No periodic rate; event-driven                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│  TEMPERATURE-TO-ARC CONVERSION                                              │
│  value = (int)((temp_celsius + 20.0f) / 35.0f * 100.0f)                    │
│                                                                              │
│  Mapping:  -20°C → 0%    (gauge start)                                      │
│            0°C  → 57%    (typical operating point)                          │
│            15°C → 100%   (gauge end)                                        │
│                                                                              │
│  Clamping: value is automatically clamped to 0–100% by LVGL arc widget     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Arc Gauge Specifications

| Arc ID | Color | Width (px) | Size | Purpose | Update Rate |
|--------|-------|-----------|------|---------|-------------|
| `home_temp_arc` | Blue #0A84FF | 372×372 | 16px | Coolroom temp (outer) | 2s (probe1) |
| `home_setpoint_arc` | Cyan #00BCD4 | 322×322 | 12px | Setpoint (middle) | on-change |
| `home_ambient_arc` | Pink #FF1493 | 272×272 | 10px | Ambient temp (inner) | 30s (probe_external_temp, SHT20 I2C) |

All arcs:
- **Rotation:** 225° start angle, 270° sweep (horseshoe)
- **Transparency:** `indicator { arc_opa: TRANSP }`, `knob { bg_opa: TRANSP }`
- **Value range:** 0–100% (representing -20°C to 15°C scale)
- **Background:** Transparent (`bg_opa: TRANSP`)

### Center Display Labels

| Label ID | Font | Color | Content | Update Trigger |
|----------|------|-------|---------|-----------------|
| `lbl_coolroom_temp_large` | 64pt Bold | Blue | Temperature (--°C) | probe1 2s |
| `lbl_setpoint_status` | 20pt | Cyan | Setpoint (Set: --°C) | setpoint change |
| `lbl_status_text` | 15pt | Orange | Status ("Running") | control loop event |

### Sidebar Elements

**Left Sidebar (x=35)** — Status Icons
- y=50: ❄️ Compressor (20pt medium, col_grey)
- y=130: 🔥 Defrost (20pt medium, col_grey)
- y=210: 💡 Light (20pt medium, col_grey)
- y=290: 🔔 Alarm (20pt medium, col_grey)

**Right Sidebar (x=850)** — Secondary Readings
- y=80: Ambient temperature (15pt small font)
- y=180: Evaporator temperature (15pt small font)
- y=280: Heap memory info (15pt subtext color)

### Background Animation State Feedback

The home display may use a restrained background animation layer to reinforce the current control state. This does not change the control logic; it only reflects it visually.

| Control State | Visual Feedback | Priority |
|---|---|---|
| Compressor ON | Falling snowflakes drifting behind the meter | Lower than labels/icons |
| Defrost active | Flickering flame border around the display frame | Overrides compressor animation |
| Fault / alarm active | Reduce or suppress motion | Highest priority |

Guidance:

- Keep motion behind labels, icons, and arcs.
- Use low-count, low-opacity accents so the control UI remains readable.
- Treat animation as feedback only; the control decisions remain in `p4_control.h`.

### Color Palette (Phase 4 Additions)

```yaml
col_blue:     #0A84FF  # Coolroom arc, snowflake indicator
col_cyan:     #00BCD4  # Setpoint arc
col_pink:     #FF1493  # Ambient arc
col_grey:     #888888  # Inactive status icons
col_bg:       #1C1C1E  # Panel background
col_panel:    #2C2C2E  # Card surface (donut ring)
col_text:     #FFFFFF  # Primary text
col_subtext:  #888888  # Secondary text / reference ring
col_orange:   #FF9F0A  # Status messages
col_green:    #30D158  # Compressor active
col_red:      #FF453A  # Alarm active
```

### Widget Hierarchy

```
home_page (screen)
  ├─ obj: left_sidebar_buttons (x=0, y=56)
  │  ├─ btn_defrost (relay control)
  │  ├─ btn_compressor
  │  ├─ btn_light
  │  └─ btn_alarm (status only)
  │
  ├─ obj: center_meter_container (x=100, y=56)
  │  ├─ arc: outer_reference_ring (decorative, grey)
  │  ├─ obj: donut_separator (dark border ring)
  │  ├─ arc: home_temp_arc (blue, outer)
  │  ├─ arc: home_setpoint_arc (cyan, middle)
  │  ├─ arc: home_ambient_arc (pink, inner)
  │  ├─ obj: inner_cover_circle (hides arc hubs)
  │  ├─ label: lbl_coolroom_temp_large (64pt)
  │  ├─ label: lbl_setpoint_status (20pt)
  │  └─ label: lbl_status_text (15pt)
  │
  └─ obj: right_sidebar_readings (x=900, y=56)
     ├─ label: lbl_ambient_temp_large
     ├─ label: lbl_evap_temp_large
     └─ label: lbl_power_info
```

### Icon Visual Feedback Loop (Real-Time)

Status icons (left sidebar) update their color dynamically based on control state:

```
RELAY-BOUND ICONS (Hardware State):
├─ relay_compressor.state change
│  ├─ on_turn_on  → ui_compressor_icon.text_color = col_green
│  └─ on_turn_off → ui_compressor_icon.text_color = col_grey
│
└─ relay_light.state change
   ├─ on_turn_on  → ui_light_icon.text_color = col_orange
   └─ on_turn_off → ui_light_icon.text_color = col_grey

CONTROL LOGIC-BOUND ICONS (Software State):
├─ defrost_mode_active (binary sensor)
│  └─ lambda: ctl_defrost_active
│     ├─ TRUE  → ui_defrost_icon animated + col_red (50ms FX lambda)
│     └─ FALSE → ui_defrost_icon static + col_grey
│
└─ any_alarm_active (binary sensor)
   └─ lambda: ctl_alarm_high_active || ctl_alarm_low_active
      ├─ TRUE  → ui_alarm_icon.text_color = col_red
      └─ FALSE → ui_alarm_icon.text_color = col_grey
```

**Icon State Table:**

| Icon | ID | Color | State | Bound To |
|------|-----|--------|-------|-----------|
| ❄️ Compressor | ui_compressor_icon | Green | Relay ON | relay_compressor |
| ❄️ Compressor | ui_compressor_icon | Grey | Relay OFF | relay_compressor |
| 🔥 Defrost | ui_defrost_icon | Red, flickering | Control active (incl. passive) | ctl_defrost_active |
| 🔥 Defrost | ui_defrost_icon | Grey | Control idle | !ctl_defrost_active |
| 💡 Light | ui_light_icon | Orange | Relay ON | relay_light |
| 💡 Light | ui_light_icon | Grey | Relay OFF | relay_light |
| 🔔 Alarm | ui_alarm_icon | Red | Any alarm | ctl_alarm_high/low_active |
| 🔔 Alarm | ui_alarm_icon | Grey | No alarm | !any alarm |

**Key Design Principle:**
- **Relay-bound:** Compressor & Light icons reflect hardware relay state (can enable/disable via relay settings)
- **Control logic-bound:** Defrost & Alarm icons reflect software control state (can trigger via settings without relay)
- This enables passive defrost cycles, soft alarm triggers, and flexible operation modes

**Touch Handlers:**
- Compressor / Defrost icons: **not clickable** — colour and animation follow relay /
  control state only (manual coil toggles removed so taps cannot fight the 10s control tick).
- Light icon: toggle `relay_light` when `ctl_relay_light_enabled` (ignored otherwise).
- Alarm icon: set `ctl_alarm_silenced = true` and freeze `ctl_alarm_silenced_mask`, turn
  off `relay_siren` (soft mute). Bell stops jiggling but stays red. Alarm flags, banners
  and ntfy stay active. Silence clears when every alarm condition is gone, or when a *new*
  condition bit appears that was not in the frozen mask (bell re-animates; siren/speech may
  run again).

### Known Constraints & Workarounds

1. **Line Indicator (Setpoint Needle):** Removed
   - ESPHome LVGL line widget doesn't support dynamic `value` parameter
   - Middle cyan arc provides adequate setpoint visualization
   - Could be re-implemented with alternative pointer/needle widget if needed

2. **Icon Color Dynamics:** ✅ COMPLETE (was planned enhancement)
   - Implemented via relay on_turn_on/off handlers (relay-bound icons)
   - Implemented via binary sensor on_state handlers (control logic-bound icons)
   - All four icons now provide real-time visual feedback

3. **No Arc Animation Timing:** Instant value updates
   - LVGL arc widget updates immediately to new value
   - No built-in easing/animation support in ESPHome LVGL implementation
   - Smooth visual updates depend on LVGL's internal rendering

---

## PHASE SUMMARY TABLE

| Phase | Component | Status | Build Test | Device Test |
|-------|-----------|--------|------------|-------------|
| 1 | WiFi, HA API, Web Server, OTA | ✅ | ✅ | ✅ |
| 2 | RS485 Modbus (relays, RTD, RTC) | ✅ | ✅ | ✅ |
| 3 | Control Logic (hysteresis, defrost, alarms) | ✅ | ✅ | ✅ |
| 4 | LVGL Touchscreen Dashboard | ⏳ | ✅ | ⏳ |
| 5 | SD Card Logging, ntfy, Backup/Restore | 🔄 | — | — |
