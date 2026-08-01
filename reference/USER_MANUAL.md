# Coolroom Controller — User Manual

**Applies to:** ESP32-P4 Coolroom Controller firmware (this repository)
**Audience:** Operators and site managers running the coolroom day to day.
**Companion document:** `reference/QUICK_START_GUIDE.md` — condensed setup steps and
recommended-settings worked examples. Read this manual for the full explanation of what
each function does and why; use the Quick Guide when you just need a fast answer.
Product choices vs Carel-style baselines: `reference/CAREL_CONTROL_DECISIONS.md`.

---

## 0. About This Manual

- Every setting in the controller is explained in **plain language**: what it does, what
  happens if you raise or lower it, and why you'd normally leave it alone.
- Settings are grouped exactly as they appear on the touchscreen and web dashboard, so you
  can follow along on the real device while reading.
- Where a control decision is more involved than "turn X on when Y happens", this manual
  includes a **structogram** (Nassi–Shneiderman / NS-style flow diagram) — see §3 for how to
  read them.
- Screenshots of the web dashboard and touchscreen are marked as placeholders throughout —
  they'll be filled in once physical hardware is connected and running.

> 📷 **Screenshot placeholder — Touchscreen Home Screen**
> *(to be added once hardware is connected)*

**Home-screen header (touchscreen):** 12-hour time with AM/PM and seconds on the left;
date on the right as day + short month (e.g. `10 Apr`), just left of the Wi‑Fi strength icon.

**Home-screen gauge (touchscreen):** three concentric arcs sweeping the same dial, from
**−10 °C** at the bottom-left end to **+30 °C** at the bottom-right end. Each arc is drawn
in its sensor's colour so the ring and the matching text readout agree at a glance:

| Ring (outermost first) | Colour | Shows |
|---|---|---|
| Outer | Blue | Current coolroom temperature |
| Middle | Cyan | Setpoint |
| Inner | Pink | Ambient |

A reading colder than −10 °C or warmer than +30 °C sits pinned at the end of its arc — the
numeric readouts remain exact. A sensor that is offline leaves its ring empty. The dial
range is set by the `dial_min_c` / `dial_max_c` substitutions at the top of
`esp32-p4-coolroom.yaml`. The setpoint's minimum is locked to `dial_min_c`, so every
legal setpoint has a visible position on the cyan arc.

**Temperature Display Unit** (Settings 7/8 on LVGL, **Probes & Sensors** on web)
switches touchscreen and web temperature readouts between Celsius and Fahrenheit.
Default is **Celsius**. It is a display preference only: control calculations, stored
settings, logs, and device sensor payloads remain Celsius. The preference is exposed to
Home Assistant as `select.temperature_display_unit`; Home Assistant independently renders
temperature sensors using its configured unit system.

**Home-screen left icons (touchscreen):**

| Icon | Touch? | What it does |
|---|---|---|
| Snowflake (compressor) | No | Status only — spins/colours when the compressor relay is on. Control logic owns the relay; tapping does nothing. |
| Flame (defrost) | No | Status only — animates while defrost is active. |
| Light | Yes | Manual on/off for the cabinet light **when Light Relay Enabled** is on (§4.11). Ignored if that hardware enable is off. Door-Triggered Light (§4.6) can still drive the same relay from the door switch. |
| Bell | Yes | Soft-mutes the siren and speech for the *current* set of alarms. Bell stops jiggling but stays red until those conditions clear. Banners and phone notifications keep going. Mute lifts when every alarm is gone, or immediately if a *new* alarm type appears (bell re-animates). |

**Centre status line** (under the setpoint on the home gauge, and mirrored on the web
gauge) shows one condition at a time. When several faults or offline sensors are active
together, it **rotates** through them about every 2½ seconds so none are hidden behind a
single priority string. Typical fault labels:

| Label | Meaning |
|---|---|
| **RELAY BOARD OFFLINE** | Modbus relay board not responding (compressor cannot run) |
| **TEMP BOARD OFFLINE** | RS485 temperature-probe board not responding |
| **HUMIDITY SENSOR OFFLINE** | Internal cabinet humidity/temp sensor (SHT31) is enabled but not answering |
| **AMBIENT SENSOR OFFLINE** | External/ambient sensor (SHT20) is enabled but not answering |
| **COOLROOM PROBE BAD** | Coolroom control probe missing, stale, or implausible (§4.7) |
| **HIGH TEMP** / **LOW TEMP** / **DOOR OPEN** / **NOT COOLING** / **ICE ON COIL** | Active alarms (§4.4–§4.6) |

With nothing wrong it shows **COOLING**, **DEFROST**, **LOCKOUT**, or **OK**.

> 📷 **Screenshot placeholder — Web Dashboard, guest view**
> *(to be added once hardware is connected)*

---

## 1. Overview — What This Controller Does

The controller keeps a coolroom at a target temperature, runs **passive** defrost on the
evaporator coil on a schedule (compressor held off; optional evaporator fan follows the
compressor when enabled), watches for problems (door left open, sensor failure, no cooling,
ice buildup), and tells you about it — on the touchscreen, on a web dashboard, and via push
notification to your phone.

**Two ways to reach it:**

| Surface | What it's for | Where |
|---|---|---|
| **Touchscreen** (7" LCD on the unit) | Day-to-day monitoring + on-site adjustments, PIN-protected | Physically on the controller |
| **Web Dashboard** | Full monitoring + every setting, from any browser on the network | `http://<device-ip>/assets/dashboard.html` (or `/dashboard`) |

**What it's physically connected to:**

| Component | Role |
|---|---|
| Probe 1 (RTD CH1) | Main coolroom **room air** temperature — everything is measured against this (fixed role) |
| Probe 2 (RTD CH2) | **Evaporator coil** temperature — used for smart defrost and ice detection (fixed role) |
| Internal humidity sensor (SHT31) | Cabinet humidity, informational (see §4.7 — no automatic humidity control) |
| External humidity sensor (SHT20) | Room/ambient humidity, informational |
| Compressor relay | Turns cooling on/off |
| Fan relay | Evaporator fan (coil 0) — follows compressor when Fan Relay Enabled (§4.13) |
| Light relay | Cabinet light — home-screen tap and/or door-triggered (§0 home icons, §4.6) |
| Siren relay | Audible alarm — automatic when Alarm Siren Enabled; mute from home bell (§0, §4.4) |
| Door sensor | Optional — off by default, see §4.6 |
| SD card | Event/temperature logging + settings backup — optional, device runs fine without one |

---

## 2. Getting Started

### 2.1 First connection

1. On first boot with no known WiFi network, the controller starts its own WiFi access
   point. Connect to it and browse to `192.168.4.1` to enter your real WiFi network's name
   and password.
2. Once connected to your network, find the device's IP address (your router's device list,
   or the touchscreen's Info page) and open `http://<device-ip>/` — that is the Coolroom
   dashboard (not ESPHome's stock entity list). Main status, timers, and system health load
   **without** a password. Settings stay hidden until **🔐 Login** in the page header
   with `web_server_username` / `web_server_password` from `secrets.yaml`.
3. After **🔐 Login**, settings and administration unlock. See §4.11.
4. To change WiFi network later, or reconnect after a network change, use the **New WiFi
   SSID / Password** fields in the web dashboard's Network section — this is deliberately
   **web-only**, not available on the touchscreen.

### 2.2 Touchscreen settings PIN

The touchscreen has its own, separate 4-digit PIN (unrelated to the web login above) that
gates the on-screen settings pages. **Factory default PIN: `0000`.**

> Change this on first use. Tap **Settings** → enter the PIN → **Change PIN** button
> on the first settings page.

The touchscreen PIN only unlocks settings shown on the touchscreen. It cannot be used to
log into the web dashboard, and the web dashboard's operator password cannot be used on the
touchscreen — they are two independent locks by design (see §4.11).

> 📷 **Screenshot placeholder — Touchscreen PIN entry keypad**
> *(to be added once hardware is connected)*

### 2.3 Relay wiring and opt-in door/fan features

Before relying on cooling or fan output:

1. Wire the Waveshare 4-CH Modbus relay as **coil 0 = evaporator fan**, **coil 1 =
   compressor**, **coil 2 = light**, **coil 3 = siren**. Defrost on this controller is
   always **passive** (no heater coil) — do not treat channel 1 as a defrost heater.
2. Leave **Fan Relay Enabled** **off** (factory default) until coil 0 is confirmed as a
   fan. Then enable it from touchscreen Settings 1/8, web **Compressor**, or **Hardware**.
   When on, the fan follows the compressor and is forced off during defrost and drip.
3. Door features default **off**: enable **Door Sensor Enabled** only when the reed is
   fitted and **Door Sensor Mode** matches NC/NO wiring. **Hold Compressor While Door Open**
   is separate and also defaults off — turn it on only if you want cooling paused whenever
   the door reads open (§4.6).

Full decision log vs Carel-style controllers: `reference/CAREL_CONTROL_DECISIONS.md`.
Condensed checklist: `reference/QUICK_START_GUIDE.md` §2.

---

## 3. How to Read the Diagrams In This Manual

Where a function makes more than one decision in sequence, this manual uses a
**Nassi–Shneiderman structogram** instead of a flowchart. There are no arrows to trace —
you just read the boxes top to bottom, like a recipe:

- A **plain box** is a step that always happens.
- A **split box** is a decision — read the condition at the top, then follow whichever
  column (usually YES / NO) matches your situation.
- A **box with a shaded top strip** is a loop — everything nested inside it repeats for as
  long as the condition at the top holds.

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Step that always happens                                             │
├──────────────────────────────────────────────────────────────────────┤
│                     Is the condition true?                           │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES — do this                   │ NO — do this instead               │
├─────────────────────────────────┴────────────────────────────────────┤
│ WHILE some condition holds                                           │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │ repeats each cycle until the condition above stops being true   │   │
│ └────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. Function Groups

### 4.1 Temperature Control & Compressor

*Touchscreen: Settings 1/8 — "Compressor & Fallback". Web: **Temperature Control** +
**Compressor** sections.*

This is the core loop: keep the coolroom near a target temperature without short-cycling
the compressor.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Setpoint** | The temperature you want the coolroom to hold. | −10 – 15 °C | 2.0 °C | Floor matches the home-gauge dial (`dial_min_c`). Set to whatever your stored product needs. |
| **Compressor Differential** | The "dead band" around the setpoint. Compressor switches ON at setpoint + half the differential, OFF at setpoint − half. | 0.5 – 10.0 °C | 1.0 °C | Wider = fewer compressor starts (longer compressor life) but more temperature swing. Narrower = tighter temperature control but more frequent cycling. |
| **Compressor Off-Delay (Lockout)** | Minimum time the compressor must stay off before it's allowed to restart, even if the temperature calls for cooling. | 0 – 10 min | 3 min | Protects the compressor motor from rapid restart. Only lower this if your compressor's manufacturer explicitly allows shorter cycling. |
| **Compressor Min Run Time** | Minimum time the compressor must stay ON once started, even if the room has already reached the cut-out temperature. | 0 – 30 min | 2 min | Complements Off-Delay on the ON side. Set 0 to disable. |
| **Fan Relay Enabled** | Whether Modbus coil 0 (evaporator fan) may energise. When on, the fan follows the compressor and is forced off during defrost + drip. | On/Off | **Off** | Same control as §4.13 Hardware → Fan. Confirm coil 0 is a fan before enabling. Also on touchscreen Settings 1/8. |
| **Sensor Fallback Duty-Cycle Enabled** | If the main probe fails, run the compressor on a fixed timer instead of stopping cooling completely. | On/Off | On | Leave on unless you'd rather the room simply stop cooling during a sensor fault (some sites prefer that so staff notice immediately). |
| **Fallback Compressor ON Time** | How long the compressor runs per fallback cycle when the probe has failed. | 1 – 30 min | 3 min | Works together with OFF time below — together they set a safe average duty cycle without real temperature feedback. |
| **Fallback Compressor OFF Time** | How long the compressor rests per fallback cycle when the probe has failed. | 1 – 60 min | 27 min | Default 3 min ON / 27 min OFF ≈ 10% duty cycle — a conservative "keep it cold-ish, don't ice up or overwork the compressor" fallback. |

**How the compressor decides ON vs OFF:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Read Probe 1 (coolroom temperature)                                  │
├──────────────────────────────────────────────────────────────────────┤
│           Is the reading missing, stale, or out of range?            │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES                             │ NO                                 │
│  Force compressor OFF now       │  Compressor currently OFF?         │
│  (safety — see §4.7 Probe Fault)│ ┌───────────────┬─────────────────┐│
│                                 │ │ YES            │ NO              ││
│                                 │ │ Temp above     │ Temp below      ││
│                                 │ │ setpoint +      │ setpoint −      ││
│                                 │ │ half the diff?  │ half the diff?  ││
│                                 │ │  → turn ON      │  → turn OFF     ││
│                                 │ └───────────────┴─────────────────┘│
├─────────────────────────────────┴────────────────────────────────────┤
│ Before turning ON: has it been at least "Compressor Off-Delay"       │
│ since it last turned off? If not, wait — hold current state.         │
│ (A power-up counts as "it last turned off" — see below.)             │
│ Before turning OFF: has Min Run Time elapsed? If not, keep ON.       │
│ If Hold Compressor While Door Open is on and the door is open:       │
│ force compressor OFF (fan follows if Fan Relay Enabled).             │
└──────────────────────────────────────────────────────────────────────┘
```

**After a power cut or restart, the compressor waits.** The off-delay starts counting from
the moment the controller powers up, exactly as if the compressor had just switched off.
A power cut looks the same to the compressor as being switched off, and restarting one
against pressure that hasn't equalised is what the off-delay exists to prevent — so the
room stays uncooled for up to the full Compressor Off-Delay after every restart. The
countdown is shown under the snowflake on the touchscreen home screen and as *Compressor
Lockout Remaining* on the web dashboard, and the snowflake is **amber** while it runs
(grey = idle, blue = running, red = the relay has been disabled in §4.13 **or** the
RS485 relay board is offline). This applies to the fallback duty cycle in §4.7 as
well — its ON window is **held**, not spent, while the off-delay is counting **or**
the relay board is unreachable, so a fallback cycle that comes due during either
condition still gets its full ON Time once both clear rather than being skipped.

**No relay board = no compressor run (and no cooling animation).** Compressor
control is the one loop that requires the Modbus RTU relay module to be online.
Without it, coil writes go nowhere: the compressor stays off, the snowflake stays
red, the home centre status reads **RELAY BOARD OFFLINE**, and the falling-snow FX does
not run. Other diagnostics (probe fault, Wi-Fi, system time) continue as usual.
When the board comes online the normal off-delay / fallback / hysteresis path
resumes. If other faults are active at the same time, the centre status rotates
through them (§0).

> 📷 **Screenshot placeholder — Web Dashboard: Temperature Control & Compressor sections**
> 📷 **Screenshot placeholder — Touchscreen: Settings 1/8**

---

### 4.2 Defrost Schedule

*Touchscreen: Settings 2/8 — "Defrost Schedule" (scroll for Skip-If-Cold / Force-Max). Web: **Defrost** section.*

Frost builds up on the evaporator coil over time and blocks airflow, so the coil needs
periodic defrosting. This group controls the basic on/off timer for that.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Defrost System Enabled** | Master switch for **automatic** defrost (scheduled, smart-delta, dew-point). | On/Off | On | Turning this off stops those automatic starts. **Start Defrost Now** still works for service. Defrost on this controller is always **passive** (compressor held off); coil 0 is the **Fan Relay** (§4.13), not a heater. |
| **Start Defrost Now** *(button)* | Begin a defrost cycle immediately. | — | — | Use for service or when the coil looks iced between schedules. Resets the interval clock from this cycle. Works even during a probe fault (ends by Max Duration if the evaporator reading isn't available) and even when Defrost System Enabled is off. |
| **Stop Defrost Now** *(button)* | Abort the current defrost or drip phase immediately. | — | — | Cooling resumes subject to the compressor off-delay (§4.1). |
| **Defrost Interval** | How often a defrost cycle starts, on a fixed timer. | 60 – 1440 min | 480 min (8 h) | Shorter interval for rooms with heavy door traffic or high humidity (more frost buildup); longer for dry, low-traffic rooms. |
| **Defrost Max Duration** | Safety cap — defrost stops after this long even if it hasn't finished. | 5 – 60 min | 30 min | Raise slightly if defrost is being cut off before the coil is properly clear (check evaporator temp after a cycle). |
| **Defrost Early Termination by Temperature** | Ends defrost as soon as the evaporator reaches the target temp below, instead of always running the full Max Duration. | On/Off | On | Recommended on — avoids unnecessarily warming the room once the coil is already clear. |
| **Defrost Termination Temp** | The evaporator temperature that counts as "coil is clear" when the above is on. | 0.0 – 15.0 °C | 5.0 °C | Only relevant if Early Termination is on. |
| **Defrost Drip-Drain Phase Enabled** | After a passive defrost cycle ends, hold before resuming cooling so melted frost can drain instead of refreezing immediately. | On/Off | On | Recommended on for any room where meltwater could re-ice on a cold coil. |

**Defrost cycle, start to finish:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Manual Start Defrost Now?  → YES: start cycle (even if System off)   │
├──────────────────────────────────────────────────────────────────────┤
│ WHILE Defrost System Enabled  (automatic starts only)                │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │        Has it been "Defrost Interval" since the last defrost    │   │
│ │        ended? (or a Smart/Dew-Point trigger fired — see §4.3)    │   │
│ ├───────────────────────────────┬─────────────────────────────────┤   │
│ │ YES → start defrost            │ NO → keep cooling normally      │   │
│ └───────────────────────────────┴─────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────────┤
│ WHILE defrost is running  (or Stop Defrost Now → abort, skip drip)   │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │ Has Max Duration elapsed, OR (Early Termination is on           │   │
│ │ AND evaporator has reached Termination Temp)?                   │   │
│ │  → YES: end cycle (heater off if it was on)                     │   │
│ └────────────────────────────────────────────────────────────────┘   │
│ If Drip/Drain Phase is on: hold compressor off for the drip           │
│ time (§4.3) before resuming normal cooling.                            │
└──────────────────────────────────────────────────────────────────────┘
```

> 📷 **Screenshot placeholder — Web Dashboard: Defrost section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 2/8**

---

### 4.3 Defrost — Smart & Drip

*Touchscreen: Settings 3/8 — "Defrost Smart & Drip" (scroll for Frost Rate). Web: **Defrost** section (same
group as §4.2 on the web dashboard).*

These add two *extra* ways to trigger a defrost early, on top of the fixed timer in §4.2 —
they never replace the timer, only supplement it.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Defrost Drip Time** | How long to hold before resuming cooling after defrost ends (see §4.2's Drip Phase toggle). | 0 – 30 min | 5 min | Increase for coolrooms with poor drainage or a lot of meltwater. |
| **Smart Defrost (Delta-Triggered)** | Watches the gap between coolroom and evaporator temperature; if it grows too large for too long, starts a defrost early — regardless of the fixed timer. | On/Off | **Off** | Turn on for rooms with variable/heavy loads where frost can build up faster than the fixed interval expects. |
| **Smart Defrost Delta Threshold** | How large the coolroom-to-evaporator gap must get before it counts as "frosted up". | 1.0 – 20.0 °C | 8.0 °C | Lower = triggers more readily (more frequent smart defrosts); higher = more tolerant. |
| **Smart Defrost Dwell Time** | How long that gap must stay above the threshold before triggering — avoids reacting to a brief spike. | 1 – 120 min | 30 min | Raise if smart defrost is triggering on short-lived temperature blips (e.g. right after a door opens). |
| **Dew Point Early Defrost Trigger** | Starts defrost early when the evaporator is below freezing *and* below the air dew point (from the internal SHT31). | On/Off | Off | Enable in humid rooms where frost forms between scheduled cycles. |
| **Defrost Skip-If-Cold** | When a *scheduled* interval is due but the evaporator is already at/below the skip threshold, skip that cycle and roll the timer forward. | On/Off | Off | Saves unnecessary defrost cycles when the coil is already clear/cold. Manual / smart / dew / frost-rate / force-max starts are never skipped. |
| **Skip-If-Cold Below** | Evaporator temperature at or below which a due scheduled defrost is skipped. | −30 – 0 °C | −10 °C | Needs Probe 2. Lower = skip more often. |
| **Defrost Max Interval Override (Force-Max)** | Safety net: force a defrost if this many minutes have passed since the last cycle, even when skip-if-cold keeps postponing. | 720 – 1440 min | 720 (12 h) | Raise toward 24 h only if you intentionally allow long skip stretches. |
| **Frost Rate Monitoring** | Early defrost when humidity drops sharply over a sample window while room temperature stays nearly stable (frost forming). | On/Off | Off | Needs internal SHT31 humidity. Complements dew-point trigger. |
| **Frost Rate Humidity Drop** | Humidity drop (%) over the window that counts as frost forming. | 1 – 20 % | 5 % | Lower = more sensitive. |
| **Frost Rate Window** | Length of the humidity/temperature sample window. | 60 – 3600 s | 300 s | Longer windows ignore brief RH blips. |
| **Dew Point Early Defrost Trigger** | Uses humidity + temperature to calculate the dew point, and starts a defrost the moment the evaporator is actually cold enough for frost to be forming — not just "probably frosted", but confirmed. | On/Off | **Off** | Turn on for humid environments (produce storage, anywhere with high moisture load) — this is the most targeted of the three triggers. |

**Note:** both extra triggers default to **Off**. A brand-new install only defrosts on the
fixed timer (§4.2) until you turn one or both of these on.

> 📷 **Screenshot placeholder — Web Dashboard: Defrost (Smart) fields**
> 📷 **Screenshot placeholder — Touchscreen: Settings 3/8**

---

### 4.4 Alarm Thresholds

*Touchscreen: Settings 4/8 — "Alarm Thresholds". Web: **Alarms** section.*

The basic "something's wrong with the temperature" alarms.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **High Temp Alarm Delta** | How far *above* setpoint the room can drift before a high-temperature alarm fires. | 0.5 – 20.0 °C | 3.0 °C | Tighten for sensitive product, loosen if door traffic causes nuisance alarms. |
| **Low Temp Alarm Delta** | How far *below* setpoint before a low-temperature alarm fires. | 0.5 – 20.0 °C | 3.0 °C | Tighten if freeze-sensitive product is stored (see the plums example in the Quick Guide). |
| **Alarm Persist Time** | How long an alarm condition must hold continuously before the siren/notification actually fires. | 0 – 30 min | 5 min | Prevents nuisance alarms from a brief door-open temperature blip. Raise for high-traffic rooms. |
| **Alarm Siren Enabled** | Master switch for the physical siren relay. | On/Off | On | When on, the siren sounds automatically for any active alarm (past its persist time). Tap the **home-screen bell** to mute it for the current event — the bell stops animating but stays red; banners and notifications keep running. Mute lifts when every alarm has cleared, or when a *new* alarm type appears so that event can sound and the bell re-animates. Turn this switch off if you want the siren permanently quiet. |

> 📷 **Screenshot placeholder — Web Dashboard: Alarms section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 4/8**

---

### 4.5 Alarms — Advanced

*Touchscreen: Settings 5/8 — "Alarms Advanced". Web: **Alarms** section (same group as
§4.4 on the web dashboard).*

Less commonly touched, but important for tuning out false alarms.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Ice Detection Enabled** | Master switch for the evaporator ice alarm. | On/Off | On | Off disables ice detection entirely (useful if Probe 2 is unused). |
| **Ice Alarm Delta** | Alarm when (coolroom − evaporator) stays **at or above** this gap while the compressor runs — a large gap means the coil is iced / starved of airflow (**Precision polarity**). | 5.0 – 30.0 °C | 15.0 °C | Lower for earlier warning; raise if false alarms during normal pull-down. Older firmware used the inverted "small gap" test — that is no longer used. |
| **Ice Alarm Dwell Time** | How long the ice condition must hold continuously before the alarm fires. | 1 – 60 min | 10 min | Any break resets the dwell. Raise to ignore brief spikes. |
| **No-Cool Alarm Timeout** | If the compressor has been running this long without the room actually cooling, something's wrong (stuck compressor, refrigerant leak, blocked airflow) — alarm fires. | 15 – 240 min | 60 min | Tighten for high-value product where a refrigeration failure needs fast attention; loosen for rooms with a naturally slow pulldown. |
| **Startup Alarm Grace Floor** | No alarms fire for this long after the controller boots, giving the room time to reach setpoint from a cold start without nuisance alarms. | 0 – 60 min | 15 min | Raise if your room routinely takes longer than 15 minutes to pull down after a power cycle. |
| **Post-Defrost Alarm Grace** | No alarms fire for this long after a defrost cycle ends, while the room recovers from the temporary warm-up defrost causes. | 0 – 60 min | 20 min | Raise if alarms are triggering right after routine defrosts. |
| **Alarm Recovery Hysteresis** | Once an alarm has fired, the temperature must come back this far *past* the original threshold (not just to it) before the alarm is considered cleared. | 0.1 – 5.0 °C | 0.5 °C | Prevents the alarm flapping on/off right at the threshold. Raise if you're seeing repeated clear/re-alarm cycles. |

> 📷 **Screenshot placeholder — Web Dashboard: Alarms (advanced) fields**
> 📷 **Screenshot placeholder — Touchscreen: Settings 5/8**

**High/Low alarm lifecycle:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│           Is the room past the High or Low Alarm Delta?              │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES                             │ NO — no alarm, nothing to do        │
│  Start (or continue) timing      │                                    │
│  how long this has been true     │                                    │
├─────────────────────────────────┴────────────────────────────────────┤
│           Has it held for at least "Alarm Persist Time"?             │
├─────────────────────────────────┬────────────────────────────────────┤
│ NO — condition noted, but         │ YES — alarm is now ACTIVE:        │
│ not yet alarming                  │  siren on (if enabled and not     │
│                                    │  muted from the home bell),       │
│                                    │  ntfy push sent, event logged     │
├─────────────────────────────────┴────────────────────────────────────┤
│ WHILE alarm is ACTIVE                                                │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │ Home bell tapped? → mute siren/audio for this event only         │   │
│ │ (bell stays red, stops jiggling; banners + ntfy keep going;      │   │
│ │  mute clears when all alarms clear, or a *new* alarm type appears)│   │
│ │ Has the room recovered past threshold **plus** the Recovery      │   │
│ │ Hysteresis? → YES: clear alarm, siren off, "cleared" notification│   │
│ └────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

*(Startup Grace and Post-Defrost Grace simply suppress the whole diagram above — no alarm
condition is evaluated at all — for their respective windows.)*

---

### 4.5b Audio Alerts (Speaker)

*Touchscreen: Settings 8/8 — "Audio". Web: **Audio** tab. ESPHome web: **Audio Alerts** group.
Microphone / voice commands are **on hold** — no mic fitted; may not be used on this project.
Speaker announcements only.*

Spoken phrases use a pre-recorded female English voice on the on-board ES8311 speaker
(NS4150B amp). They work offline (no cloud TTS). Regenerate clips with
`scripts/generate_audio_clips.sh` if you want different wording or voice.

| Setting | What it does | Default |
|---|---|---|
| **Audio Alerts Enabled** | Master on/off for every spoken phrase. | On |
| **Speaker Volume (%)** | Loudness of spoken alerts (50–90%). Default **85%**. Floored so alarm speech cannot be silenced by accident; capped to avoid amp distortion. Use **Test Speaker** after changing it. | 85% |
| **Test Audio Alert** *(button)* | Plays a short test line (ignores master off so you can verify hardware). | — |
| **Speaker Amplifier (diagnostic)** | Raw power control for the amplifier itself. Playback switches it on and off automatically, so leave it alone in normal use — it is there to test the amplifier in isolation or force it quiet. Returns to off after every reboot. | Off |
| **Speak High / Low Temp Alarm** | Alarm voice when that temperature alarm becomes active. | On |
| **Speak Door Open Alarm** | Alarm voice when the *delayed* door alarm fires (§4.6). | On |
| **Speak No-Cool / Ice / Probe Fault** | Alarm voice for those conditions (phrases match the centre status: *Not cooling*, *Ice on coil*, *Coolroom probe bad*). | On |
| **Speak Relay Board Offline** | Voice when the Modbus relay board stops responding. | On |
| **Speak Temp Board Offline** | Voice when the RS485 temperature-probe board stops responding (only when an RTD source is selected). | On |
| **Speak Humidity / Ambient Sensor Offline** | Voice when the enabled SHT31 or SHT20 stops answering. | On |
| **Speak Cooling Started / Stopped** | Info voice when the compressor starts or stops. | Started On / Stopped **Off** |
| **Speak Defrost Started / Complete** | Info voice for defrost cycle edges. | On |
| **Speak Door Opened / Closed** | Info voice when the reed sees the door move (immediate). | Opened On / Closed **Off** |

The home-screen **bell** soft-mute also suppresses spoken phrases for the current event
(same as the siren). The icon stays red and still while muted; a new alarm type lifts mute
so speech and the jiggle can run again. Flip **Audio Alerts Enabled** off if you want the
room quiet permanently.

**Volume.** Use **Speaker Volume (%)** on touchscreen Settings 8/8 or the web Audio tab (default 85%). The scale is
not a linear percentage of loudness: on this codec ~75% is unity gain and anything above
roughly 90% clips, so the control is deliberately limited to 50–90%. The same value is
what Home Assistant sees on the *Coolroom Speaker* media player — changing it in either
place updates the other.

---

### 4.6 Door

*Touchscreen: Settings 6/8 — "Door". Web: **Door** section.*

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Door Sensor Enabled** | Master switch for the door reed: door-open **alarm**, and a prerequisite for Door-Triggered Light and Hold Compressor While Door Open. Turning this **off** also turns Door-Triggered Light and door-hold off. | On/Off | **Off** | Enable when a door sensor is fitted. Leave off if the reed is disconnected or faulty. |
| **Door Sensor Mode (NC or NO)** | Tells the controller whether your physical door switch is Normally Closed or Normally Open wiring. | NC or NO | NC | Must match how the door switch is actually wired, or "open" and "closed" will read backwards. |
| **Door-Triggered Light Enabled** | When on (and Door Sensor Enabled is on), opening the door turns the cabinet light on; closing it turns the light off. Turning light on while the sensor is off **auto-enables** the sensor. | On/Off | **Off** | Turn on for automatic cabinet lighting with staff traffic. Use the Home screen light button for manual control instead. |
| **Hold Compressor While Door Open** | When on (and Door Sensor Enabled is on), the compressor stays off for as long as the door reed reads open. The fan (if Fan Relay Enabled) follows the compressor, so it also stops. | On/Off | **Off** | Use for busy doors / less humid air through a cold coil. Leave **off** if the reed can stick open — that would starve cooling. Auto-disables when Door Sensor is turned off. |
| **Door Alarm Delay** | How long the door can stay open before an alarm fires. | 0 – 300 s | 300 s (5 min) | Shorten for rooms where doors should only ever be open briefly; lengthen for rooms with routine long-duration loading. |

> 📷 **Screenshot placeholder — Web Dashboard: Door section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 6/8**

**Door-triggered light** (requires Door Sensor Enabled):

```text
┌──────────────────────────────────────────────────────────────────────┐
│                    Is Door Sensor Enabled?                           │
├─────────────────────────────────┬────────────────────────────────────┤
│ NO — door never drives the light │ YES                                │
│ (Door-Triggered Light is forced  │  Is Door-Triggered Light Enabled?  │
│  off when sensor is disabled)    │  NO → light untouched by door      │
│                                  │  YES → open ON / close OFF         │
└─────────────────────────────────┴────────────────────────────────────┘
```

**Door alarm** (also gated by Door Sensor Enabled):

```text
┌──────────────────────────────────────────────────────────────────────┐
│                    Is Door Sensor Enabled?                           │
├─────────────────────────────────┬────────────────────────────────────┤
│ NO — door alarm does nothing,    │ YES                                │
│ ever                              │  WHILE door is open               │
│                                   │ ┌────────────────────────────────┐│
│                                   │ │ Has it been open for at least  ││
│                                   │ │ "Door Alarm Delay"?            ││
│                                   │ │  → YES: alarm fires             ││
│                                   │ └────────────────────────────────┘│
├─────────────────────────────────┴────────────────────────────────────┤
│ Door closes at any point → alarm clears immediately, timer resets    │
└──────────────────────────────────────────────────────────────────────┘
```

The door alarm depends only on the reed and the delay, so it still fires during a
probe fault or the start-up grace period — a failed temperature probe does not
disable it.

**Hold compressor while door open** (also gated by Door Sensor Enabled; default off):

```text
┌──────────────────────────────────────────────────────────────────────┐
│         Is Door Sensor Enabled AND Hold Compressor While Door Open?  │
├─────────────────────────────────┬────────────────────────────────────┤
│ NO — door never holds cooling    │ YES AND door reads open            │
│                                  │  → compressor forced OFF           │
│                                  │  → fan follows (if Fan Relay on)   │
│                                  │ Door closed → normal hysteresis    │
│                                  │ resumes (subject to Off-Delay)     │
└─────────────────────────────────┴────────────────────────────────────┘
```

---

### 4.7 Probes & Sensors

*Touchscreen: Settings 7/8 — "Probes". Web: **Probes & Sensors** section.*

The web Probes tab also shows a live table of **Raw** (before calibration offset),
**Offset**, and **Corrected** (what control, alarms, and the home gauge use) for Probe 1,
Probe 2, and the internal SHT31 (SHT31 has no offset — raw and corrected match).

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Temperature Display Unit** | Chooses Celsius or Fahrenheit for LVGL and web dashboard temperature readouts. Published to Home Assistant as a select. | Celsius / Fahrenheit | **Celsius** | Change for operator preference. This does not alter control math or stored values; HA temperature sensors follow HA's own unit system. |
| **Probe 1 (Coolroom) Calibration Offset** | Adds a fixed correction to the main coolroom-air RTD reading (CH1). | -10 – 10 °C | 0.0 °C | Only set this after comparing the probe against a trusted reference thermometer — enter the difference so the displayed reading matches reality. |
| **Probe 2 (Evaporator) Calibration Offset** | Same idea, for the evaporator RTD (CH2). | -10 – 10 °C | 0.0 °C | Same approach — compare against a reference thermometer first. |
| **Calibrate Probes Now** *(button)* | Auto-calibrate RTD offsets against the internal SHT31 reference (averages samples, writes offsets). | — | — | Needs SHT31 + at least one RTD online. Refuses until RS485 boards are connected on the bench. |
| **Evaporator Probe (Probe 2) Enabled** | Master switch for the evaporator RTD. | On/Off | On | Smart Defrost (§4.3) and the Ice Alarm (§4.5) both need Probe 2 — disabling it disables those features too, even if their own switches are on. Does **not** reassign which physical sensor is Probe 1 or Probe 2. |
| **Internal SHT31 Sensor Enabled** | Cabinet humidity/temperature sensor. | On/Off | On | Informational + dew-point / frost-rate / auto-cal — see note below. Not a substitute for Probe 1. |
| **External SHT20 Sensor Enabled** | Room/ambient humidity/temperature sensor. | On/Off | On | Informational only — see note below. |

> **Probe wiring is fixed.** Probe 1 (RTD CH1) is always coolroom **room air** temperature;
> Probe 2 (RTD CH2) is always the **evaporator coil** sensor. Do not swap the sensors on the
> converter channels, and there is no settings option to swap them in software. See
> `reference/hardware_pins.md` (Modbus RTD probe roles).

> **Humidity is monitored, not controlled.** This controller has no humidifier or
> dehumidifier relay — the SHT31/SHT20 sensors report humidity for your information and for
> the Dew Point defrost trigger (§4.3) to use, but there is no automatic humidity control
> loop. If your stored product needs a specific humidity range, you'll need a separate
> humidification/dehumidification system; use these readings to monitor it.

**Probe fault (safety) behavior** — this isn't a setting, but it's important to understand:
if Probe 1's reading goes stale (no new sample for too long) or reads outside a sane
physical range, the controller treats it as a fault:

```text
┌──────────────────────────────────────────────────────────────────────┐
│           Is Probe 1's latest reading missing, stale, or             │
│           physically implausible?                                    │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES — PROBE FAULT                │ NO — normal operation             │
│  • Compressor forced OFF          │                                    │
│    immediately (safety)           │                                    │
│  • If Sensor Fallback is enabled  │                                    │
│    (§4.1), compressor instead     │                                    │
│    runs the fallback duty cycle   │                                    │
│  • ntfy push + event log entry    │                                    │
└─────────────────────────────────┴────────────────────────────────────┘
```

The fallback duty cycle needs no temperature reading at all — that is the whole point of it.
It starts its first ON period as soon as the compressor off-delay allows a start (§4.1),
which after a restart means up to the full Compressor Off-Delay of waiting first. Until
then the snowflake stays amber with the lockout counting down, then turns blue for the
Fallback Compressor ON Time.

> 📷 **Screenshot placeholder — Web Dashboard: Probes & Sensors section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 7/8**

---

### 4.8 Notifications (ntfy Push Alerts)

*Web dashboard: **Alarms & Notify** tab — no touchscreen equivalent (push goes to your
phone/computer, not the unit itself).*

The controller sends push notifications through [ntfy](https://ntfy.sh) — a free,
no-account-needed push service. Every message includes a timestamp.

| Setting | Range / values | Default | When to change it |
|---|---|---|---|
| **ntfy Push Notifications Enabled** | on / off | on | Mute phone pushes without silencing local alarms or the siren |
| **ntfy Server URL** | `http://` or `https://` URL | `https://ntfy.sh` | Point at a self-hosted ntfy instance |
| **ntfy Topic** | 1–64 characters | (firmware build default) | Match the topic you subscribe to in the ntfy app |
| **Priority: High Temp Alarm** | min / low / default / high / urgent | high | Raise to urgent if the phone must break through Do Not Disturb |
| **Priority: Low Temp Alarm** | min / low / default / high / urgent | high | Same as high-temp, for freeze-sensitive stock |
| **Priority: Door Open Alarm** | min / low / default / high / urgent | high | Often turned down — a door left open is usually noticed on site first |
| **Priority: No-Cool Alarm** | min / low / default / high / urgent | urgent | The failure that spoils stock; leave urgent unless you have a reason |
| **Priority: Ice Alarm** | min / low / default / high / urgent | high | Coil icing / airflow issue |
| **Priority: Probe Fault** | min / low / default / high / urgent | urgent | With the main probe out, temperature alarms cannot protect the room |
| **Priority: Hardware Offline** | min / low / default / high / urgent | urgent | Shared priority for relay board / temp board / humidity / ambient offline pushes (same faults as centre status + spoken alerts) |
| **Priority: SD Card Failure** | min / low / default / high / urgent | high | Cooling is unaffected — fair one to turn down |
| **Priority: All-Clear Messages** | min / low / default / high / urgent | low | Covers Alarm CLEARED, SD Card Recovered, and hardware ONLINE recoveries; low so good news does not wake anyone |
| **Send Test Notification** *(button)* | — | — | Confirm the phone is subscribed; sent at the High Temp Alarm priority |

| Alert | Priority | When it fires |
|---|---|---|
| 🌡️ Coolroom HIGH TEMP Alarm | *(Priority: High Temp)* | High-temp alarm becomes active (past persist time, §4.4/§4.5) |
| ❄️ Coolroom LOW TEMP Alarm | *(Priority: Low Temp)* | Low-temp alarm becomes active |
| 🚪 Coolroom DOOR OPEN Alarm | *(Priority: Door Open)* | Door stays open past the Door Alarm Delay (§4.6) |
| 🥶 Coolroom NO-COOL Alarm | *(Priority: No-Cool)* | Compressor running but room isn't cooling (§4.5) |
| ❄️ Coolroom ICE ALARM | *(Priority: Ice)* | Ice Alarm Delta condition met (§4.5) |
| ✅ Coolroom Alarm CLEARED | *(Priority: All-Clear)* | Any alarm above recovers past its threshold/hysteresis band |
| ⚠️ Coolroom PROBE FAULT | *(Priority: Probe Fault)* | Main probe fails (§4.7) |
| ⚠️ Coolroom RELAY BOARD OFFLINE | *(Priority: Hardware Offline)* | Modbus relay board stops responding (§4.3 / centre status) |
| ⚠️ Coolroom TEMP BOARD OFFLINE | *(Priority: Hardware Offline)* | Modbus RTD board expected but offline |
| ⚠️ Coolroom HUMIDITY SENSOR OFFLINE | *(Priority: Hardware Offline)* | Cabinet SHT31 enabled but not responding |
| ⚠️ Coolroom AMBIENT SENSOR OFFLINE | *(Priority: Hardware Offline)* | Ambient SHT20 enabled but not responding |
| ✅ Coolroom … ONLINE (relay / temp / humidity / ambient) | *(Priority: All-Clear)* | Matching board or sensor responds again |
| ⚠️ Coolroom SD CARD FAILURE | *(Priority: SD Card)* | SD card missing at boot, or fails during operation (§4.9) |
| ✅ Coolroom SD Card Recovered | *(Priority: All-Clear)* | Auto-remount brings a previously-failed card back online (§4.9) |

Every alarm type reaches ntfy — none are event-log-only any more.

**Setup:** open the web dashboard **Alarms & Notify** tab after logging in. Set the
**ntfy Server** and **Topic**, tune each alert's priority if you want, then use
**Send Test Notification** to confirm delivery. Install the free ntfy app
(iOS/Android) or use ntfy.sh in a browser and subscribe to that same topic.

---

### 4.9 Data & SD Card

*Web dashboard: **System** section (buttons) + touchscreen Info page (status only).*

The SD card is entirely **optional** — the controller runs normally without one. If no card
is present at boot, or a card fails while running, logging and backup/restore simply become
no-ops (silently skipped) rather than crashing or affecting cooling control.

> ⚠️ **Take a Backup before any firmware update.** Settings live in flash and survive
> reboots and over-the-air updates, but a USB firmware update using the full factory image
> erases them — every setting returns to its factory default, which typically shows up as
> switches you had turned off coming back on. Whoever performs the update should use the
> settings-preserving flash method (`tools/esphome_flash.sh`) or update over the air; a
> Backup makes recovery a single Restore press either way.

| Feature | What it does |
|---|---|
| **Event Log** | Every alarm, defrost start/end, and compressor on/off is written to a daily CSV file on the SD card, with the sensor readings behind the decision — useful for troubleshooting after the fact. |
| **Temperature Log** | Periodic temperature/humidity samples logged the same way, to one file per day named for that date. Samples taken before the controller has learned the time (briefly at boot, or for longer if it can't reach a time server) go to a single `nodate.csv` file instead, so they aren't filed under a wrong date. It appears in the log browser alongside the dated files. |
| **Backup All Settings to SD** *(button)* | Saves every setting in this manual to a `backup.json` file on the card. Confirms success in-place — it does **not** force a browser download. Download a copy yourself from the file list below when you want one. Settings themselves live in flash (NVS) and survive reboots without this button; Backup is an off-device copy for factory-reset recovery or cloning to another unit. |
| **Restore All Settings from SD** *(button)* | Loads settings back from that file — useful after a factory reset or when cloning settings to another unit. If there is no backup on the card, it tells you rather than doing nothing. The card is **never** applied automatically at boot: an old backup on the card used to silently overwrite every change made since the last Backup press, which is why Restore is now operator-only. |
| **SD Card on Info (touchscreen)** | Mount status, free space, whether `backup.json` is present, plus **Backup SD** / **Restore SD**. Full directory listing stays on the web SD Card tab. |
| **SD Card browser** | The web dashboard **SD Card** tab lists every managed file (logs + `backup.json`) with size and modified time (local timezone — Australia/Perth on this build), plus card stats: mounted/name, used/free/total space, bus speed, and this-boot read/write counts (for lifespan estimation — consumer cards do not expose wear SMART data). Download and delete are available from the same table; `backup.json` can be downloaded but not deleted. |
| **Events tab** *(live)* | Web **Events** tab streams the same alarm / fault / defrost / WiFi lines that are appended to `events.csv`, as they happen this boot. Pause or clear the view without touching the SD file. |
| **Auto-Remount** | If the card fails mid-session (removed, corrupted), the controller checks every 60 seconds and automatically resumes logging/backup the moment a working card is present again — **no reboot needed**. It does *not* automatically restore your settings on reconnect (to avoid overwriting anything you changed while the card was out); it only resumes logging and lets you press Restore manually if you want to. |
| **SD Card Failure Alert** | You'll get a push notification (§4.8) the moment the card is missing or fails — so a dead/removed card doesn't go unnoticed. The card is only reported failed when it genuinely stops accepting writes; a single file that can't be written no longer takes the whole card offline. |

**Auto-remount cycle:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│ WHILE SD card is reported not-OK                                     │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │ Every 60 seconds: attempt to mount the card                     │   │
│ │           Did the mount succeed this time?                       │   │
│ │ ┌────────────────────────────┬────────────────────────────────┐│   │
│ │ │ NO — stay "not-OK",         │ YES —                          ││   │
│ │ │ try again in 60s             │  • mark card OK                ││   │
│ │ │                               │  • log "SD_REMOUNTED"          ││   │
│ │ │                               │  • push "SD Card Recovered"    ││   │
│ │ │                               │  • logging/backup resume       ││   │
│ │ │                               │  (settings are NOT re-loaded   ││   │
│ │ │                               │   automatically)               ││   │
│ │ └────────────────────────────┴────────────────────────────────┘│   │
│ └────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

**Downloading/deleting logs:** the log files browser (list/download/delete) is reachable
from the web dashboard, **operator (logged-in) access only** — see §4.11. There is no
touchscreen equivalent, and deletion is never available on the touchscreen by design.

> 📷 **Screenshot placeholder — Web Dashboard: System / SD Card buttons**
> 📷 **Screenshot placeholder — Web Dashboard: Log file manager**

---

### 4.10 Wireless

*Web only, **Wireless** tab. WiFi changes are deliberately **not** available on the
touchscreen.*

| Item | What it does |
|---|---|
| **Current Connection** *(read-only)* | Network name, signal strength, IP address, channel, the access point's BSSID, and this device's own MAC. Network name, signal and IP are also mirrored read-only on the touchscreen Info page. |
| **Setup AP** *(read-only)* | Whether the controller's own fallback access point is currently running. |
| **Scan** | Lists the networks in range, strongest first, one row per name. Click a row to fill in its name below. Scanning does **not** drop the current connection. |
| **Join Network** | Network name + password, then **Join Network**. |
| **Home Assistant API Enabled** | Opt-in for Home Assistant’s ESPHome native API. **Off by default.** When on, HA can discover and read/control published sensors, switches, numbers, and the rest. When off, any HA connection is dropped immediately. Re-enable only from this dashboard (or the stock ESPHome web UI) — once off, HA itself cannot turn it back on. |

**Joining a network is a trial, not a commitment.** The controller drops its current
connection to try the new one, and:

- if it associates within **45 seconds**, the new network is saved and becomes the one it
  boots onto from then on;
- if it doesn't, the previous network's credentials are put back automatically and it
  reconnects to where it was.

Nothing is written to permanent storage until the new network has actually worked, so a
power cut in the middle of an attempt also leaves the old network in place. The dashboard
shows *testing* → *joined* or *reverted* with a plain-language result — you will normally
lose contact with the page for a minute while this happens, which is expected.

**If a saved network stops working later** (renamed, retired, password changed) the
controller restores the network configured in its firmware after **5 minutes** with no
connection at all, and connects to that instead. This is the safety net for the case that
otherwise needs a laptop and a USB cable: a saved network replaces the configured one
entirely, so without it a network that disappears would strand the device.

**First-time / no known network:** the controller starts its own access point automatically
— connect to it and browse to `192.168.4.1` to enter your real network's details (see §2.1).

> 📷 **Screenshot placeholder — Web Dashboard: Wireless tab**

---

### 4.11 Access Control & Security

There are **two completely separate locks** on this system — don't confuse them:

| | Web Dashboard Login | Touchscreen Settings PIN |
|---|---|---|
| **What it protects** | Every web-dashboard setting + admin function | Only the 8 touchscreen settings pages |
| **Credential** | The device's one HTTP username/password (`secrets.yaml`, set at build time) | 4-digit PIN, factory default `0000`, changeable on-device |
| **How to change it** | Not changeable from the dashboard — requires re-flashing firmware with new `secrets.yaml` values (re-run `scripts/embed_dashboard.py` before compile) | Touchscreen: Settings 1/8 → Change PIN |
| **Session behavior** | Logs out silently after 2 minutes of inactivity (no banner); never persisted across a page reload | Stays unlocked until you navigate back to Home |

**Web dashboard has exactly two states** — there is no third "admin" or "superadmin" tier
(an earlier version of this documentation described one; it never actually existed
server-side and has been removed):

- **Guest** (default): main status / health only — no settings UI on the page.
- **Operator** (logged in): settings, advanced controls, and administration appear.

**Important limitation, worth understanding:** Login only controls what the *dashboard
page* lets you edit — the REST API (`/api/states`) is intentionally open on the LAN so
guests can load live metrics without a password. Treat your site network (or VPN) as the
actual security perimeter, same as you would for any other network appliance. Full detail:
`reference/RBAC_USER_GUIDE.md` and `reference/AUTHENTICATION_GUIDE.md`.

This two-tier model is the **intended, permanent design** for this system — not a stopgap.
A guest can always see everything on the main display without logging in; a single operator
login elevates to settings and administration. Real multi-user / multi-account server auth
is **not required** for this project (closed 2026-08-01).

**Deliberately web-only, never added to the touchscreen** (by explicit design, not an
oversight): WiFi configuration (§4.10), Home Assistant API enable (§4.10), the web
dashboard's login credential, and the SD card log-delete function (§4.9). These stay off
the touchscreen so a PIN alone can never be used to change network access, expose the
device to Home Assistant, or wipe evidence in the event log.

> 📷 **Screenshot placeholder — Web Dashboard: Login prompt**

---

### 4.12 System & Diagnostics

*Touchscreen: Info page (read-only). Web: **System** + **Diagnostics** sections.*

Mostly read-only status, useful for troubleshooting rather than day-to-day adjustment.

| Item | What it shows |
|---|---|
| System Uptime / Current Time / NTP Sync Status | How long since last reboot, and whether the clock is synced. Wall clock uses the ESP32-P4 internal LP RTC; NTP over Wi‑Fi keeps it accurate. With a cell in the board’s RTC battery holder, time can survive power cuts; without it, expect pending time until first NTP after every hard power loss. The Info page **System Time** line shows how long ago the last NTP sync landed — the controller polls every **60 s** until the first sync, then settles to **weekly** once the clock is valid. |
| Free Heap / Free PSRAM / Chip Temperature / CPU Usage | Controller's own internal health. CPU is the average busy percentage across both P4 cores over the last sample window (typically a few seconds). The web dashboard EMA-smooths the displayed CPU so a hard page refresh does not briefly flash a reconnect spike; the touchscreen Info page shows the raw windowed value. |
| SD Card Free Space / SD Card Mounted | Storage headroom and current mount status (§4.9). The touchscreen Info page shows card total plus used/free in MB **and percent**, and whether a `backup.json` is present. |
| RS485 Bus Status / RTD Sample Age / Probe Health Summary | Whether the sensor bus and individual probes are responding and how fresh their last reading is |
| System Time Valid / SHT31 / SHT20 Online | System Time Valid = wall clock has a usable time (SoC LP RTC, usually after NTP). SHT31/SHT20 = humidity sensors detected on the I2C header. |
| Sensor Fallback Active | Whether the compressor is currently running the fallback duty cycle from §4.1/§4.7 |
| **Restart Controller** *(button)* | Reboots the device. Settings are preserved (stored in flash), in-progress defrost/alarm timers are not. On the web dashboard this lives on the **Hardware** tab under *Controller*. |
| **Factory Reset** *(button)* | Erases every setting in this manual back to its default value **and clears the saved WiFi credentials**, then reboots. The controller comes back on its own access point and has to be re-joined to your network (§4.10), so do not use it remotely. Take a Backup (§4.9) first if you want to restore afterward. On the web dashboard it is on the **Hardware** tab under *Controller*, behind a confirmation and a typed `RESET`. |
| **Display Backlight** | Touchscreen brightness. |

> 📷 **Screenshot placeholder — Web Dashboard: System & Diagnostics sections**
> 📷 **Screenshot placeholder — Touchscreen: Info page**

---

### 4.13 Hardware & Relay Outputs

*Web only, **Hardware** tab.*

Shows what the controller can see of itself — controller/WiFi/IP, both RS485 boards, RTC,
both humidity sensors, SD card, chip temperature, free memory and uptime — plus direct
control over the four relay outputs, the touchscreen PIN, and a **Controller** section
holding **Restart Controller** and **Factory Reset** (both described in §4.12).

**Modbus coil map (Waveshare RTU 4-CH, address 1):**

| Coil | Output | Notes |
|---|---|---|
| 0 | Fan | Evaporator fan. Enable default **off**. Follows compressor when enabled; off during defrost/drip. |
| 1 | Compressor | Cooling. Enable default on. |
| 2 | Light | Cabinet light. |
| 3 | Siren | External alarm. |

Defrost is always passive — there is no heater coil on this controller.

| Setting | What it does | Default | When to change it |
|---|---|---|---|
| **Compressor Relay Enabled** | Whether the compressor output may energise at all. | On | Turn off to isolate the compressor for maintenance without disabling the control logic behind it. |
| **Fan Relay Enabled** | Whether the evaporator **fan** output (Modbus coil 0) may energise. When on, the fan follows the compressor and is forced off during defrost + drip. Default **off**. Also on §4.1 / web Compressor. | **Off** | Confirm the plant wires a fan (not a heater) to coil 0 before enabling. |
| **Light Relay Enabled** | Whether the light output may energise at all. | On | Turn off if the room light is switched by something else. |
| **Siren Relay Enabled** | Whether the alarm output may energise at all. | On | Turn off where the alarm relay drives an external siren you don't want sounding every time — see below. |

**These are different from the feature switches elsewhere in this manual.** A feature
switch (*Defrost System Enabled*, *Alarm Siren Enabled*, …) decides whether a function
runs. A relay enable decides whether its physical output is allowed to close. Turning off
**Siren Relay Enabled** leaves alarms raising, notifying and logging exactly as before —
the external siren simply stays silent. Turning an enable off drops that output
immediately and holds it off; nothing in the control logic can close it again until you
turn it back on. The settings survive a reboot.

**Rows are greyed out when the relay board wasn't found.** If the RS485 relay board didn't
answer at startup the outputs can't be commanded at all, and the tab says so rather than
accepting toggles that would do nothing.

**Relay outputs are also reset at every startup.** The relay board keeps its own state when
the controller reboots, so its contacts are commanded open once it comes back on the RS485
bus, and the control logic takes it from there. Without that, a compressor left running
through a controller restart would keep running — and be shown as running — through the
off-delay that is supposed to be holding it off.

The light is the one output that isn't simply forced open at startup: if Door Sensor and
Door-Triggered Light are both on (§4.6) and the door is already open when the controller
comes back, the light is switched on to match. Everything else — compressor, fan, siren —
starts open (fan stays off until Fan Relay Enabled and the compressor call for it).

> **If the light comes on by itself after a reboot, check the door input first.** With
> Door Sensor and Door-Triggered Light both enabled, an open door is *supposed* to light the
> room. A door switch that isn't fitted or is wired against the Door Sensor Mode setting
> reads as permanently open, so the light comes on at every startup and stays on. Confirm
> the reading on the **Door Reed Sensor** row before assuming a fault, and correct the NC/NO
> setting or the wiring (§4.6).

> 📷 **Screenshot placeholder — Web Dashboard: Hardware tab**

---

## 5. Troubleshooting Quick Reference

| Symptom | Likely cause | Where to look |
|---|---|---|
| Room won't reach setpoint | Compressor differential too wide, door left open, or a real cooling fault | §4.1, §4.6, §4.5 (No-Cool Alarm) |
| Alarms firing right after every door open | Alarm Persist Time too short for your traffic pattern | §4.4 |
| Alarms firing right after every defrost | Post-Defrost Alarm Grace too short | §4.5 |
| Repeated alarm/clear flapping | Alarm Recovery Hysteresis too tight | §4.5 |
| Ice alarm firing during normal operation | Ice Alarm Delta too sensitive for your setup | §4.5 |
| Coil visibly frosting between scheduled defrosts | Enable Smart Defrost and/or Dew Point Trigger | §4.3 |
| Room warms noticeably after every defrost | Turn on Early Termination by Temperature, or shorten Max Duration | §4.2 |
| Door alarm never fires | Door Sensor Enabled is off by default — check §4.6 | §4.6 |
| Compressor stops whenever the door is open | Hold Compressor While Door Open is on — intentional if you enabled it; turn off if a stuck reed is starving cooling | §4.6 |
| Fan never runs even though the compressor does | Fan Relay Enabled is off (default) or coil 0 is not wired to a fan | §4.1, §4.13 |
| Light is on by itself after every reboot | Door Sensor + Door-Triggered Light are both on and the door reads open — an unfitted or miswired reed reads open permanently. Check Door Reed Sensor, NC/NO, or turn Door-Triggered Light off | §4.6, §4.11 |
| No push notifications arriving | Confirm ntfy topic subscription (§4.8); confirm WiFi is connected (§4.10) | §4.8, §4.10 |
| SD card seems to have "given up" | Check for the SD Card Failure push — auto-remount retries every 60s once the card is working again, no reboot needed | §4.9 |
| Repeated SD failure/recovery pushes for a card that is physically fine | Expected to be gone: this was a firmware fault where an unwritable filename was misread as a dead card. If it still happens, the card really is dropping writes — try a different card | §4.9 |
| Touchscreen readings don't match a reference thermometer | Set a Calibration Offset after comparing against a trusted reference | §4.7 |
| Compressor won't start after a power cut | Normal — the off-delay counts from power-up. Watch the amber countdown under the snowflake | §4.1 |
| Snowflake red, status says RELAY BOARD OFFLINE, no cooling animation | RS485 relay board not responding — compressor control will not pretend to run without it | §4.1, Hardware |
| An output never energises, no matter what the logic does | Its relay enable is off — a disabled output is held open | §4.13 |
| External siren sounds on alarms you'd rather it didn't | Turn off Siren Relay Enabled; alarms, pushes and logging continue | §4.13 |
| Controller vanished after a WiFi change | It reverts by itself after 45 s, and falls back to the firmware's network after 5 min without a connection | §4.10 |
| Home Assistant rejects the encryption key when adding the device | Almost always **Home Assistant API Enabled is still off** (its default). The controller drops the connection mid-handshake, which HA reports as a bad key rather than a refusal. Turn the toggle on first, then re-add with the same key | §4.10 |
| A setting described here isn't on the web dashboard | The dashboard opens in guest mode and hides every settings tab until you press **🔐 Login** — check you are logged in before assuming a setting is missing | §4.11 |
| Settings revert to defaults (e.g. ntfy switches itself back on) after a firmware update | Expected only when the controller was reflashed **over USB with the factory image**, which erases stored settings. Normal reboots and over-the-air updates keep them. Whoever flashes should use the settings-preserving method — see the note in §4.9 | §4.9 |

---

## 6. Document Map

| Group | Touchscreen page | Web dashboard section |
|---|---|---|
| Home icons (status / light / mute) | Home left rail | — (touchscreen only) |
| Temperature Control & Compressor | Settings 1/8 | Temperature Control, Compressor (incl. Fan Relay Enabled) |
| Defrost Schedule | Settings 2/8 | Defrost |
| Defrost — Smart & Drip | Settings 3/8 | Defrost |
| Alarm Thresholds | Settings 4/8 | Alarms |
| Alarms — Advanced | Settings 5/8 | Alarms |
| Audio Alerts (speaker) | Settings 8/8 | Audio tab |
| Door (sensor, light, hold compressor) | Settings 6/8 | Door |
| Probes & Sensors | Settings 7/8 | Probes & Sensors |
| Notifications | — (web only; no keyboard on LVGL) | Alarms & Notify tab |
| Data & SD Card | Info (status + Backup/Restore) | SD Card tab (full file list) |
| Events (live log) | — (home alarms cover live status) | Events tab |
| Wireless | Info (SSID/signal/IP only) | Wireless tab (change Wi‑Fi) |
| Access Control & Security | Settings PIN entry | Login; PIN on Hardware tab |
| System & Diagnostics | Info | System, Diagnostics |
| Hardware & Relay Outputs (coil map / enables) | — | Hardware tab |

---

*See also: `reference/QUICK_START_GUIDE.md` for first-time setup steps and worked
recommended-settings examples; `reference/CAREL_CONTROL_DECISIONS.md` for hysteresis /
min-run / fan / door-hold product choices; `reference/RBAC_USER_GUIDE.md` and
`reference/AUTHENTICATION_GUIDE.md` for full detail on the web login model.*
