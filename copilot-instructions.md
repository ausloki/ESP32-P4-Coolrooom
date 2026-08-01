- 🧠 Read `/memory-bank/memory-bank-instructions.md` first.
- 🗂 Load all `/memory-bank/*.md` files before each task.
- 📂 Also load files from the active feature folder (e.g. `/memory-bank/authentication/`).
- 🚦 Follow the Kiro-Lite workflow: PRD → Design → Tasks → Code.
- 🔒 Follow rules in `copilot-rules.md`.
- 📝 On "/update memory bank", refresh activeContext.md & progress.md.

**⭐ CRITICAL SESSION PROTOCOL:**
- **At session START:** Always read files in `memory-bank/` to understand past context, previous patterns, and current state
  - `memory-bank/memory-bank-instructions.md` — Structure and usage rules
  - `memory-bank/activeContext.md` — What was just finished, what's pending, immediate next steps
  - `memory-bank/progress.md` — Phase completion tracking, build metrics, known limitations
  - Feature-specific folders (e.g., `memory-bank/phase-4/`) — Implementation patterns and lessons learned
- **Before session CLOSE:** Update `memory-bank/` files with completion status, new patterns discovered, and explicit next steps for following session

---

# ESP32-P4 Coolroom Controller — Hardware-First Design Rules

## Hardware Platform

**Board**: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (ESP32-P4NRW32)  
**Non-negotiable**: All pin assignments and hardware decisions must be verified against this exact board first.

### Before Every Code Decision
1. **Check `reference/` folder first** — schematic PDF, datasheets, hardware_pins.md, control diagrams
2. **Always consult local ESP32-P4 silicon references before coding**:
  - `reference/esp32-p4_technical_reference_manual_en.pdf`
  - `reference/esp32-p4_datasheet_en.pdf`
  Use these manuals to confirm peripheral behavior, timer/clock expectations, and power-domain details so implementation matches ESP32-P4 hardware, not generic ESP32 assumptions.
2. **Then check Waveshare GitHub**: https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B
3. Never assume generic ESP32 pin mappings — always verify from schematic or official examples
4. For Phase 4 (LVGL UI) changes, consult `reference/HANDOVER_2026-07-18.md` for display architecture

### Confirmed GPIO Assignments (Don't Reuse!)

| Signal | GPIO | Source |
|--------|------|--------|
| I2C SDA (RTC, GT911) | 7 | Waveshare wiki |
| I2C SCL | 8 | Waveshare wiki |
| RS485 UART TX | 27 | 13_RS485_Test example |
| RS485 UART RX | 26 | 13_RS485_Test example |
| WiFi SDIO D0-D3 | 14-17 | esp32_p4_function_ev_board.h |
| WiFi SDIO CLK | 18 | esp32_p4_function_ev_board.h |
| WiFi SDIO CMD | 19 | esp32_p4_function_ev_board.h |
| ESP32-C6 WKUP | 6 | esp32_p4_function_ev_board.h |
| ESP32-C6 RESET | 54 | esp32_p4_function_ev_board.h |
| TF Card SDMMC | 39-44 | Waveshare wiki |
| I2S Audio | 9-13, 53 | Waveshare wiki |
| GT911 Touch INT | 23 | Schematic |
| GT911 Touch RST | 33 | Schematic |
| Display Backlight | 32 | Schematic |
| Door Reed Sensor | 20 | GPIO header (Phase 5) |

**Reserved** (never reuse): 6, 7-8, 9-13, 14-19, 23, 26-27, 32-33, 39-44, 53-54

## Code Architecture

**Principle**: C++/ESP-IDF logic layer, YAML coordination layer, never logic in YAML.

- `p4_helpers.h` — boot diagnostics, NTP, RTD validation, temp formatting, heap utils
- `p4_control.h` — compressor hysteresis, alarms, defrost scheduling, probe fault handling
- `esp32-p4-coolroom.yaml` — thin YAML; lambdas call C++ helpers, business logic lives in C++

## Phase 4 LVGL Display Architecture

### Layout Overview
- **Left sidebar**: 4 status icons (❄️ 🔥 💡 🔔) positioned vertically
- **Center**: Three concentric horseshoe arcs (270° sweep, 225° start angle)
- **Right sidebar**: Secondary readings (ambient, evaporator, heap)
- **Canvas**: 1024×600 pixels (7" MIPI-DSI), GT911 touch controller

### Icon Visual Feedback: Two Patterns

**Pattern 1: Relay-Bound Icons** (compressor, light)
- Updates in `relay.on_turn_on/off` handlers (synchronous with relay state)
- Can enable/disable via relay settings or soft triggers
- Implementation:
```yaml
relay_example:
  on_turn_on:
    - lvgl.label.update:
        id: ui_icon_name
        text_color: col_active_color
  on_turn_off:
    - lvgl.label.update:
        id: ui_icon_name
        text_color: col_grey
```

**Pattern 2: Control Logic-Bound Icons** (defrost, alarm)
- Updates in binary sensor `on_state` handlers (event-driven from control flags)
- Enable passive operation modes and soft triggers (defrost without relay, alarm without siren)
- Implementation:
```yaml
binary_sensor:
  - platform: template
    id: control_flag_active
    lambda: 'return id(ctl_control_flag) > 0;'  # or boolean check
    on_state:
      - lvgl.label.update:
          id: ui_icon_name
          text_color: !lambda 'return x ? id(col_active_color) : id(col_grey);'
```

### Temperature-to-Arc Conversion Formula
```cpp
int percent = (int)((temp_celsius + 20.0f) / 35.0f * 100.0f);
// Maps: -20°C → 0%, 0°C → 57%, 15°C → 100%
```

### Color Palette (Use Hex Codes, Not Names)
```yaml
col_bg:      #1C1C1E  (dark background)
col_panel:   #2C2C2E  (card surface)
col_blue:    #0A84FF  (coolroom arc, snowflake)
col_cyan:    #00BCD4  (setpoint arc)
col_pink:    #FF1493  (ambient arc)
col_green:   #30D158  (compressor active)
col_orange:  #FF9F0A  (defrost/light active, warnings)
col_red:     #FF453A  (alarm active)
col_grey:    #888888  (inactive status)
col_text:    #FFFFFF  (primary text)
col_subtext: #8E8E93  (secondary text)
```

## Build & Constraints

```bash
cd /Volumes/Scratch/Documents/ESP32-P4-Coolroom
source .venv/bin/activate
esphome compile esp32-p4-coolroom.yaml
```

**Stack**: ESPHome 2026.7.0, ESP-IDF 5.5.4, RISC-V toolchain 14.2.0_20260121  
**Target**: RAM < 25%; app image should fit comfortably inside one 7 MB OTA slot.

**Flash Policy**
- Board flash is 32 MB, but the real firmware constraint is the dual-OTA layout in `partitions_custom.csv`.
- Each OTA app slot is `0x700000` bytes (7,340,032 bytes, about 7.0 MB).
- Soft target: keep the app image under 6.0 MB.
- Review threshold: investigate growth once the image is above about 5.5 MB.
- Hard limit: the image must remain below one OTA slot size.

**Current Metrics** (as of 2026-07-18):
- RAM: 19.4% (111,594 / 576,464 bytes) ✓ Healthy
- Flash: 20.2% (1,479,622 / 7,340,032 bytes, about 1.48 MB of a 7 MB OTA slot) ✓ Comfortable

## Documentation Rules

When firmware changes, **always update in same commit**:
- `reference/hardware_pins.md` — if GPIO assignments change
- `reference/control_logic_ns_diagram.md` — if control loop or LVGL feedback changes
- `reference/HANDOVER_2026-07-18.md` — if display architecture changes
- `reference/session_recaps.md` — dated recap at every phase boundary
- `HANDOVER_NOTES_YYYY-MM-DD.md` and/or `reference/HANDOVER_YYYY-MM-DD.md` — handover notes at every change

## Phase Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | WiFi, HA API, web server, OTA | ✅ |
| 2 | RS485 Modbus: relays + 2x RTD; wall clock = SoC LP RTC + NTP | ✅ |
| 3 | Coolroom control logic (hysteresis, alarms, defrost) | ✅ |
| 4 | LVGL 7" MIPI-DSI touchscreen dashboard | ✅ |
| 5 | SD card logging, ntfy notifications, backup/restore | ⏳ |

## Key Reference Files

| File | Purpose | When to Use |
|------|---------|-----------|
| `reference/ESP32-P4-WIFI6-Touch-LCD-7B.pdf` | Board schematic | GPIO conflicts, pin function lookup |
| `reference/hardware_pins.md` | Confirmed GPIO assignments | Before reusing any GPIO |
| `reference/control_logic_ns_diagram.md` | Control loop + LVGL diagram | Understanding defrost/alarm flow, icon feedback |
| `reference/HANDOVER_2026-07-18.md` | Display architecture, icon patterns | Phase 4 UI changes |
| `reference/DISPLAY_ARCHITECTURE_VISUAL.md` | Visual layout reference | Widget positioning, z-order |
| `reference/session_recaps.md` | Phase boundary recaps | Project history, what changed when |
| `reference/program_control_logic_flowchart.md` | Boot & sensor flow | Initialization sequence
## Post-Task Execution & Documentation Protocol

On the completion of ANY task, sub-task, or feature, you MUST execute the following four steps before declaring the task finished:

## Mandatory After Every Change

This workflow is mandatory after every code or documentation change, no exceptions.

0. **Compile + NVS-safe flash** (firmware/dashboard changes):
  - Embed dashboard if HTML changed: `python3 scripts/embed_dashboard.py`
  - Coverage check: `.venv/bin/python tools/check_dashboard_coverage.py`
  - Compile: `./tools/esphome_compile.sh esp32-p4-coolroom.yaml`
  - Flash with `./tools/esphome_flash.sh --device /dev/cu.usbmodemXXXX` (or OTA to IP).
    **Do not** use plain `esphome upload` over USB — it erases NVS settings.
  - Optional live reboot-survival smoke: `.venv/bin/python tools/test_settings_persistence.py --host <ip>`
1. Update code-review graph:
  - `/Volumes/Scratch/Documents/ESP32-P4-Coolroom/.venv/bin/python -m code_review_graph update --repo .`
  - `/Volumes/Scratch/Documents/ESP32-P4-Coolroom/.venv/bin/python -m code_review_graph status`
2. Update all impacted docs in the same pass.
3. Append a dated recap entry to `reference/session_recaps.md`.
4. Update handover notes (`HANDOVER_NOTES_YYYY-MM-DD.md` and/or `reference/HANDOVER_YYYY-MM-DD.md`).
5. Create a compact commit with code + docs together:
  - `/usr/bin/git add -A`
  - `/usr/bin/git commit -m "<clear scoped message>"`
6. Verify clean working tree:
  - `/usr/bin/git status --short`

If any step fails, task closeout is blocked until fixed.

### 1. Compilation, Flash & Code Review
* Compile firmware and verify zero errors/warnings: `./tools/esphome_compile.sh esp32-p4-coolroom.yaml`
* Flash with `./tools/esphome_flash.sh` (NVS-preserving) or OTA — never plain USB `esphome upload`
* Run `.venv/bin/python tools/check_dashboard_coverage.py` when entities/globals change
* Review RAM/Flash metrics — ensure RAM remains under 25% and the app image remains comfortably below the 7 MB OTA-slot ceiling (soft target < 6.0 MB)
* Generate code-review graph: Create a mermaid.js architecture graph detailing the updated logic, state changes, or data flow
  - Run: `tools/code_review_graph_cli.sh` (or equivalent tool)
  - Document: Explain what changed and why in graph title/annotations
  - Purpose: Visualize control flow, LVGL widget hierarchy, relay/sensor interactions
  - Include in: Session recap or handover notes as reference diagram

### 2. Documentation Updates
* Update all relevant reference files:
  - `reference/hardware_pins.md` — if GPIO assignments changed
  - `reference/control_logic_ns_diagram.md` — if control loop or LVGL feedback changed
  - `reference/HANDOVER_2026-07-18.md` — if display architecture changed
  - `reference/session_recaps.md` — dated recap at phase boundaries
* Ensure code comments and API schemas reflect latest changes
* Update this file (`copilot-instructions.md`) if new patterns emerge

### 3. Session Recaps & Handover Files
* Create or update `reference/session_recaps.md` with new dated entry (one per phase boundary)
* Create or update handover notes for each change (`HANDOVER_NOTES_YYYY-MM-DD.md` and/or `reference/HANDOVER_YYYY-MM-DD.md`)
* Document in recap:
  - **What was changed** — Features, bug fixes, architecture updates
  - **Why it was changed** — Requirements, design rationale, constraints addressed
  - **Current system state** — Build metrics (RAM, Flash), compilation status, phase progress
  - **Explicit next steps** — What should happen next session, pending device tests, known limitations

### 4. Git Commit & Push
* Stage all modified files: `git add -A`
* Construct commit message using Conventional Commits format:
  - `feat(lvgl): ...` for feature additions
  - `fix(control): ...` for bug fixes
  - `docs(ref): ...` for documentation-only changes
  - `refactor(yaml): ...` for code restructuring
  - `test(diag): ...` for testing/diagnostics
* Include emoji prefixes where helpful (✅ ✨ 🐛 📝 🔄)
* Example: `feat(lvgl): Add icon visual feedback for defrost control logic`
* Execute: `git commit -m "Your message"` and verify success

### Completion Checklist

Before marking a task complete, verify ALL of the following:
- [ ] Code compiles with zero errors/warnings
- [ ] Device flashed with NVS-preserving method (`tools/esphome_flash.sh` or OTA)
- [ ] `tools/check_dashboard_coverage.py` clean when entities/globals changed
- [ ] RAM usage reported (should be ≤ 25%)
- [ ] Flash usage reported (should be ≤ 20%)
- [ ] All relevant reference files updated
- [ ] Handover notes updated for this change
- [ ] `reference/session_recaps.md` entry added (if phase boundary)
- [ ] Git commit created with clear message
- [ ] Build artifact paths documented (e.g., `firmware.factory.bin` location)
- [ ] Next steps explicitly documented for following session