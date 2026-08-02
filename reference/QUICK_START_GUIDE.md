# Coolroom Controller — Quick Start Guide

**For:** operators who need the room running correctly today, not the full explanation.
**Full detail:** `reference/USER_MANUAL.md` — every setting, every diagram, every "why".

---

## 1. What This Thing Does, In One Paragraph

It watches your coolroom's temperature and keeps a compressor cycling to hold a setpoint,
runs **passive** defrost on a schedule (plus optional "smart" early triggers), optionally
drives an evaporator fan with the compressor, watches for problems (door open too long,
sensor failure, no cooling, ice buildup), and pushes an alert to your phone when something
needs attention. You can control it from the 7" touchscreen on the unit, or from a web page
on any browser on your network.

---

## 2. First-Time Setup Checklist

- [ ] **Connect to WiFi.** No known network on first boot → connect to the controller's own
      access point → browse to `192.168.4.1` → enter your real network's SSID/password.
      To change networks later, log in → **Wireless** tab → **Scan** → pick the network →
      enter the password → **Join Network**. The controller tries it and puts the old
      network back by itself if it can't join, so a typo can't strand it. See §4.10.
      Optional: on the same **Wireless** tab, turn on **Home Assistant API Enabled** only if
      you want Home Assistant to see this device’s sensors/switches (off by default).
- [ ] **Find the device IP** (router's device list, or touchscreen Info page) and open
      `http://<device-ip>/` in a browser (also `/dashboard`). Main status loads without a
      password; settings stay hidden until 🔐 Login with your `secrets.yaml` web credentials.
- [ ] **Change the touchscreen PIN** from the factory default `0000` — tap **Settings**
      on the touchscreen → enter `0000` → **Change PIN** button.
- [ ] **Check both probes are reading sensibly** (System/Diagnostics section — Probe Health
      Summary) before relying on the unit.
- [ ] **Check the hardware is all present** — log in → **Hardware** tab. Both RS485 boards,
      System Time, the humidity sensors and the SD card should read *online* (RS485 boards
      only once those are wired). If you don't want
      the alarm relay driving an external siren, turn **Siren Relay Enabled** off here:
      alarms and push notifications carry on as normal. See §4.13.
- [ ] **Wire the Modbus relay coils correctly** — channel / coil **0 = evaporator fan**,
      **1 = compressor**, **2 = light**, **3 = siren**. Defrost is passive on this
      controller (no heater coil). Leave **Fan Relay Enabled** off until coil 0 is
      confirmed as a fan. See User Manual §4.13 and `reference/CAREL_CONTROL_DECISIONS.md`.
- [ ] **Door features are opt-in** — **Door Sensor Enabled**, **Door-Triggered Light**, and
      **Hold Compressor While Door Open** all default **off**. Turn the sensor on only when
      the reed is fitted and NC/NO mode matches the wiring. Leave door-hold off unless you
      want cooling paused while the door is open (a stuck reed would starve cooling).
- [ ] **Subscribe to push notifications** — log in → **Alarms & Notify** tab → set topic
      (and server if not using ntfy.sh) → optionally tune each alert's priority →
      **Send Test Notification**. Install the free ntfy app and subscribe to that same
      topic. See User Manual §4.8.
- [ ] **Set your target temperature and alarm thresholds** — see the worked example below,
      or your own product's requirements.
- [ ] **Do one manual backup** (System section → "Backup All Settings to SD") once you're
      happy with your configuration, so you can restore it after a factory reset if needed.

> 📷 **Screenshot placeholder — Web Dashboard, first login**
> 📷 **Screenshot placeholder — Touchscreen, Settings PIN entry**

---

## 3. The 7 Setting Groups At a Glance

| # | Group | Touchscreen | What lives here |
|---|---|---|---|
| 1 | Compressor & Fallback | Settings 1/8 | Setpoint, differential, lockout, min run, Fan Relay Enable, sensor-fault fallback |
| 2 | Defrost Schedule | Settings 2/8 | On/off, interval, duration, early-stop, drip toggle; Start/Stop side-by-side + Force-Max; Skip-If-Cold row |
| 3 | Defrost Smart & Drip | Settings 3/8 | Drip time, smart delta/dwell + Frost Window, dew-point, frost-rate (no scroll) |
| 4 | Alarm Thresholds | Settings 4/8 | High/low temp alarm deltas, persist time, siren |
| 5 | Alarms Advanced | Settings 5/8 | Ice enable/dwell/delta, no-cool, startup/defrost grace |
| 6 | Door | Settings 6/8 | Sensor on/off, NC/NO, door light, hold compressor while open, alarm delay |
| 7 | Probes | Settings 7/8 | Display unit, calibration offsets, calibrate, probe/humidity enables (Probe1=room air RTD CH1, Probe2=evap RTD CH2 — fixed) |
| 8 | Audio | Settings 8/8 | Master enable, volume, test, speak toggles, amp diagnostic |

WiFi, the web login password, and SD card log-delete are **web-dashboard only** — by design,
not an oversight. Full explanation: User Manual §4.11. The web dashboard also carries tabs
with no touchscreen equivalent: **Hardware**, **Wireless**, and **Events** (live alarm/fault
log as it happens). Optional SD temperature logs keep the last **N days** (default **60**,
web SD Card tab); below **32 MB** free the controller skips new log/backup writes without
treating the card as failed (User Manual §4.9).

**Door Sensor Enabled is the master for the reed.** Door-open alarm, Door-Triggered Light,
and Hold Compressor While Door Open all require it. Enabling Door-Triggered Light turns
the sensor on if needed; turning the sensor off turns Door-Triggered Light **and** door-hold
off too. The home light icon animates only while the light relay is actually on — tap it to
toggle the light manually (when Light Relay Enabled is on). Snowflake and flame are
status-only; tap the bell to soft-mute the siren (bell stays red, stops jiggling; a new
alarm type re-animates).

**Defrost Start / Stop** are on the web Defrost tab and touchscreen Settings 2/8 — not the
home flame icon. Defrost System Enabled gates automatic starts only. Defrost is always
**passive** here (compressor held off; any heater is external). **Fan Relay Enabled**
(coil 0, default **off**) gates the evaporator fan — when on, the fan follows the compressor
and stops during defrost/drip. Same toggle on touchscreen Settings 1/8, web Compressor, and
Hardware → Fan.

**Celsius / Fahrenheit:** use **Temperature Display Unit** on touchscreen Settings 7/8
or web **Probes & Sensors**. Celsius is the default. The choice changes LVGL/web readouts
(including the large home-gauge centre temperature on both the classic and swipe-up
cooling views), not the controller's Celsius calculations or saved thresholds. It is also
available in Home Assistant as `select.temperature_display_unit`; HA converts its
temperature sensors according to HA's own unit-system setting.

**Home gauge:** the classic three-arc view (coolroom / setpoint / ambient) is the default.
Swipe **up** on the centre for an HA-style thermostat view (set vs current on one ring with
display-only thumb/pip — not drag-to-set); swipe **down** to return. Centre temperature
stays blue like the coolroom arc.

**After a power cut the compressor won't start straight away** — the off-delay counts from
power-up, so expect up to 3 minutes (default) of amber countdown under the snowflake before
cooling resumes. That is deliberate compressor protection, not a fault. See §4.1.

---

## 4. Worked Example — Cold Storage Profile: Dessert Plums (~15° Brix)

This is a realistic **starting point**, not a guarantee — always confirm against your own
produce's condition, your local food-safety guidance, and how the room actually performs
once loaded. Plums at ~15° Brix are ripe, sweet dessert-quality fruit, typically destined
for short-to-medium-term cold storage rather than long-haul controlled-atmosphere storage.

**Why these particular settings:** plums tolerate cold well (close to 0 °C without chilling
injury, unlike some other stone fruit), but need consistently high humidity to avoid
shriveling, and a reasonably tight low-temperature alarm since sweeter fruit can still
suffer freeze damage a little below 0 °C.

| Setting | Recommended | Why |
|---|---|---|
| **Setpoint** | `0.5 °C` | Close to optimal storage temperature for stone fruit while leaving a safety margin above freezing. |
| **Compressor Differential** | `1.0 °C` (default) | Keeps the swing tight around 0.5 °C without excessive compressor cycling. |
| **Compressor Off-Delay** | `3 min` (default) | No produce-specific reason to change this. |
| **Compressor Min Run Time** | `2 min` (default) | Leave on; pairs with Off-Delay to stop short-cycling. |
| **Fan Relay Enabled** | Leave `Off` until coil 0 is confirmed as a fan; then `On` if the plant has an evaporator fan on that channel | Default off is intentional — miswiring a heater to coil 0 must not run heat unsupervised. |
| **Defrost System Enabled** | `On` | Needed — a near-0 °C, high-humidity room frosts the coil steadily. |
| **Defrost Interval** | `480 min` (default, 8 h) | Reasonable baseline; shorten if you see visible frost buildup between cycles. |
| **Defrost Early Termination by Temp** | `On` (default) | Avoids over-warming the room on every cycle — important for a chill-sensitive but not frost-tolerant product. |
| **Defrost Drip Phase** | `On` (default), `5 min` | Standard — prevents meltwater re-icing the coil. |
| **Smart Defrost (Delta-Triggered)** | **Turn On** (default is Off) | Produce rooms have variable loads (new stock coming in warm) — catches frost buildup the fixed timer alone would miss. |
| **Smart Defrost Delta Threshold** | `8.0 °C` (default) | Reasonable starting point; tighten if visible ice appears between smart-triggered cycles. |
| **Dew Point Early Defrost Trigger** | **Turn On** (default is Off) | The single most useful setting for a high-humidity produce room — defrosts exactly when frost is actually forming, not on a guess. |
| **High Temp Alarm Delta** | `2.0 °C` (i.e. alarms above ~2.5 °C) | Catches a warming excursion early enough to act before ripening accelerates or decay risk rises. |
| **Low Temp Alarm Delta** | `1.5 °C` (i.e. alarms below ~ -1.0 °C) | Tighter than default — protects against freeze injury, since higher-sugar fruit still isn't freeze-proof much below 0 °C. |
| **Alarm Persist Time** | `5 min` (default) | Fine as-is. |
| **Ice Alarm Delta** | `15 °C` (default) | Large coolroom−evap gap while cooling = iced coil (Precision polarity). Raise toward 18–20 if you get false alarms during heavy pull-down; enable Ice Detection + leave Dwell at 10 min. |
| **Ice Alarm Dwell** | `10 min` (default) | Condition must hold this long before the alarm fires. |
| **No-Cool Alarm Timeout** | `45 min` (tighter than default 60) | Stone fruit is high-value enough to justify catching a refrigeration failure a bit faster. |
| **Door Sensor Enabled** | `On` | Recommended for any room with regular staff traffic moving stock. Required if you want Door-Triggered Light or door-hold. |
| **Door-Triggered Light Enabled** | `On` | Convenient for staff picking/sorting fruit — light comes on automatically while the door's open. Turns Door Sensor Enabled on if it was off. |
| **Hold Compressor While Door Open** | Leave `Off` unless the reed is reliable and you want cooling paused on open doors | Opt-in. Busy loading doors / less humid air across a cold coil can benefit; a stuck-open reed would starve cooling. |
| **Door Alarm Delay** | `300 s` (default) | Fine for routine loading/unloading; shorten if the room should never be open long. |
| **Internal/External Humidity Sensors** | `On` (default) | Monitor toward a target ~90–95% RH — remember this controller only *reports* humidity, it doesn't control it (User Manual §4.7). Pair with your own humidification setup if the room runs dry. |
| **Probe Calibration Offsets** | `0.0 °C` until checked | Compare against a calibrated reference thermometer before adjusting. |

> **Disclaimer:** these are illustrative starting values based on general commercial
> cold-storage practice for stone fruit, not a substitute for your own quality/food-safety
> procedures. Always verify against your product's actual condition and any regulatory
> requirements that apply to your operation.

### Other common produce — quick starting points

Rough industry-standard starting points only — same disclaimer as above applies, more so
the less this list matches your actual product and packaging.

| Product | Typical setpoint | Typical RH target | Notes |
|---|---|---|---|
| Apples (long-term) | 0 – 4 °C | 90–95% | Very cold-tolerant; consider a tighter Low Alarm Delta only if storing right at 0 °C. |
| Leafy greens / general veg | 0 – 4 °C | 95–98% | High RH need, similar defrost considerations to the plum profile above. |
| Dairy / general chiller | 1 – 4 °C | Not usually critical | Wider alarm deltas are usually fine; Smart/Dew-Point triggers less critical unless the room runs humid. |

---

## 5. Two Diagrams You'll Actually Use Day-to-Day

**Compressor decision, every cycle:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│           Probe reading missing/stale/out of range?                  │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES → compressor OFF (safety)    │ NO → compare to setpoint ± half    │
│                                   │      the differential:            │
│                                   │  above upper band → ON            │
│                                   │  below lower band → OFF           │
│                                   │  in between → hold current state  │
├─────────────────────────────────┴────────────────────────────────────┤
│ Off-Delay lockout still active? → wait, don't restart yet            │
│ Min Run Time not elapsed? → refuse cut-out until the ON window ends  │
│ Hold Compressor While Door Open + door open? → force compressor OFF  │
│ Fan Relay Enabled? → fan follows compressor (off during defrost/drip)│
└──────────────────────────────────────────────────────────────────────┘
```

**What can trigger a defrost:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│                 ANY of the following becomes true:                   │
├────────────────────────┬───────────────────────┬─────────────────────┤
│ Fixed interval elapsed │ Smart Defrost:         │ Dew Point / Frost-  │
│ since last defrost     │ coolroom↔evap gap too  │ rate humidity drop  │
│ (Skip-If-Cold can      │ large, too long        │ (both off by        │
│ postpone this one only)│                        │ default)            │
├────────────────────────┴───────────────────────┴─────────────────────┤
│ Force-Max Interval Override (safety net)  OR  Manual Start Now       │
├──────────────────────────────────────────────────────────────────────┤
│ → Passive defrost starts (compressor held off; fan off). Ends at Max   │
│   Duration, or earlier if Early Termination by Temp is on and the    │
│   coil reaches its target. Optional drip hold before cooling resumes.│
└──────────────────────────────────────────────────────────────────────┘
```

Also: Min Run Time holds the compressor ON until its minimum ON window elapses (pairs with Off-Delay). Decision log vs Carel: `reference/CAREL_CONTROL_DECISIONS.md`.

---

## 6. Alerts You'll Actually See

| You'll see this push... | ...it means | You should |
|---|---|---|
| 🌡️ HIGH TEMP Alarm | Room too warm, past persist time | Check door/load/compressor |
| ❄️ LOW TEMP Alarm | Room too cold, past persist time | Check setpoint, check for a stuck compressor |
| 🚪 DOOR OPEN Alarm | Door open past the alarm delay | Close the door / check the door sensor |
| 🥶 NO-COOL Alarm | Compressor running but room isn't cooling | Check for a refrigeration fault urgently |
| ❄️ ICE ALARM | Evaporator coil may be iced | Check airflow, consider a defrost cycle |
| ✅ Alarm CLEARED | Whatever tripped above has recovered | No action needed |
| ⚠️ PROBE FAULT | Main sensor has failed | Check probe wiring/connection urgently |
| ⚠️ RELAY BOARD OFFLINE | Modbus relay board not responding | Check RS485 / power to the relay module |
| ⚠️ TEMP BOARD OFFLINE | Modbus RTD board not responding | Check RS485 / RTD converter |
| ⚠️ HUMIDITY SENSOR OFFLINE | Cabinet SHT31 not responding | Check I2C / enable only if fitted |
| ⚠️ AMBIENT SENSOR OFFLINE | Ambient SHT20 not responding | Check I2C / enable only if fitted |
| ✅ … ONLINE (relay/temp/humidity/ambient) | Matching board or sensor recovered | No action needed |
| ⚠️ SD CARD FAILURE | Logging/backup paused, cooling still runs fine | Check the card when convenient — not urgent for cooling itself |
| ✅ SD Card Recovered | Card came back automatically, no reboot needed | No action needed |

---

## 7. When To Open the Full Manual

- You need to understand **why** a setting behaves the way it does, not just what value to
  type in.
- You're chasing a nuisance alarm or an unexpected defrost pattern.
- You want the full control-logic diagrams (compressor, defrost, all four alarm types,
  probe fault, SD auto-remount) rather than the two condensed ones above.
- You need the access-control/security model explained in full (touchscreen PIN vs. web
  login — they are separate).

→ `reference/USER_MANUAL.md`
