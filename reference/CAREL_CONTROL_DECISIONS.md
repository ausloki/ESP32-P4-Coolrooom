# Carel / control decisions (2026-08-01)

| Item | Decision |
|------|----------|
| **A** Symmetric vs asymmetric hysteresis | **Not required** — keep symmetric (`St ± rd/2`). |
| **B** Compressor min ON-time | **Already implemented** (`Compressor Min Run Time`, default 2 min). |
| **D** Door open → hold compressor | **Implemented** as opt-in **Hold Compressor While Door Open** (default **off**), LVGL Door page + web Door group. |
| **C** Fan control | **Implemented** by repurposing Modbus **coil 0** (was Defrost Relay) as **Fan Relay** (default enable **off**). Fan follows compressor; forced off during defrost + drip. Defrost is always passive. |
