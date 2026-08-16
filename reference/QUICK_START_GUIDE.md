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
- [ ] **Check the hardware is all present** — log in → **Hardware** tab. System Time and
      the SD card should read *online*; RS485 boards and SHT31/SHT20 only once those are
      wired **and** left Enabled in Probes (disconnected I2C must show offline/disabled,
      not OK). If you don't want
      the alarm relay driving an external siren, turn **Siren Relay Enabled** off here:
      alarms and push notifications carry on as normal. See §4.13.
- [ ] **Wire the Modbus relay coils correctly** — channel / coil **0 = evaporator fan
      (optional / future)**, **1 = compressor**, **2 = light**, **3 = siren**. Defrost is
      passive on this controller (no heater coil). **Today the plant’s evaporator fans run
      continuously** (hardwired, not through the relay) — leave **Fan Relay Enabled** off.
      Coil 0 is ready if you later switch fans to controller control. See User Manual §4.13
      and `reference/CAREL_CONTROL_DECISIONS.md`.
- [ ] **Door features are opt-in** — **Door Sensor Enabled**, **Door-Triggered Light**, and
      **Hold Compressor While Door Open** all default **off**. Turn the sensor on only when
      the reed is fitted and NC/NO mode matches the wiring. Leave door-hold off unless you
      want cooling paused while the door is open (a stuck reed would starve cooling).
- [ ] **Subscribe to push notifications** — log in → **Alarms & Notify** tab → set topic
      (and server if not using ntfy.sh) → optionally tune each alert's priority →
      **Send Test Notification**. Install the free ntfy app and subscribe to that same
      topic. See User Manual §4.8.
- [ ] **Set your target temperature and alarm thresholds** — see the worked examples below
      (§4 general 2 °C food coolroom, or §4b Black Amber / Amber Jewel / Tegan Blue plums),
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
| 7 | Probes | Settings 7/8 | Display unit, calibration offsets, probe/humidity enables, optional CT clamp + gated run-proof (Probe1=room air RTD CH1, Probe2=evap RTD CH2 — fixed) |
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
(coil 0, default **off**) is for a **future** switch to controller-managed evaporator fans.
**Current plant:** fans run continuously on their own supply — keep the enable off. If you
later wire fans through coil 0 and turn the enable on, the fan follows the compressor and
stops during defrost/drip. Same toggle on touchscreen Settings 1/8, web Compressor, and
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
stays blue like the coolroom arc. Right-rail **Evap** / **Int** / **Ext** sensor readouts
appear on both home views.

**After a power cut the compressor won't start straight away** — the off-delay counts from
power-up, so expect up to 3 minutes (default) of amber countdown under the snowflake before
cooling resumes. That is deliberate compressor protection, not a fault. See §4.1.

---

## 4. Worked Example — 2 °C Food Coolroom (All Sensors Online)

This is the project’s **recommended general starting profile** for a commercial food
coolroom held at **2.0 °C**, with RTDs, relay board, I2C humidity/ambient, and door reed
treated as fitted. Prefer safe/stable defaults over aggressive energy saving. Printable
tables + apply notes: `reference/recommended_settings_2c.html`.

**Plant assumptions:** single compressor, **passive** defrost, evaporator fans **hardwired
continuous** (leave **Fan Relay Enabled** off). Humidity is monitored, not controlled.

| Setting | Recommended | Why |
|---|---|---|
| **Setpoint** | `2.0 °C` | Target hold for a general food chiller (matches firmware factory default). |
| **Compressor Differential** | `1.0 °C` (default) | Asymmetric: ON at SP+1 °C (3.0), OFF at SP (2.0) — tight control without excessive cycling. |
| **Compressor Off-Delay** | `3 min` (default) | Compressor protection; do not shorten without OEM approval. |
| **Compressor Min Run Time** | `2 min` (default) | Pairs with Off-Delay against short-cycling. |
| **Fan Relay Enabled** | Leave `Off` | Fans currently run constantly (hardwired). Enable only after rewiring fans onto coil 0. |
| **Sensor Fallback Duty-Cycle** | `On`, `3` / `27` min (default) | ~10% duty if Probe 1 fails — safer than stopping cooling completely. |
| **Defrost System Enabled** | `On` | Needed — a ~2 °C humid room frosts the coil. |
| **Defrost Interval** | `360 min` (**6 h**; factory default is 480 / 8 h) | Shorter for typical food-room door traffic / moisture; still conservative. |
| **Defrost Early Termination by Temp** | `On` @ `5.0 °C` (default) | Ends defrost when Probe 2 shows the coil is clear — less room warm-up. |
| **Defrost Drip Phase** | `On` (default), `5 min` | Prevents meltwater re-icing the coil. |
| **Smart Defrost (Delta-Triggered)** | **Turn On** (default is Off) | Probe 2 online — catches frost between fixed intervals under variable loads. |
| **Smart Defrost Delta / Dwell** | `8.0 °C` / `30 min` (defaults) | Starting point; tighten only if ice appears between smart starts. |
| **Dew Point Early Defrost Trigger** | **Turn On** (default is Off) | SHT31 online — defrost when frost is actually forming. |
| **Frost Rate Monitoring** | **Turn On** (default is Off), `5%` / `300 s` | Complements dew-point when humidity drops while temperature is stable. |
| **Defrost Skip-If-Cold** | Leave `Off` | Prefer scheduled defrosts until the plant is proven; Force-Max stays 720 min. |
| **High Temp Alarm Delta** | `2.5 °C` (alarms above ~4.5 °C; factory 3.0) | Earlier food-stock warning than factory (~5.0 °C). |
| **Low Temp Alarm Delta** | `2.0 °C` (alarms below ~0 °C; factory 3.0) | Freeze guard near 0 °C without nuisance trips inside the normal band. |
| **Alarm Persist Time** | `5 min` (default) | Fine as-is. |
| **Ice Detection / Delta / Dwell** | `On` / `15 °C` / `10 min` (defaults) | Large coolroom−evap gap while cooling = iced coil (Precision polarity). |
| **No-Cool Alarm Timeout** | `45 min` (tighter than default 60) | Faster refrigeration-failure notice for food stock. |
| **Door Sensor Enabled** | `On` | Assumed reed fitted; required for door alarm / door light. Match NC/NO to wiring. |
| **Door-Triggered Light Enabled** | `On` | Staff convenience on load/unload. |
| **Hold Compressor While Door Open** | Leave `Off` | Opt-in only — a stuck reed would starve cooling. |
| **Door Alarm Delay** | `300 s` (default) | Routine loading; shorten if doors should never stay open long. |
| **Evaporator Probe / Internal SHT31 / External SHT20** | `On` (defaults) | Enable what is fitted. Current bench: RS485 RTD + relay are live; I2C SHT sensors are not fitted (leave those enables off). |
| **CT Clamp Enabled** | `Off` (default) | Optional RS485 current clamp on the **whole plant feed** — leave off unless fitted and addressed to **110** @ 9600. |
| **CT Run-Proof** | Leave `Off` until CT is online | Opt-in after enable; defaults 0.40 A idle / 0.55 A run / 30 s (≈64 W idle / 190 W run @ 240 V). |
| **Probe Calibration Offsets** | `0.0 °C` until checked | Compare against a calibrated reference thermometer before adjusting. |
| **SD Temp Log Retention** | `90 days` (factory 60) | Longer daily CSV history for food-room audit trail. |
| **ntfy / Audio / Siren** | ntfy `Off` (default); Audio / Siren `On` | Turn ntfy on and subscribe the phone to your topic when you want push alerts; keep local audio/siren on. |

> **Disclaimer:** illustrative starting values for a general ~2 °C commercial food coolroom
> on this controller — not a substitute for your own quality/food-safety procedures. Always
> verify against the product, local rules, and how the loaded room actually performs.

---

## 4b. Worked Example — Dessert Plums (Black Amber · Amber Jewel · Tegan Blue)

End-to-end **plums profile** for short-to-medium-term cold storage of this site’s three
Japanese dessert plums (starting assumption ~15° Brix; Amber Jewel often packs sweeter).
Apply every group below (or leave consciously at the stated default).
Printable tables: `reference/recommended_settings_plums.html`. Cross-check names against
User Manual §4.1–§4.9 / §4.13.

**Site crop set (this coolroom):** three Japanese dessert plums grown/stored together —

| Cultivar (use this spelling) | Also seen as | Typical published chill hold |
|---|---|---|
| **Black Amber** | ‘Blackamber’ (UC Davis literature) | Prefer **~0 °C**; avoid the **~2–8 °C** “killing zone” (chilling injury / internal breakdown worse than at 0 °C). |
| **Amber Jewel** | Informal “Amber Jewels”; industry/nursery catalogues use **Amber Jewel** (singular) | WA / Curtin trials store at **0 °C**; **5 °C** raises ethylene and CI vs 0 °C. ANFIC notes excellent storage; fruit often packed **very sweet** (~20° Brix when fully ripe). |
| **Tegan Blue** | User spelling “Teagan Blue” — published / nursery name is **Tegan Blue** | WA research stores at **0 ± 1 °C**, **~90 ± 5% RH** (1-MCP/MAP studies). |

Sources are postharvest papers and extension sheets (UC Davis / Crisosto for Black Amber; Curtin–WA / Singh group for Amber Jewel and Tegan Blue; UC plum fact sheet for generic Japanese-plum optima). They are **storage-temperature** guidance, not certified freeze-point tables for your packout.

**Shared room (recommended):** keep **one setpoint for all three**. Published air targets for these cultivars **do not diverge** enough to justify separate rooms — all want near **0 °C**, not a warmer intermediate band. Use:

- **Setpoint `0.5 °C`** — shared hold slightly above expected freeze, until you confirm freeze point for **your** Brix / maturity.
- **Low Temp Alarm Delta `1.5 °C`** → trip about **−1.0 °C** — **freeze guard** for a mixed load.

**What drives the shared numbers?** Literature does **not** publish distinct flesh freeze points for these three by name. Freeze risk tracks **soluble solids (Brix) and maturity** more than cultivar label: lower SSC → warmer (higher) freeze point → freeze first. Size the Low Δ for the **lowest-Brix / least-ripe pack** in the room (refractometer on each cultivar at packout). High-SSC Amber Jewel packs are usually *more* freeze-tolerant, not less. Black Amber’s well-documented risk is **chilling injury when held too warm** (~5 °C), not a uniquely high freeze point — that is why High Temp Alarm stays tight as well.

**Why not just use §4 (2 °C food)?** Plums want air **near 0 °C** (often about
**−0.5…+1 °C** by cultivar — **confirm freeze point for your packout / Brix** before going colder).
They need a tighter **freeze-guard** low alarm, and humid stone-fruit rooms frost the coil
harder. There is **no humidity setpoint** on this controller — SHT31 only reports RH and
feeds dew-point / frost-rate defrost; provide separate humidification for ~90–95% RH.

**Plant assumptions:** single compressor; **passive** defrost; evaporator fans **hardwired
continuous** (**Fan Relay Enabled** off). Probe 1 = coolroom air, Probe 2 = evaporator
(fixed). Bench: RS485 / expansion I2C often disconnected — enable what is fitted; offline
is expected until wired.

### 4b.1 Temperature control & compressor (§4.1)

| Setting | Recommended | Why |
|---|---|---|
| **Setpoint** | `0.5 °C` | Shared hold for Black Amber / Amber Jewel / Tegan Blue — near 0 °C with margin above freeze until packout freeze point is known. (§4 uses 2.0.) |
| **Compressor Differential** | `1.0 °C` (default) | Asymmetric: **ON at 1.5 °C**, **OFF at 0.5 °C**. |
| **Compressor Off-Delay (Lockout)** | `3 min` (default) | Min-off / motor protection — do not shorten without OEM approval. |
| **Compressor Min Run Time** | `2 min` (default) | Min-on pairs with Off-Delay against short-cycling. |
| **Fan Relay Enabled** | Leave `Off` | Continuous hardwired fans. |
| **Sensor Fallback Duty-Cycle** | `On`, `3` / `27` min (default) | ~10% duty if Probe 1 fails. |

### 4b.2 Defrost schedule + smart / drip (§4.2–§4.3)

| Setting | Recommended | Why |
|---|---|---|
| **Defrost System Enabled** | `On` | Near-0 °C + humidity → steady frost. |
| **Defrost Interval** | `360 min` (6 h; factory 480) | Humid produce / door traffic — same idea as §4. |
| **Defrost Max Duration** | `30 min` (default) | Safety cap if the coil is slow to clear. |
| **Defrost Early Termination by Temperature** | `On` @ **Defrost Termination Temp** `5.0 °C` | Ends when Probe 2 shows coil clear — less room warm-up. |
| **Defrost Drip-Drain Phase** | `On`, **Defrost Drip Time** `5 min` | Meltwater drain before cooling resumes. |
| **Smart Defrost (Delta-Triggered)** | **On** (factory Off) | Variable inbound fruit loads; needs Probe 2. |
| **Smart Defrost Delta / Dwell** | `8.0 °C` / `30 min` (defaults) | Tighten later only if ice appears between smart starts. |
| **Dew Point Early Defrost Trigger** | **On if Internal SHT31 fitted**, else `Off` | Best humidity-aware trigger for stone fruit. |
| **Frost Rate Monitoring** | **On if SHT31 fitted** (`5%` / `300 s`), else `Off` | Complements dew-point. |
| **Defrost Skip-If-Cold** | Leave `Off` (−10 °C threshold unused while off) | Commission on schedule first. |
| **Defrost Max Interval Override (Force-Max)** | `720 min` (12 h, default) | Safety net if you later enable Skip-If-Cold. |

### 4b.3 Alarms + post-defrost smart lockout (§4.4–§4.5)

| Setting | Recommended | Why |
|---|---|---|
| **High Temp Alarm Delta** | `2.0 °C` → ~**2.5 °C** | Tighter than §4 (2.5) / factory (3.0) — catch drift toward the CI “killing zone” (~2–8 °C) before ripening / breakdown risk rises (esp. Black Amber). |
| **Low Temp Alarm Delta** | `1.5 °C` → ~**−1.0 °C** | **Freeze guard** for the mixed room — tighter than §4 (2.0) / factory (3.0). Sized for the **lowest-Brix pack** among the three; confirm freeze point for your packout / Brix before tightening further. |
| **Alarm Persist Time** | `5 min` (default) | Ignores brief door / load spikes. |
| **Alarm Siren Enabled** | `On` | Local audible alarm. |
| **Alarm Recovery Hysteresis** | `0.5 °C` (default) | Stops clear/re-alarm flap. |
| **Ice Detection Enabled** | `On` | Needs Probe 2. |
| **Ice Alarm Delta / Dwell** | `15 °C` / `10 min` (defaults) | Precision polarity (large coolroom−evap gap). Do **not** use older “small gap” ice numbers. |
| **No-Cool Alarm Timeout** | `45 min` (factory 60) | Faster refrigeration-failure notice for high-value fruit. |
| **Startup Alarm Grace Floor** | `15 min` (default) | Suppresses nuisance alarms during boot pulldown. |
| **Post-Defrost Alarm Grace** | `20 min` (default) | Alarm quiet + **smart lockout**: blocks Smart / Dew-Point / Frost-Rate re-triggers right after a cycle (scheduled / Force-Max / Manual still allowed). Raise if early defrosts or alarms fire immediately after drip. |

### 4b.4 Door & light (§4.6)

| Setting | Recommended | Why |
|---|---|---|
| **Door Sensor Enabled** | `On` if reed fitted | Prerequisite for door alarm / light / hold. |
| **Door Sensor Mode (NC or NO)** | Match wiring (leave NC if that is how it is wired) | Wrong polarity inverts open/closed. |
| **Door-Triggered Light Enabled** | `On` | Staff load/unload convenience (needs Light Relay Enabled). |
| **Hold Compressor While Door Open** | Leave `Off` | Stuck-reed would starve cooling. |
| **Door Alarm Delay** | `300 s` (default) | Routine loading; shorten if doors must never stay open. |

### 4b.5 Probes, humidity monitor, CT (§4.7)

| Setting | Recommended | Why |
|---|---|---|
| **Temperature Display Unit** | `Celsius` (unless operators prefer °F) | Display only; control stays °C. |
| **Probe 1 / Probe 2 Calibration Offsets** | `0.0 °C` until checked | Manual offsets only — compare against a reference thermometer. **No “Calibrate Probes Now” action.** |
| **Evaporator Probe (Probe 2) Enabled** | `On` | Required for smart defrost, ice alarm, early terminate. |
| **Internal SHT31 Sensor Enabled** | `On` if fitted; else leave Off / expect offline | Reports RH toward a **monitor target ~90–95%** (no RH control loop / hysteresis entity exists). Enables Dew-Point / Frost-Rate when On and healthy. Bench: expansion I2C often absent. |
| **External SHT20 Sensor Enabled** | `On` if fitted | Ambient context on home gauge / diagnostics only. |
| **CT Clamp Enabled** | `Off` until QNDBK3 fitted @ addr **110** | Whole **plant feed** clamp — leave Off otherwise. |
| **CT Run-Proof Enabled** | `Off` until clamp online | Then On; set **Idle Max** / **Running Min** from measured idle vs run Amps (defaults 0.40 / 0.55 A are starting points only). |
| **CT Run-Proof Delay** | `30 s` (default) after enable | Persist before fail-to-start / stuck-on / overcurrent latch. |
| **CT Overcurrent Limit** | `1.50 A` start, or `0` to disable overcurrent only | Tune from measured plant peak after enable. |

### 4b.6 Notifications, audio, hardware, SD, access (§4.5b, §4.8–§4.9, §4.11, §4.13)

| Setting | Recommended | Why |
|---|---|---|
| **ntfy Notifications Enabled** | **On** for remote critical alarms | Low / High / No-Cool / Ice / Probe / Door / Hardware Offline / CT (if used). Set Server + Topic; **Send Test**. Priorities: leave defaults (No-Cool / Probe / Hardware Offline = urgent). |
| **Audio Alerts Enabled / Speaker Volume** | `On` / `85%` | Spoken alarms on site; leave per-phrase Speak toggles at factory defaults unless noisy. |
| **Compressor / Light / Siren Relay Enabled** | `On` | Hardware enables for cooling, door light, siren. |
| **SD Temp Log Retention** | `90 days` (factory 60) | Longer daily CSVs for harvest / seasonal log-tune (§7). Event log + 5 min temp samples (`log_interval_min`) are compile-time — leave as-is. |
| **Backup All Settings to SD** | After profile is dialled in | Off-device copy before flash / factory reset. |
| **Touchscreen PIN** | Change from `0000` | First-time checklist — not a cooling parameter. |
| **Home Assistant API Enabled** | Leave as needed (default Off) | Not part of the cooling profile. |

> **Disclaimer:** illustrative starting values from commercial / research stone-fruit chill
> practice for Black Amber, Amber Jewel, and Tegan Blue (~15° Brix starting assumption)
> and this controller’s documented behaviour — **not** a food-safety certification or a
> measured freeze-point prescription. Confirm freeze point for **your** packout / Brix,
> local rules, and how the loaded room actually performs. Cultivar spelling in catalogues:
> **Amber Jewel** (not “Jewels”), **Tegan Blue** (not “Teagan”).

### Other common produce — quick starting points

Rough industry-standard setpoint/RH hints only — same disclaimer. For dessert plums use
**§4b** (or `recommended_settings_plums.html`) rather than this table alone.

| Product | Typical setpoint | Typical RH target | Notes |
|---|---|---|---|
| **Dessert plums** (Black Amber, Amber Jewel, Tegan Blue; ~15° Brix start) | ~0.5 °C shared (see §4b) | 90–95% (monitor only) | One room / one SP; Low Δ freeze guard for lowest-Brix pack; confirm freeze point. |
| Apples (long-term) | 0 – 4 °C | 90–95% | Cold-tolerant; tighten Low Alarm Delta only if holding right at 0 °C. |
| Leafy greens / general veg | 0 – 4 °C | 95–98% | High RH need; same smart/dew-point idea as §4 / §4b when SHT is online. |
| Dairy / general chiller | 1 – 4 °C | Not usually critical | Start from the **§4** 2 °C profile. |

---

## 5. Two Diagrams You'll Actually Use Day-to-Day

**Compressor decision, every cycle:**

```text
┌──────────────────────────────────────────────────────────────────────┐
│           Probe reading missing/stale/out of range?                  │
├─────────────────────────────────┬────────────────────────────────────┤
│ YES → compressor OFF (safety)    │ NO → asymmetric band:              │
│                                   │  above setpoint + differential → ON │
│                                   │  below setpoint → OFF              │
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

## 7. After ~30 days — log-based setting tune (optional)

Once the SD card has about a month of `events.csv` (and daily temperature CSVs), you can
run a **recommend-only** helper on a PC to spot nuisance alarms, short-cycling, busy doors,
or aggressive defrost cadence, and compare suggestions to the 2 °C (§4) or plums (§4b)
profile you actually applied.

| OS | How |
|---|---|
| **macOS** | `./tools/analyse_logs_tune_settings.sh --host <controller-ip>` |
| **Windows** | `tools\analyse_logs_tune_settings.cmd --host <controller-ip>` |
| **Offline** | Download logs from the web **SD Card** tab (or the same script), then `--log-dir <folder>` |

The tool refuses to invent tweaks with **&lt; 30 days** of usable history **in the selected
window** (exit code 2). It does **not** change the controller unless you pass `--apply --yes`,
and then only an allowlisted set of number settings (never setpoint, never probe enables).

**WA fruit picking (late Dec → Apr):** external ambient can near ~40 °C — re-run the tune
monthly or per harvest block with `--since`/`--until` or `--window picking` (preset =
20 Dec → 30 Apr); keep setpoint at 2.0 °C and re-check differential / defrost / alarms.
Reports also summarise ambient (when logged) and compressor cycle rate. Scheduled
recommend-only HTML under `logs/tune_reports/`: `tools/run_seasonal_log_tune.sh` (macOS) /
`.cmd` / `.ps1` (Windows), or menu item **5** in `CoolroomLogTools`. Full protocol / venv /
launchd / Task Scheduler notes: `tools/README_LOG_TUNING.md`. Printable starting tables:
`reference/recommended_settings_2c.html` (general 2 °C) and
`reference/recommended_settings_plums.html` (Black Amber / Amber Jewel / Tegan Blue §4b).

---

## 8. When To Open the Full Manual

- You need to understand **why** a setting behaves the way it does, not just what value to
  type in.
- You're chasing a nuisance alarm or an unexpected defrost pattern.
- You want the full control-logic diagrams (compressor, defrost, all four alarm types,
  probe fault, SD auto-remount) rather than the two condensed ones above.
- You need the access-control/security model explained in full (touchscreen PIN vs. web
  login — they are separate).

→ `reference/USER_MANUAL.md`
