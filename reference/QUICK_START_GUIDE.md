# Coolroom Controller — Quick Start Guide

**For:** operators who need the room running correctly today, not the full explanation.
**Full detail:** `reference/USER_MANUAL.md` — every setting, every diagram, every "why".

---

## 1. What This Thing Does, In One Paragraph

It watches your coolroom's temperature and keeps a compressor cycling to hold a setpoint,
defrosts the coil on a schedule (plus two optional "smart" early triggers), watches for
problems (door open too long, sensor failure, no cooling, ice buildup), and pushes an alert
to your phone when something needs attention. You can control it from the 7" touchscreen on
the unit, or from a web page on any browser on your network.

---

## 2. First-Time Setup Checklist

- [ ] **Connect to WiFi.** No known network on first boot → connect to the controller's own
      access point → browse to `192.168.4.1` → enter your real network's SSID/password.
- [ ] **Find the device IP** (router's device list, or touchscreen Info page) and open
      `http://<device-ip>/assets/dashboard.html` in a browser.
- [ ] **Log in** with the device credential (`🔐 Login`, top of dashboard) to unlock every
      setting. Ask your installer if you don't have this.
- [ ] **Change the touchscreen PIN** from the factory default `0000` — tap **⚙️ Settings**
      on the touchscreen → enter `0000` → **🔒 Change PIN** button.
- [ ] **Check both probes are reading sensibly** (System/Diagnostics section — Probe Health
      Summary) before relying on the unit.
- [ ] **Subscribe to push notifications** — install the free ntfy app and subscribe to your
      device's topic name (ask your installer). See User Manual §4.8.
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
| 1 | Compressor & Fallback | Settings 1/7 | Setpoint, differential, lockout, sensor-fault fallback |
| 2 | Defrost Schedule | Settings 2/7 | On/off, fixed interval, max duration, early-stop-by-temp, drip |
| 3 | Defrost Smart & Drip | Settings 3/7 | Drip time, delta-triggered defrost, dew-point trigger |
| 4 | Alarm Thresholds | Settings 4/7 | High/low temp alarm deltas, persist time, siren |
| 5 | Alarms Advanced | Settings 5/7 | Ice alarm, no-cool alarm, startup/defrost grace, hysteresis |
| 6 | Door | Settings 6/7 | Door sensor on/off, NC/NO mode, alarm delay |
| 7 | Probes | Settings 7/7 | Calibration offsets, probe/humidity sensor enables |

WiFi, the web login password, and SD card log-delete are **web-dashboard only** — by design,
not an oversight. Full explanation: User Manual §4.11.

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
| **Compressor Differential** | `1.0 °C` (default) | ON at 1.5 °C / OFF at 0.5 °C (asymmetric band) — tight control without excessive cycling. |
| **Compressor Off-Delay** | `3 min` (default) | No produce-specific reason to change this. |
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
| **Ice Alarm Delta** | `2.5 °C` (slightly above default) | A humid room frosts more readily — a slightly higher threshold gives you an earlier warning before ice becomes a real airflow problem. |
| **No-Cool Alarm Timeout** | `45 min` (tighter than default 60) | Stone fruit is high-value enough to justify catching a refrigeration failure a bit faster. |
| **Door Sensor Enabled** | `On` | Recommended for any room with regular staff traffic moving stock. |
| **Door-Triggered Light Enabled** | `On` (default) | Convenient for staff picking/sorting fruit — light comes on automatically while the door's open. |
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
│ YES → compressor OFF (safety)    │ NO → asymmetric Carel band:       │
│                                   │  ≥ setpoint + differential → ON    │
│                                   │  ≤ setpoint → OFF (after min-run) │
│                                   │  in between → hold current state  │
├─────────────────────────────────┴────────────────────────────────────┤
│ Off-Delay lockout still active? → wait, don't restart yet            │
└──────────────────────────────────────────────────────────────────────┘
```

**What can trigger a defrost:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│                 ANY of the following becomes true:                   │
├────────────────────────┬───────────────────────┬─────────────────────┤
│ Fixed interval elapsed  │ Smart Defrost:         │ Dew Point Trigger:  │
│ since last defrost      │ coolroom↔evap gap too  │ evaporator is below │
│ (§ Defrost Schedule)    │ large, too long        │ freezing AND below  │
│                         │ (§ Smart & Drip,        │ the calculated dew  │
│                         │ off by default)         │ point (off by       │
│                         │                         │ default)            │
├────────────────────────┴───────────────────────┴─────────────────────┤
│ → Defrost starts. Ends at Max Duration, or earlier if Early           │
│   Termination by Temp is on and the coil reaches its target.         │
└──────────────────────────────────────────────────────────────────────┘
```

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
