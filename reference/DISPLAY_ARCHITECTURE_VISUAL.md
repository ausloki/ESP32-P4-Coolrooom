# LVGL Home Page Architecture — Visual Reference
**Build:** 2026-07-18 | ESP32-P4-WIFI6-Touch-LCD-7B | Phase 4

---

## Display Layout Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          ESP32-P4 Home Screen (1024×600)                  │
├────────────────┬──────────────────────────────────────┬──────────────────┤
│                │                                      │                  │
│  LEFT SIDEBAR  │         CENTER METER GAUGE          │  RIGHT SIDEBAR   │
│  (Status      │      (Three Arc Gauges + Text)       │  (Secondary      │
│   Icons)      │                                      │   Readings)      │
│                │                                      │                  │
│  ❄️ x=35,y=50  │  Reference Ring (Light Grey, 30%)  │  x=850           │
│                │     ╱─────────────────────╲        │                  │
│  🔥 x=35,y=130 │    ╱        COOLROOM        ╲       │  Ambient: --°C   │
│    (20pt)      │   │    64pt Blue Font       │       │  y=80            │
│                │   │                          │       │  (15pt small)    │
│  💡 x=35,y=210 │   │  Set: --°C (20pt Cyan)  │       │                  │
│    col_grey    │   │  Running (15pt Orange)  │       │  Evap: --°C      │
│                │    ╲     Inner Cover       ╱       │  y=180           │
│  🔔 x=35,y=290 │     ╲──────────────────────╱        │  (15pt small)    │
│                │                                      │                  │
│  All 20pt      │   ⊙ OUTER:  BLUE Arc (372×372)     │  Heap: ---       │
│  col_grey      │       Coolroom Temp (2s update)    │  y=280           │
│                │   ⊙ MIDDLE: CYAN Arc (322×322)     │  (15pt subtext)  │
│                │       Setpoint Temp (on-change)    │                  │
│                │   ⊙ INNER:  PINK Arc (272×272)     │                  │
│                │       Ambient Temp (10s update)    │                  │
│                │                                      │                  │
│  ╔════════════ ║═════════════════════════════════════║ ════════════════╗│
│  ║ HOME       ║  Donut Ring │ Cover Circle          ║ READINGS        ║│
│  ║ CONTROLS   ║ Dark Border │ (hides arc hubs)      ║ PANEL           ║│
│  ║ (hidden    ║ 60px        │ 270×270, border 18px  ║ (status only)   ║│
│  ║  in this   ║            │                        ║                  ║│
│  ║ view)      ║            │                        ║                  ║│
│  ╚════════════ ║═════════════════════════════════════║ ════════════════╝│
│                │                                      │                  │
└────────────────┴──────────────────────────────────────┴──────────────────┘

FULL LAYOUT with Touch Controls (normally hidden):

┌────────┬──────────────────────────────────────────────┬──────────────────┐
│  LEFT  │  CENTER METER + CONTROLS                     │  RIGHT           │
│ PANEL  │  ┌─────────────────────────────────────┐     │  SIDEBAR         │
│        │  │ ╭─────────────────────────────────╮ │     │                  │
│ Defrost│  │ │    Touch Controls (top center)  │ │     │  Secondary       │
│ Relay  │  │ │ ▲ Increase Setpoint             │ │     │  Readings        │
│ Button │  │ │ (updates cyan arc)              │ │     │  (Read-Only)     │
│        │  │ │                                 │ │     │                  │
│ Comp.  │  │ │ ▼ Decrease Setpoint             │ │     │  Ambient         │
│ Relay  │  │ │ (updates cyan arc)              │ │     │  Evap            │
│ Button │  │ │                                 │ │     │  Heap            │
│        │  │ ╰─────────────────────────────────╯ │     │                  │
│ Light  │  │                                     │     │                  │
│ Relay  │  │ [THREE ARC GAUGES HERE]             │     │                  │
│ Button │  │ Blue outer arc (live coolroom)      │     │                  │
│        │  │ Cyan middle arc (setpoint)          │     │                  │
│ Alarm  │  │ Pink inner arc (ambient)            │     │                  │
│ Status │  │                                     │     │                  │
│        │  └─────────────────────────────────────┘     │                  │
│        │                                              │                  │
└────────┴──────────────────────────────────────────────┴──────────────────┘
```

---

## Arc Gauge Rendering Order (Z-Order)

```
Layer 5 (Top):    Text Labels
                  ├─ lbl_coolroom_temp_large (64pt blue)
                  ├─ lbl_setpoint_status (20pt cyan)
                  └─ lbl_status_text (15pt orange)

Layer 4:          Inner Cover Circle (270×270, hides arc hubs)
                  └─ bg_color: col_bg, border: 18px col_panel

Layer 3:          Inner Arc (Pink, #FF1493)
                  └─ home_ambient_arc (272×272, arc_width: 10px)

Layer 2:          Middle Arc (Cyan, #00BCD4)
                  └─ home_setpoint_arc (322×322, arc_width: 12px)

Layer 1:          Outer Arc (Blue, #0A84FF)
                  └─ home_temp_arc (372×372, arc_width: 16px)

Layer 0:          Reference Ring (Light Grey, 30% opacity)
                  └─ Decorative arc (375×375, arc_width: 15px)
                      bg_color: col_subtext, arc_opa: 30%

Background:       Container (col_bg #1C1C1E)
```

**Rendering Note:** LVGL renders in widget order, so inner arcs must be defined after outer arcs in YAML for correct z-order display.

---

## Temperature-to-Arc Conversion

```
Physical Scale:        Arc Display Scale:
   15°C ─────────         100% ────────
         │                    │
         │                    │  
    0°C ─ TYPICAL OP         57% ─ TYPICAL OP
         │                    │
         │                    │
  -20°C ─────────         0% ─────────

Formula: percent = (int)((temp_c + 20) / 35 * 100)

Example Values:
  -20°C → 0%    (gauge at start)
  -15°C → 14%
  -10°C → 29%
   -5°C → 43%
    0°C → 57%   (typical operating point)
    5°C → 71%
   10°C → 86%
   15°C → 100%  (gauge at end)

Clamping: Values <0% display as 0%, >100% display as 100%
          LVGL arc widget enforces this automatically
```

---

## Update Timing Diagram

```
Timeline (milliseconds):

┌─ 0 ms ────────────────────────────────────────────────────────────┐
│                                                                     │
│  ❄ probe1_temp on_value (every 2000 ms)                           │
│  │  ├─ p4_rtd_valid() check                                       │
│  │  ├─ Calculate: value = (int)((x + 20) / 35 * 100)             │
│  │  ├─ lvgl.arc.update(home_temp_arc, value)                     │
│  │  └─ lvgl.label.update(lbl_coolroom_temp_large, text)           │
│  │                                                                  │
│  ├─ 2000 ms ──────────────────────────────────────────────┐       │
│  │  └─ probe1_temp update fires again                     │       │
│  │                                                         │       │
│  │  🔥 probe3_temp on_value (every 10000 ms)             │       │
│  │  │  ├─ p4_rtd_valid() check                           │       │
│  │  │  ├─ Calculate: value = (int)((x + 20) / 35 * 100)  │       │
│  │  │  ├─ lvgl.arc.update(home_ambient_arc, value)      │       │
│  │  │  └─ lvgl.label.update(lbl_ambient_temp_large, ...) │       │
│  │  │                                                      │       │
│  │  ├─ 4000 ms ───────────────────────────────────┐      │       │
│  │  │  └─ probe1_temp update fires again          │      │       │
│  │  │                                              │      │       │
│  │  ├─ 6000 ms ───────────────────────────────────┐      │       │
│  │  │  └─ probe1_temp update fires again          │      │       │
│  │  │                                              │      │       │
│  │  ├─ 8000 ms ───────────────────────────────────┐      │       │
│  │  │  └─ probe1_temp update fires again          │      │       │
│  │  │                                              │      │       │
│  │  ├─ 10000 ms ──────────────────────────────────┼──────┼──┐    │
│  │  │  ├─ probe1_temp update (5th time)           │      │  │    │
│  │  │  └─ probe3_temp update (1st time) ─────────────────┘  │    │
│  │  │                                              │         │    │
│  │  └─ User changes setpoint (EVENT-DRIVEN, not timed) ─────┤    │
│  │     ├─ id(ctl_setpoint) = new_value                      │    │
│  │     ├─ sync NVS preferences                              │    │
│  │     ├─ Calculate: value = (int)((x + 20) / 35 * 100)     │    │
│  │     ├─ lvgl.arc.update(home_setpoint_arc, value)         │    │
│  │     └─ lvgl.label.update(lbl_setpoint_status, "Set: ...") │    │
│  │                                                           │    │
│  └───────────────────────────────────────────────────────────┘    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

Key Points:
• probe1_temp fires every 2s (fast, live reading)
• probe3_temp fires every 10s (slower, ambient reading)
• setpoint fires on-demand when user adjusts (no periodic update)
• All updates are asynchronous; LVGL renders new widget state immediately
```

---

## Color & Typography Reference

### Color Palette
```
Arcs:
  home_temp_arc      → col_blue     (#0A84FF)  snowflake blue
  home_setpoint_arc  → col_cyan     (#00BCD4)  aqua/cyan
  home_ambient_arc   → col_pink     (#FF1493)  deep magenta

Text:
  Temperature        → col_blue     (#0A84FF)  64pt Bold
  Setpoint label     → col_cyan     (#00BCD4)  20pt Regular
  Status message     → col_orange   (#FF9F0A)  15pt Regular
  Icons (inactive)   → col_grey     (#888888)  20pt Regular

Containers:
  Background         → col_bg       (#1C1C1E)  dark
  Panels/rings       → col_panel    (#2C2C2E)  slightly lighter
  Reference ring     → col_subtext  (#888888)  30% opacity

Decorative:
  Donut ring border  → col_panel    (#2C2C2E)  60px width
  Inner cover        → col_bg       (#1C1C1E)  solid background
  Outer reference    → col_subtext  (#888888)  20% opacity
```

### Typography
```
Font Family: Roboto (from gfonts://Roboto)

Sizes:
  64pt  →  lbl_coolroom_temp_large     (main temperature)
  20pt  →  lbl_setpoint_status         (setpoint value)
  15pt  →  lbl_status_text              (status message)
           lbl_ambient_temp_large       (secondary readings)
           lbl_evap_temp_large
           lbl_power_info
  20pt  →  ui_*_icon                    (status emoji icons)

All fonts are non-bold except main temperature (64pt uses Bold weight if available).
```

---

## Widget Hierarchy (YAML Structure)

```
lvgl:
  screens:
    home_page:
      widgets:
        # Hidden off-screen LED container (status tracking)
        - obj: (x: -50, y: -50)
          └─ 4 LED widgets (not shown on-screen)

        # Left sidebar: Relay control buttons
        - obj: (x: 0, y: 56)  [btn_defrost]
          └─ on_click: relay_defrost.toggle()
        - obj: (x: 0, y: 174)  [btn_compressor]
          └─ on_click: relay_compressor.toggle()
        - obj: (x: 0, y: 292)  [btn_light]
          └─ on_click: relay_light.toggle()
        - obj: (x: 0, y: 410)  [btn_alarm]
          └─ on_click: (read-only status)

        # Center container (meter gauge)
        - obj: (x: 100, y: 56, w: 912, h: 496)
          └─ widgets:
             ├─ arc: decorative outer ring (light grey, 30%)
             ├─ obj: donut separator (dark ring)
             ├─ arc: home_temp_arc (blue, outer)
             ├─ arc: home_setpoint_arc (cyan, middle)
             ├─ arc: home_ambient_arc (pink, inner)
             ├─ obj: inner_cover_circle (hides arc centers)
             ├─ label: lbl_coolroom_temp_large (64pt blue)
             ├─ label: lbl_setpoint_status (20pt cyan)
             ├─ label: lbl_status_text (15pt orange)
             ├─ label: ui_compressor_icon (20pt emoji)
             ├─ label: ui_defrost_icon
             ├─ label: ui_light_icon
             └─ label: ui_alarm_icon

        # Right sidebar: Secondary readings (read-only)
        - obj: (x: 850, y: 56)  [right_readings]
          └─ widgets:
             ├─ label: lbl_ambient_temp_large
             ├─ label: lbl_evap_temp_large
             └─ label: lbl_power_info
```

---

## Phase Progression Diagram

```
┌─────────────┐
│  PHASE 1    │  WiFi, HA API, Web Server, OTA
│  ✅ DONE    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  PHASE 2    │  RS485 Modbus (relays, RTD sensors, RTC)
│  ✅ DONE    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  PHASE 3    │  Control Logic (hysteresis, defrost scheduling, alarms)
│  ✅ DONE    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  PHASE 4    │  ⏳ LVGL Touchscreen Dashboard (IN PROGRESS)
│  ⏳ BUILD OK │    ├─ Meter-based gauge display        ✅ IMPLEMENTED
│  ⏳ TESTING │    ├─ Three arc indicators               ✅ IMPLEMENTED
│             │    ├─ Real-time sensor updates         ✅ IMPLEMENTED
│             │    ├─ Touch relay controls             ✅ INTEGRATED
│             │    └─ Device deployment                ⏳ PENDING
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  PHASE 5    │  🔄 SD Card Logging & Notifications (PLANNED)
│  🔄 PLANNED │    ├─ Event logging to SD card
│             │    ├─ ntfy push notifications
│             │    ├─ Backup/restore configuration
│             │    └─ Web UI log export
└─────────────┘

Build Status (as of 2026-07-18 21:40 UTC+8):
  RAM:   19.3% (111,250 / 576,464 bytes) ✅ Healthy headroom
  Flash: 20.1% (1,477,510 / 7,340,032 bytes) ✅ Comfortable margin
  Warnings: 0
  Errors: 0
```

---

## Known Issues & Workarounds

| Issue | Impact | Workaround | Status |
|-------|--------|-----------|--------|
| Line widget no dynamic update | Setpoint needle can't update | Use cyan arc for visual reference | ✅ WORKING |
| Icon color dynamics not wired | Icons always grey (inactive look) | Requires lambda color updates | 🔄 FUTURE |
| No arc animation easing | Value changes instantly | Smooth enough for UI; low priority | ✅ ACCEPTABLE |

---

## Animated State Visual Spec

This section defines the approved background-animation language for state feedback on the home display. The goal is to add motion without compromising readability or control responsiveness.

### Animation Principles

- Animations must remain behind the temperature meter, labels, and status icons.
- Use subtle motion only; the control UI remains the primary visual layer.
- Prefer low-count, low-opacity accents over dense full-screen motion.
- If the system is in a fault or alarm state, animation intensity should reduce rather than increase.

### Compressor Running: Falling Snowflakes

**Intent:** communicate active cooling without distracting from the center gauge.

**Visual Treatment:**

- 3 to 6 small snowflakes drift downward at slow speed.
- Snowflakes appear in the background field around the center meter.
- Motion should be gentle and continuous, not jittery or random.
- Color palette should stay within cool tones, using the existing blue/cyan family where possible.

**Recommended Parameters:**

| Parameter | Target |
| --- | --- |
| Count | 3–6 flakes |
| Opacity | Low to medium |
| Speed | Slow |
| Direction | Downward, slight lateral drift allowed |
| Layer | Background only |

### Defrost Active: Flickering Flame Border

**Intent:** communicate heat/defrost activity using the outer frame of the display.

**Visual Treatment:**

- A thin orange/red glow appears around the display or the main meter frame.
- Border flicker should be soft and irregular, like a subtle flame shimmer.
- The effect should not overpower the center temperature or alarm text.
- If compressor animation is also active, defrost visuals take priority.

**Recommended Parameters:**

| Parameter | Target |
| --- | --- |
| Border width | Thin to moderate |
| Opacity | Low |
| Flicker speed | Slow, irregular |
| Color family | Orange/red |
| Layer | Outer frame accent |

### State Priority

| Priority | State | Visual Behavior |
| --- | --- | --- |
| 1 | Fault / alarm | Suppress most motion; keep alert UI clear |
| 2 | Defrost active | Flame border flicker enabled |
| 3 | Compressor running | Falling snowflakes enabled |
| 4 | Idle / stable | Static background |

### Implementation Guidance

- Use LVGL object animation or frame-swapped assets for the moving elements.
- Keep the animation layer separate from the existing text and arc widget hierarchy.
- Avoid large MP4 backgrounds for the core status view unless used only as a decorative panel.
- If both effects are used, they should be easy to disable during tuning or low-resource modes.

---

**Last Updated:** 2026-07-18 21:40 UTC+8  
**Build ID:** 0x8d2fcea9  
**Device:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
