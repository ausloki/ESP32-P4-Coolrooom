# Program Control Logic Flowchart
# ESP32-P4 Coolroom Controller

```mermaid
flowchart TD
    BOOT([Boot / Power On]) --> INIT[p4_log_boot\np4_ntp_set_fast_sync\nstatus = Booting...]
    INIT --> RS485[RS485 Modbus\nControllers come online\nrelay_board / rtd_board_1 / rtd_board_2]
    RS485 --> NTP{NTP Sync\nComplete?}
    NTP -->|No - retry 60s| NTP
    NTP -->|Yes| RTC[PCF8563 RTC synced\nhw_rtc_ok = true\nSNTP → 24h interval]
    RTC --> LOOP

    LOOP([Main 10s Control Loop])

    LOOP --> P1[Read probe1_temp\nfrom rtd1_ch1_raw]
    P1 --> FAULT{probe stale\nor invalid?}

    FAULT -->|Yes| PFERR[ctl_probe_fault = true\ncompressor OFF\ndefrost OFF\nsiren ON]
    PFERR --> WAIT10[Wait 10s]
    WAIT10 --> LOOP

    FAULT -->|No| COMP[p4_ctl_compressor_eval\nt vs setpoint ± diff/2]

    COMP --> C1{result?}
    C1 -->|+1 turn ON| CON[relay_compressor ON\nrelay_defrost must be OFF]
    C1 -->|-1 turn OFF| COFF[relay_compressor OFF]
    C1 -->|0 hold| CHOLD[no change]
    CON --> ALM
    COFF --> ALM
    CHOLD --> ALM

    ALM[Alarm Evaluation\nhi = t > setpoint + alarm_high_delta\nlo = t < setpoint - alarm_low_delta]
    ALM --> A1{hi OR lo?}
    A1 -->|Yes| SIREON[ctl_alarm_high/low = true\nrelay_siren ON]
    A1 -->|No| SIREOFF[clear alarm flags\nrelay_siren OFF]
    SIREON --> DEFCHK
    SIREOFF --> DEFCHK

    DEFCHK{relay_defrost\ncurrently ON?}
    DEFCHK -->|No - check if due| DEFDUE{p4_ctl_defrost_due\nelapsed >= interval_h?}
    DEFDUE -->|No| WAIT10
    DEFDUE -->|Yes| DEFSTART[relay_compressor OFF\nrelay_defrost ON\ndefrost_on_since_ms = millis]
    DEFSTART --> WAIT10

    DEFCHK -->|Yes - check timeout| DEFTMO{p4_ctl_defrost_timeout\nelapsed >= max_min?}
    DEFTMO -->|No - still running| WAIT10
    DEFTMO -->|Yes| DEFEND[relay_defrost OFF\ndefrost_last_end_ms = millis]
    DEFEND --> WAIT10
```

---

## Boot Sequence Detail

```mermaid
sequenceDiagram
    participant Boot
    participant NTP
    participant RTC as PCF8563 RTC
    participant RS485
    participant HA as Home Assistant

    Boot->>Boot: p4_log_boot() - CPU/DRAM/PSRAM diagnostics
    Boot->>NTP: p4_ntp_set_fast_sync(60000ms)
    Boot->>RS485: Modbus controllers init (priority -10)
    RS485-->>Boot: relay_board online → hw_rs485_relay_ok = true
    RS485-->>Boot: rtd_board_1 online → hw_rs485_rtd1_ok = true
    RS485-->>Boot: rtd_board_2 online → hw_rs485_rtd2_ok = true
    NTP-->>Boot: on_time_sync → p4_ntp_set_interval(86400000ms)
    RTC-->>Boot: on_time_sync → hw_rtc_ok = true
    Boot->>HA: API connection established
    Boot->>Boot: 10s control loop starts
```

---

## Probe / Sensor Architecture

```mermaid
graph LR
    RTD1["RTD Board 1\nModbus addr 100\n9600 8N1"] -->|CH1 reg 1| R1C1["rtd1_ch1_raw\n(internal)"]
    RTD1 -->|CH2 reg 2| R1C2["rtd1_ch2_raw\n(internal)"]
    RTD2["RTD Board 2\nModbus addr 101\n9600 8N1"] -->|CH1 reg 1| R2C1["rtd2_ch1_raw\n(internal)"]

    R1C1 -->|p4_rtd_or_nan| P1["probe1_temp\nCoolroom Primary\n★ Control Probe"]
    R1C2 -->|p4_rtd_or_nan| P2["probe2_temp\nEvaporator"]
    R2C1 -->|p4_rtd_or_nan| P3["probe3_temp\nAmbient"]

    P1 --> CTL["10s Control Loop\np4_ctl_compressor_eval\np4_ctl_alarm_high/low\np4_ctl_defrost_due"]
    CTL --> REL1["relay_compressor\nCoil 1"]
    CTL --> REL0["relay_defrost\nCoil 0"]
    CTL --> REL3["relay_siren\nCoil 3"]
```

---

## Phase Summary

| Phase | Description                                | Status      |
|-------|--------------------------------------------|-------------|
| 1     | WiFi, HA API, web server, OTA              | ✅ Complete |
| 2     | RS485 Modbus: relays + 3x RTD probes, RTC  | ✅ Complete |
| 3     | Coolroom control logic                     | ✅ Complete |
| 4     | LVGL 7" MIPI-DSI touchscreen UI            | ✅ Complete |
| 5     | SD card logging, ntfy, backup/restore      | ✅ Complete |
| 6     | Extended control: lockout, grace, smart defrost, fallback, ice, no-cool | 🔄 Current |
| 7     | Diagnostic sensors + RS485 health + select entities | ⏳ Next |
| 8     | LVGL multi-page UI (home meter + settings + system pages) | ⏳ Planned |
