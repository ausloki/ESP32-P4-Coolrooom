# CPU / memory load breakout

Primary artefact for Cory in the IDE:
[`cpu-mem-load-breakout.canvas.tsx`](/Users/cory/.cursor/projects/Volumes-Scratch-Documents-ESP32-P4-Coolroom/canvases/cpu-mem-load-breakout.canvas.tsx).

## Live sample (2026-08-03 17:07:54 +08) — pre-split baseline

Device `192.168.37.237` via ESPHome REST:

| Metric | Value |
|--------|------:|
| CPU average | 39% |
| Core 0 busy | ~1% |
| Core 1 busy | 77% |
| Heap free (DEFAULT) | ~26.7 MB |
| PSRAM free | ~26.4 MB |
| Chip temperature | 38.3 °C |

## Implemented (2026-08-03): Option B then Option A

**B — C elevation**

- `p4_ctl_tick()` in `p4_ctl_tick.h` — full 10 s control orchestrator (was ~800 YAML lines).
- `p4_ui_second_tick()` in `p4_ui_second_tick.h` — full 1 s LVGL/UI block (was ~380 YAML lines of actions).
- YAML intervals are thin: `p4_ctl_tick_signal(...)` / `p4_ui_second_tick(...)`.
- Pure helpers remain in `p4_control.h` / `p4_ui.h`; orchestrators use ESPHome entity pointers by the same names `id()` rewrite produces (headers must only be included via `esphome: includes:` into main.cpp).

**A — Core split**

- FreeRTOS task `p4_ctl` pinned to **CPU 0**, woken by a binary semaphore from the 10 s YAML interval on loopTask (**CPU 1**).
- Control math, Modbus relay decisions, speak/ntfy script triggers, and SD temp/event appends run on C0.
- LVGL / `p4_ui_second_tick` / icon anim stay on C1. Never call LVGL from the C0 worker.
- Sharing rules documented in `p4_ctl_tick.h` (single control writer; UI reads display state without a mutex).

## Implemented (2026-08-03): Option C — small C1 trims

Already shipped earlier (kept):

- home2 arc work gated on `home2_view_active`
- Diagnostics CPU% / RTD age → ~30 s
- Countdown sensors → ~3 s (home lockout still 1 s via `p4_ui_comp_lockout_remaining_s`)
- `lvgl: full_refresh: true` (required for this MIPI path — not changed)

**C trims applied this pass:**

1. **Icon animation** — interval **50 ms → 100 ms** on Home; **skip entirely** when
   `!page_home_active` (icons not on other pages). Motion coefficients in `p4_ui.h`
   scaled ×2 so spin/flicker visual speed matches the old 50 ms cadence.
2. **1 s UI page-gating** in `p4_ui_second_tick()`:
   - Always: `p4_wifi::poll`, HA client drop, Status text_sensor publish,
     `system_time_valid` publish.
   - **NVS sync:** every 1 s except while `page_settings_active` (Settings
     steppers call debounced `persist_config_to_nvs`; flash sync on the tick
     was hitching touch under `full_refresh`).
   - **Home only** (`page_home_active`): clock/date/Wi‑Fi header, home lockout,
     status labels/colours, setpoint arcs, alarm banner, snow/flame FX, hidden LEDs.
   - **home2 only** (`page_home_active && home2_view_active`): `p4_ui_update_home2_cooling`.
   - **Info only** (`page_info_active`): Info diagnostics labels. (Always-on Info
     LVGL after the blank-page fix dirtied hidden widgets every 1 s and, with
     `full_refresh: true`, forced whole-frame redraws that starved Settings UX.)
   - `switch_to_page_info` also calls `p4_ui_second_tick` once for immediate paint.
   - `persist_config_to_nvs` is `mode: restart` + 300 ms delay so rapid +/- coalesce.

**Left undone (by design):**

- GT911 touch poll stays **50 ms** (UX risk if slowed).
- RTD Modbus **2 s**, control tick **10 s** untouched.
- `full_refresh` not flipped off (panel path requires it).
- ESP-Hosted task affinity / further LVGL flush work — larger than Option C.

## Why C1 was hot (baseline)

ESPHome pins `loopTask` to **core 1**. Before this change, LVGL, interval lambdas, Modbus orchestration, and the 10 s / 1 s YAML bodies all shared C1; C0 was nearly idle.

## Top load contributors (still relevant)

1. LVGL + MIPI-DSI with `full_refresh: true` (1024×600) — still C1
2. 1 s UI tick — now page-gated on C1 (`p4_ui_second_tick`)
3. 100 ms icon animation on Home only (was 50 ms always)
4. ESP-Hosted Wi-Fi (C6 SDIO) + web_server
5. 10 s control tick — on C0 worker

## Memory / flash (not the bottleneck)

Last compile after B+A: app image ~3.06 MiB; DIRAM ~165/576 KB used; PSRAM XIP holds code/rodata.

## Post-OTA samples (2026-08-03)

| Metric | Baseline | Post B+A | Post C (settled, Home) |
|--------|---------:|---------:|-----------------------:|
| CPU average | 39% | ~39% | ~39–41% |
| Core 0 | ~1% | ~1% | ~0.7% |
| Core 1 | 77% | ~77% | ~75–78% |

Post-C sample via REST (~2 min after OTA, idle-task window settled; still on Home).
Early post-boot readings (~4% / C1 ~7%) are **not** trustworthy until the FreeRTOS
run-time stats window fills — ignore them.

**Read carefully:** On Home, LVGL + `full_refresh` still dominate C1. Option C halves
icon wakeups (50→100 ms) and skips Home LVGL work when on Settings/Info; expect the
clearest C1 drop **off Home**, not on the default home gauge. B+A remain correct for
C0 isolation when Modbus/SD get heavier.

## Follow-ups

1. Re-sample C1 while parked on Settings / Info (Option C page gates).
2. Audit ESP-Hosted unpinned task affinity.
3. When RS485 is live, watch Modbus TX from C0 vs loopTask interaction.
4. Optional: batch diagnostic publishers; thin switch/number persist boilerplate.
