# Coolroom Controller — User Manual

**Applies to:** ESP32-P4 Coolroom Controller firmware (this repository)
**Audience:** Operators and site managers running the coolroom day to day.
**Companion document:** `reference/QUICK_START_GUIDE.md` — condensed setup steps and
recommended-settings worked examples. Read this manual for the full explanation of what
each function does and why; use the Quick Guide when you just need a fast answer.

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

> 📷 **Screenshot placeholder — Web Dashboard, guest view**
> *(to be added once hardware is connected)*

---

## 1. Overview — What This Controller Does

The controller keeps a coolroom at a target temperature, defrosts the evaporator coil on a
schedule, watches for problems (door left open, sensor failure, no cooling, ice buildup),
and tells you about it — on the touchscreen, on a web dashboard, and via push notification
to your phone.

**Two ways to reach it:**

| Surface | What it's for | Where |
|---|---|---|
| **Touchscreen** (7" LCD on the unit) | Day-to-day monitoring + on-site adjustments, PIN-protected | Physically on the controller |
| **Web Dashboard** | Full monitoring + every setting, from any browser on the network | `http://<device-ip>/assets/dashboard.html` |

**What it's physically connected to:**

| Component | Role |
|---|---|
| Probe 1 (RTD) | Main coolroom air temperature — everything is measured against this |
| Probe 2 (RTD) | Evaporator coil temperature — used for smart defrost and ice detection |
| Internal humidity sensor (SHT31) | Cabinet humidity, informational (see §4.7 — no automatic humidity control) |
| External humidity sensor (SHT20) | Room/ambient humidity, informational |
| Compressor relay | Turns cooling on/off |
| Defrost relay | Runs the defrost heater/cycle |
| Light relay | Cabinet light (see §4.6 — currently tied to the door-sensor-mode switch) |
| Siren relay | Audible alarm — fully automatic, see §4.5 |
| Door sensor | Optional — off by default, see §4.6 |
| SD card | Event/temperature logging + settings backup — optional, device runs fine without one |

---

## 2. Getting Started

### 2.1 First connection

1. On first boot with no known WiFi network, the controller starts its own WiFi access
   point. Connect to it and browse to `192.168.4.1` to enter your real WiFi network's name
   and password.
2. Once connected to your network, find the device's IP address (your router's device list,
   or the touchscreen's Info page) and open `http://<device-ip>/assets/dashboard.html`.
3. The dashboard opens in **Guest** mode — view-only. Click **🔐 Login** and enter the
   device credential (set in `secrets.yaml` at build time — ask whoever installed the unit
   if you don't have it) to unlock every setting. See §4.11 for how this works and its
   limits.
4. To change WiFi network later, or reconnect after a network change, use the **New WiFi
   SSID / Password** fields in the web dashboard's Network section — this is deliberately
   **web-only**, not available on the touchscreen.

### 2.2 Touchscreen settings PIN

The touchscreen has its own, separate 4-digit PIN (unrelated to the web login above) that
gates the on-screen settings pages. **Factory default PIN: `0000`.**

> Change this on first use. Tap **⚙️ Settings** → enter the PIN → **🔒 Change PIN** button
> on the first settings page.

The touchscreen PIN only unlocks settings shown on the touchscreen. It cannot be used to
log into the web dashboard, and the web dashboard's operator password cannot be used on the
touchscreen — they are two independent locks by design (see §4.11).

> 📷 **Screenshot placeholder — Touchscreen PIN entry keypad**
> *(to be added once hardware is connected)*

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

*Touchscreen: Settings 1/7 — "Compressor & Fallback". Web: **Temperature Control** +
**Compressor** sections.*

This is the core loop: keep the coolroom near a target temperature without short-cycling
the compressor.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Setpoint** | The temperature you want the coolroom to hold. | -30 – 15 °C | 2.0 °C | Set to whatever your stored product needs. |
| **Compressor Differential** | Carel-style dead band above the setpoint. Compressor switches **ON at setpoint + differential**, **OFF at setpoint**. | 0.5 – 10.0 °C | 1.0 °C | Wider = fewer compressor starts (longer compressor life) but more temperature swing above SP. Narrower = tighter control but more frequent cycling. |
| **Compressor Off-Delay (Lockout)** | Minimum time the compressor must stay off before it's allowed to restart, even if the temperature calls for cooling. | 0 – 10 min | 3 min | Protects the compressor motor from rapid restart. Only lower this if your compressor's manufacturer explicitly allows shorter cycling. |
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
│                                 │ │ Temp ≥         │ Temp ≤          ││
│                                 │ │ setpoint +      │ setpoint?       ││
│                                 │ │ differential?   │  → turn OFF     ││
│                                 │ │  → turn ON      │ (after min-run) ││
│                                 │ └───────────────┴─────────────────┘│
├─────────────────────────────────┴────────────────────────────────────┤
│ Before turning ON: has it been at least "Compressor Off-Delay"       │
│ since it last turned off? If not, wait — hold current state.         │
└──────────────────────────────────────────────────────────────────────┘
```

> 📷 **Screenshot placeholder — Web Dashboard: Temperature Control & Compressor sections**
> 📷 **Screenshot placeholder — Touchscreen: Settings 1/7**

---

### 4.2 Defrost Schedule

*Touchscreen: Settings 2/7 — "Defrost Schedule". Web: **Defrost** section.*

Frost builds up on the evaporator coil over time and blocks airflow, so the coil needs
periodic defrosting. This group controls the basic on/off timer for that.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Defrost System Enabled** | Master switch for the whole defrost feature. | On/Off | On | Turning this off stops **all** defrost activity, including the smart and dew-point triggers in §4.3. Only disable for troubleshooting. |
| **Defrost Interval** | How often a defrost cycle starts, on a fixed timer. | 60 – 1440 min | 480 min (8 h) | Shorter interval for rooms with heavy door traffic or high humidity (more frost buildup); longer for dry, low-traffic rooms. |
| **Defrost Max Duration** | Safety cap — defrost stops after this long even if it hasn't finished. | 5 – 60 min | 30 min | Raise slightly if defrost is being cut off before the coil is properly clear (check evaporator temp after a cycle). |
| **Defrost Early Termination by Temperature** | Ends defrost as soon as the evaporator reaches the target temp below, instead of always running the full Max Duration. | On/Off | On | Recommended on — avoids unnecessarily warming the room once the coil is already clear. |
| **Defrost Termination Temp** | The evaporator temperature that counts as "coil is clear" when the above is on. | 0.0 – 15.0 °C | 5.0 °C | Only relevant if Early Termination is on. |
| **Defrost Drip-Drain Phase Enabled** | After defrost heat turns off, hold before resuming cooling so melted frost can drain instead of refreezing immediately. | On/Off | On | Recommended on for any room where meltwater could re-ice on a cold coil. |

**Defrost cycle, start to finish:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│ WHILE Defrost System Enabled                                         │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │        Has it been "Defrost Interval" since the last defrost    │   │
│ │        ended? (or a Smart/Dew-Point trigger fired — see §4.3)    │   │
│ ├───────────────────────────────┬─────────────────────────────────┤   │
│ │ YES → start defrost            │ NO → keep cooling normally      │   │
│ ├───────────────────────────────┴─────────────────────────────────┤   │
│ │ WHILE defrost is running                                        │   │
│ │ ┌──────────────────────────────────────────────────────────┐    │   │
│ │ │ Has Max Duration elapsed, OR (Early Termination is on     │    │   │
│ │ │ AND evaporator has reached Termination Temp)?             │    │   │
│ │ │  → YES: stop defrost heat now                             │    │   │
│ │ └──────────────────────────────────────────────────────────┘    │   │
│ │ If Drip/Drain Phase is on: hold compressor off for the drip     │   │
│ │ time (§4.3) before resuming normal cooling.                      │   │
│ └────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

> 📷 **Screenshot placeholder — Web Dashboard: Defrost section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 2/7**

---

### 4.3 Defrost — Smart & Drip

*Touchscreen: Settings 3/7 — "Defrost Smart & Drip". Web: **Defrost** section (same
group as §4.2 on the web dashboard).*

These add two *extra* ways to trigger a defrost early, on top of the fixed timer in §4.2 —
they never replace the timer, only supplement it.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Defrost Drip Time** | How long to hold before resuming cooling after defrost ends (see §4.2's Drip Phase toggle). | 0 – 30 min | 5 min | Increase for coolrooms with poor drainage or a lot of meltwater. |
| **Smart Defrost (Delta-Triggered)** | Watches the gap between coolroom and evaporator temperature; if it grows too large for too long, starts a defrost early — regardless of the fixed timer. | On/Off | **Off** | Turn on for rooms with variable/heavy loads where frost can build up faster than the fixed interval expects. |
| **Smart Defrost Delta Threshold** | How large the coolroom-to-evaporator gap must get before it counts as "frosted up". | 1.0 – 20.0 °C | 8.0 °C | Lower = triggers more readily (more frequent smart defrosts); higher = more tolerant. |
| **Smart Defrost Dwell Time** | How long that gap must stay above the threshold before triggering — avoids reacting to a brief spike. | 1 – 120 min | 30 min | Raise if smart defrost is triggering on short-lived temperature blips (e.g. right after a door opens). |
| **Dew Point Early Defrost Trigger** | Uses humidity + temperature to calculate the dew point, and starts a defrost the moment the evaporator is actually cold enough for frost to be forming — not just "probably frosted", but confirmed. | On/Off | **Off** | Turn on for humid environments (produce storage, anywhere with high moisture load) — this is the most targeted of the three triggers. |

**Note:** both extra triggers default to **Off**. A brand-new install only defrosts on the
fixed timer (§4.2) until you turn one or both of these on.

> 📷 **Screenshot placeholder — Web Dashboard: Defrost (Smart) fields**
> 📷 **Screenshot placeholder — Touchscreen: Settings 3/7**

---

### 4.4 Alarm Thresholds

*Touchscreen: Settings 4/7 — "Alarm Thresholds". Web: **Alarms** section.*

The basic "something's wrong with the temperature" alarms.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **High Temp Alarm Delta** | How far *above* setpoint the room can drift before a high-temperature alarm fires. | 0.5 – 20.0 °C | 3.0 °C | Tighten for sensitive product, loosen if door traffic causes nuisance alarms. |
| **Low Temp Alarm Delta** | How far *below* setpoint before a low-temperature alarm fires. | 0.5 – 20.0 °C | 3.0 °C | Tighten if freeze-sensitive product is stored (see the plums example in the Quick Guide). |
| **Alarm Persist Time** | How long an alarm condition must hold continuously before the siren/notification actually fires. | 0 – 30 min | 5 min | Prevents nuisance alarms from a brief door-open temperature blip. Raise for high-traffic rooms. |
| **Alarm Siren Enabled** | Master switch for the physical siren relay. | On/Off | On | The siren is **fully automatic** — it turns on the moment any alarm is active (past its persist time) and off the moment all alarms clear. There is no manual mute button; turn this switch off if you need silence, and remember to turn it back on afterwards. |

> 📷 **Screenshot placeholder — Web Dashboard: Alarms section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 4/7**

---

### 4.5 Alarms — Advanced

*Touchscreen: Settings 5/7 — "Alarms Advanced". Web: **Alarms** section (same group as
§4.4 on the web dashboard).*

Less commonly touched, but important for tuning out false alarms.

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Ice Alarm Delta** | If the coolroom-to-evaporator temperature gap shrinks below this, the coil is assumed to be blocked with ice and an alarm fires. | 0.5 – 10.0 °C | 2.0 °C | Lower if you're getting false ice alarms during normal operation; raise for earlier warning in humid rooms. |
| **No-Cool Alarm Timeout** | If the compressor has been running this long without the room actually cooling, something's wrong (stuck compressor, refrigerant leak, blocked airflow) — alarm fires. | 15 – 240 min | 60 min | Tighten for high-value product where a refrigeration failure needs fast attention; loosen for rooms with a naturally slow pulldown. |
| **Startup Alarm Grace Floor** | No alarms fire for this long after the controller boots, giving the room time to reach setpoint from a cold start without nuisance alarms. | 0 – 60 min | 15 min | Raise if your room routinely takes longer than 15 minutes to pull down after a power cycle. |
| **Post-Defrost Alarm Grace** | No alarms fire for this long after a defrost cycle ends, while the room recovers from the temporary warm-up defrost causes. | 0 – 60 min | 20 min | Raise if alarms are triggering right after routine defrosts. |
| **Alarm Recovery Hysteresis** | Once an alarm has fired, the temperature must come back this far *past* the original threshold (not just to it) before the alarm is considered cleared. | 0.1 – 5.0 °C | 0.5 °C | Prevents the alarm flapping on/off right at the threshold. Raise if you're seeing repeated clear/re-alarm cycles. |

> 📷 **Screenshot placeholder — Web Dashboard: Alarms (advanced) fields**
> 📷 **Screenshot placeholder — Touchscreen: Settings 5/7**

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
│ not yet alarming                  │  siren on (if enabled),           │
│                                    │  ntfy push sent, event logged     │
├─────────────────────────────────┴────────────────────────────────────┤
│ WHILE alarm is ACTIVE                                                │
│ ┌────────────────────────────────────────────────────────────────┐   │
│ │ Has the room recovered past threshold **plus** the Recovery      │   │
│ │ Hysteresis? → YES: clear alarm, siren off, "cleared" notification│   │
│ └────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

*(Startup Grace and Post-Defrost Grace simply suppress the whole diagram above — no alarm
condition is evaluated at all — for their respective windows.)*

---

### 4.6 Door

*Touchscreen: Settings 6/7 — "Door". Web: **Door** section.*

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Door Sensor Enabled** | Master switch for the door-open **alarm**. | On/Off | **Off** | The door alarm does nothing at all until you turn this on — if you have a door sensor fitted, enable it here. Does not affect the door-triggered light below, which has its own independent switch. |
| **Door Sensor Mode (NC / NO)** | Tells the controller whether your physical door switch is Normally Closed or Normally Open wiring. | NC/NO | NC | Must match how the door switch is actually wired, or "open" and "closed" will read backwards. |
| **Door-Triggered Light Enabled** | When on, opening the door turns the cabinet light on; closing it turns the light off — independent of the alarm switch above. | On/Off | On | Turn off if you'd rather control the cabinet light manually (Home screen light button) without it being overridden by door state. |
| **Door Alarm Delay** | How long the door can stay open before an alarm fires. | 0 – 300 s | 300 s (5 min) | Shorten for rooms where doors should only ever be open briefly; lengthen for rooms with routine long-duration loading. |

> 📷 **Screenshot placeholder — Web Dashboard: Door section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 6/7**

**Door-triggered light:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│                 Is Door-Triggered Light Enabled?                     │
├─────────────────────────────────┬────────────────────────────────────┤
│ NO — door state never touches    │ YES                                │
│ the light relay                  │  Door opens → light ON             │
│                                   │  Door closes → light OFF           │
└─────────────────────────────────┴────────────────────────────────────┘
```

**Door alarm** (a separate feature — gated by Door Sensor Enabled, not the light switch above):

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

---

### 4.7 Probes & Sensors

*Touchscreen: Settings 7/7 — "Probes". Web: **Probes & Sensors** section.*

| Setting | What it does | Range | Default | When to change it |
|---|---|---|---|---|
| **Probe 1 (Coolroom) Calibration Offset** | Adds a fixed correction to the main temperature reading. | -10 – 10 °C | 0.0 °C | Only set this after comparing the probe against a trusted reference thermometer — enter the difference so the displayed reading matches reality. |
| **Probe 2 (Evaporator) Calibration Offset** | Same idea, for the evaporator probe. | -10 – 10 °C | 0.0 °C | Same approach — compare against a reference thermometer first. |
| **Evaporator Probe (Probe 2) Enabled** | Master switch for the evaporator probe. | On/Off | On | Smart Defrost (§4.3) and the Ice Alarm (§4.5) both need Probe 2 to function — disabling it disables those features too, even if their own switches are on. |
| **Internal SHT31 Sensor Enabled** | Cabinet humidity/temperature sensor. | On/Off | On | Informational only — see note below. |
| **External SHT20 Sensor Enabled** | Room/ambient humidity/temperature sensor. | On/Off | On | Informational only — see note below. |

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

> 📷 **Screenshot placeholder — Web Dashboard: Probes & Sensors section**
> 📷 **Screenshot placeholder — Touchscreen: Settings 7/7**

---

### 4.8 Notifications (ntfy Push Alerts)

*Web only — no touchscreen equivalent (push notifications go to your phone/computer, not
the unit itself).*

The controller sends push notifications through [ntfy](https://ntfy.sh) — a free,
no-account-needed push service. Every message includes a timestamp.

| Alert | Priority | When it fires |
|---|---|---|
| 🌡️ Coolroom HIGH TEMP Alarm | High | High-temp alarm becomes active (past persist time, §4.4/§4.5) |
| ❄️ Coolroom LOW TEMP Alarm | High | Low-temp alarm becomes active |
| 🚪 Coolroom DOOR OPEN Alarm | High | Door stays open past the Door Alarm Delay (§4.6) |
| 🥶 Coolroom NO-COOL Alarm | Urgent | Compressor running but room isn't cooling (§4.5) |
| ❄️ Coolroom ICE ALARM | High | Ice Alarm Delta condition met (§4.5) |
| ✅ Coolroom Alarm CLEARED | Low | Any alarm above recovers past its threshold/hysteresis band |
| ⚠️ Coolroom PROBE FAULT | Urgent | Main probe fails (§4.7) |
| ⚠️ Coolroom SD CARD FAILURE | High | SD card missing at boot, or fails during operation (§4.9) |
| ✅ Coolroom SD Card Recovered | Low | Auto-remount brings a previously-failed card back online (§4.9) |

Every alarm type reaches ntfy — none are event-log-only any more.

**Setup:** install the free ntfy app (iOS/Android) or use ntfy.sh in a browser, and
subscribe to your device's topic name (set at build time — ask your installer, or check
the firmware's `ntfy_topic` value). No further app-side configuration needed.

---

### 4.9 Data & SD Card

*Web dashboard: **System** section (buttons) + touchscreen Info page (status only).*

The SD card is entirely **optional** — the controller runs normally without one. If no card
is present at boot, or a card fails while running, logging and backup/restore simply become
no-ops (silently skipped) rather than crashing or affecting cooling control.

| Feature | What it does |
|---|---|
| **Event Log** | Every alarm, defrost start/end, and compressor on/off is written to a daily CSV file on the SD card, with the sensor readings behind the decision — useful for troubleshooting after the fact. |
| **Temperature Log** | Periodic temperature/humidity samples logged the same way. |
| **Backup All Settings to SD** *(button)* | Saves every setting in this manual to a `backup.json` file on the card. |
| **Restore All Settings from SD** *(button)* | Loads settings back from that file — useful after a factory reset or when cloning settings to another unit. |
| **Auto-Remount** | If the card fails mid-session (removed, corrupted), the controller checks every 60 seconds and automatically resumes logging/backup the moment a working card is present again — **no reboot needed**. It does *not* automatically restore your settings on reconnect (to avoid overwriting anything you changed while the card was out); it only resumes logging and lets you press Restore manually if you want to. |
| **SD Card Failure Alert** | You'll get a push notification (§4.8) the moment the card is missing or fails — so a dead/removed card doesn't go unnoticed. |

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

### 4.10 Network & WiFi

*Web only. WiFi changes are deliberately **not** available on the touchscreen.*

| Setting | What it does |
|---|---|
| **New WiFi SSID / New WiFi Password** | Two fields, submitted one after the other, to reconnect the controller to a different (or corrected) WiFi network without re-flashing firmware. |
| **WiFi SSID / Signal Strength** *(read-only)* | Shows what network you're currently connected to and how strong the signal is — also mirrored, read-only, on the touchscreen Info page. |
| **IP Address / MAC Address** *(read-only)* | For finding the device on your network or reserving a static lease on your router. |
| **Controller Online** *(diagnostic)* | Whether the device is currently reachable. |

**First-time / no known network:** the controller starts its own access point automatically
— connect to it and browse to `192.168.4.1` to enter your real network's details (see §2.1).

> 📷 **Screenshot placeholder — Web Dashboard: Network & Connectivity section**

---

### 4.11 Access Control & Security

There are **two completely separate locks** on this system — don't confuse them:

| | Web Dashboard Login | Touchscreen Settings PIN |
|---|---|---|
| **What it protects** | Every web-dashboard setting + admin function | Only the 7 touchscreen settings pages |
| **Credential** | The device's one HTTP username/password (`secrets.yaml`, set at build time) | 4-digit PIN, factory default `0000`, changeable on-device |
| **How to change it** | Not changeable from the dashboard — requires re-flashing firmware with new `secrets.yaml` values | Touchscreen: Settings 1/7 → 🔒 Change PIN |
| **Session behavior** | Logs out automatically after 2 minutes of inactivity; never persisted across a page reload | Stays unlocked until you navigate back to Home |

**Web dashboard has exactly two states** — there is no third "admin" or "superadmin" tier
(an earlier version of this documentation described one; it never actually existed
server-side and has been removed):

- **Guest** (default): view-only — status, alarms, system health.
- **Operator** (logged in): everything Guest sees, plus every setting in this manual and
  the admin functions in §4.9/§4.10.

**Important limitation, worth understanding:** the login only controls what the *dashboard
page* shows you — it is not a security boundary against anyone who already has network
access to the device. Treat your site network (or VPN) as the actual security perimeter,
same as you would for any other network appliance. Full detail:
`reference/RBAC_USER_GUIDE.md` and `reference/AUTHENTICATION_GUIDE.md`.

This two-tier model is the **intended, permanent design** for this system — not a stopgap
awaiting a future "real" multi-account/role-based system. A guest can always see everything
on the main display without logging in; a single operator login elevates to settings and
administration. Real server-side authorization (separate accounts, enforced permissions)
would only be worth building if a genuine multi-user need arises later.

**Deliberately web-only, never added to the touchscreen** (by explicit design, not an
oversight): WiFi configuration (§4.10), the web dashboard's login credential, and the SD
card log-delete function (§4.9). These stay off the touchscreen so a PIN alone can never be
used to change network access or wipe evidence in the event log.

> 📷 **Screenshot placeholder — Web Dashboard: Login prompt**

---

### 4.12 System & Diagnostics

*Touchscreen: Info page (read-only). Web: **System** + **Diagnostics** sections.*

Mostly read-only status, useful for troubleshooting rather than day-to-day adjustment.

| Item | What it shows |
|---|---|
| System Uptime / Current Time / NTP Sync Status | How long since last reboot, and whether the clock is synced (affects timestamp accuracy in logs/notifications) |
| Free Heap / Free PSRAM / Chip Temperature | Controller's own internal health |
| SD Card Free Space / SD Card Mounted | Storage headroom and current mount status (§4.9) |
| RS485 Bus Status / RTD Sample Age / Probe Health Summary | Whether the sensor bus and individual probes are responding and how fresh their last reading is |
| RTC / SHT31 / SHT20 Online | Whether each optional peripheral is detected and responding |
| Sensor Fallback Active | Whether the compressor is currently running the fallback duty cycle from §4.1/§4.7 |
| **Restart Controller** *(button)* | Reboots the device. Settings are preserved (stored in flash), in-progress defrost/alarm timers are not. |
| **Factory Reset** *(button)* | Resets every setting in this manual back to its default value. Use Backup (§4.9) first if you want to restore afterward. |
| **Display Backlight** | Touchscreen brightness. |

> 📷 **Screenshot placeholder — Web Dashboard: System & Diagnostics sections**
> 📷 **Screenshot placeholder — Touchscreen: Info page**

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
| No push notifications arriving | Confirm ntfy topic subscription (§4.8); confirm WiFi is connected (§4.10) | §4.8, §4.10 |
| SD card seems to have "given up" | Check for the SD Card Failure push — auto-remount retries every 60s once the card is working again, no reboot needed | §4.9 |
| Touchscreen readings don't match a reference thermometer | Set a Calibration Offset after comparing against a trusted reference | §4.7 |

---

## 6. Document Map

| Group | Touchscreen page | Web dashboard section |
|---|---|---|
| Temperature Control & Compressor | Settings 1/7 | Temperature Control, Compressor |
| Defrost Schedule | Settings 2/7 | Defrost |
| Defrost — Smart & Drip | Settings 3/7 | Defrost |
| Alarm Thresholds | Settings 4/7 | Alarms |
| Alarms — Advanced | Settings 5/7 | Alarms |
| Door | Settings 6/7 | Door |
| Probes & Sensors | Settings 7/7 | Probes & Sensors |
| Notifications | — (web only) | Header (ntfy config at build time) |
| Data & SD Card | Info (status only) | System |
| Network & WiFi | Info (status only) | Network & Connectivity |
| Access Control & Security | Settings PIN entry | Login |
| System & Diagnostics | Info | System, Diagnostics |

---

*See also: `reference/QUICK_START_GUIDE.md` for first-time setup steps and worked
recommended-settings examples; `reference/RBAC_USER_GUIDE.md` and
`reference/AUTHENTICATION_GUIDE.md` for full detail on the web login model.*
