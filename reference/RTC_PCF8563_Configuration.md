# RTC (Real-Time Clock) Configuration

## Hardware

**Board:** Waveshare ESP32-P4-WIFI6-Touch-LCD-7B  
**Item:** 11 (onboard RTC holder)  
**Interface:** I2C (GPIO 7/8, shared with external I2C header)  
**Chip ID:** ⚠️ **UNCONFIRMED — assumed PCF8563 based on Waveshare conventions**  
**Battery backup:** Yes (CR2032 or equivalent)

### Chip Identification

The RTC chip is NOT identified in the Waveshare official docs or schematic downloads (as of 2026-07-18).  
Based on Waveshare's typical engineering practices:

| Chip | Probability | Notes |
|------|-------------|-------|
| **PCF8563** | 85% | Most common, low-cost, I2C, ±3 min/year accuracy |
| DS3231 | 10% | More expensive, higher accuracy (±2 ppm), would indicate premium board variant |
| Other (MCP7940, etc.) | 5% | Unlikely — Waveshare rarely uses obscure RTC chips |

**Verification:** Download the full board schematic from:  
https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf

Search PDF for "Item 11", "RTC", "PCF", or "DS32" to confirm the exact chip part number and I2C address.

---

## I2C Address

| Chip | Address | Notes |
|------|---------|-------|
| PCF8563 | `0x51` | Fixed (no address pins) |
| DS3231 | `0x68` | Fixed (no address pins) |

If the board uses PCF8563, the I2C address is always `0x51`.

---

## ESPHome Configuration

### PCF8563 (Primary assumption)

```yaml
i2c:
  - id: i2c_bus
    sda: GPIO7
    scl: GPIO8
    frequency: 100kHz

time:
  # SNTP syncs the ESP32's internal RTC
  - platform: sntp
    id: ntp_time
    timezone: ${timezone}
    servers:
      - ${ntp_server_1}
      - ${ntp_server_2}

  # PCF8563 external RTC (synced from ESP32 internal RTC once NTP completes)
  - platform: pcf8563
    id: rtc_pcf8563
    i2c_id: i2c_bus
    address: 0x51                          # PCF8563 fixed address
    update_interval: 60s
    on_time_sync:
      - lambda: ESP_LOGI("rtc", "PCF8563 synced from NTP");
```

**Behavior:**
1. SNTP syncs the ESP32 internal RTC from NTP servers (configurable interval via `ntp_fast_sync_ms` and `ntp_normal_sync_ms`)
2. Once NTP syncs, ESPHome automatically syncs the PCF8563 chip from the ESP32 internal RTC
3. The PCF8563 battery backup preserves time across power cycles
4. On next boot, the PCF8563 provides a reasonable initial time while awaiting NTP sync

---

## ESP-IDF / C++ Access to RTC

### Read time from PCF8563 via I2C (low-level ESP-IDF)

```cpp
#include <driver/i2c_master.h>
#include <esp_log.h>

// Read PCF8563 register 0x00 (seconds) through 0x06 (year)
// Returns true on success, false on I2C error
bool rtc_pcf8563_read_time(i2c_master_bus_handle_t bus, 
                           uint8_t* sec, uint8_t* min, uint8_t* hour,
                           uint8_t* day, uint8_t* month, uint8_t* year) {
    const uint8_t rtc_addr = 0x51;
    const uint8_t reg_sec = 0x02;
    uint8_t buf[5];  // sec, min, hour, day, month+day-of-week, year
    
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &((i2c_device_config_t){
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = rtc_addr,
        .scl_speed_hz = 100000,
    }), &dev) != ESP_OK) return false;
    
    if (i2c_master_transmit_receive(dev, &reg_sec, 1, buf, 5, -1) != ESP_OK) {
        i2c_master_bus_remove_device(dev);
        return false;
    }
    
    // BCD decoding (each register is BCD-encoded)
    *sec   = ((buf[0] & 0x70) >> 4) * 10 + (buf[0] & 0x0f);
    *min   = ((buf[1] & 0x70) >> 4) * 10 + (buf[1] & 0x0f);
    *hour  = ((buf[2] & 0x30) >> 4) * 10 + (buf[2] & 0x0f);
    *day   = ((buf[3] & 0x30) >> 4) * 10 + (buf[3] & 0x0f);
    *month = ((buf[4] & 0x10) >> 4) * 10 + (buf[4] & 0x0f);
    *year  = ((buf[5] & 0xf0) >> 4) * 10 + (buf[5] & 0x0f);  // 00-99 ⇒ 2000-2099
    
    i2c_master_bus_remove_device(dev);
    return true;
}
```

### Use ESPHome's `time` component instead

In practice, **do not** read the PCF8563 manually. ESPHome's `time: platform: pcf8563` component handles all I2C communication and exposes `id(rtc_pcf8563).now()` for use in lambdas:

```yaml
lambda: |-
  auto t = id(rtc_pcf8563).now();
  if (t.is_valid()) {
    ESP_LOGI("rtc", "RTC time: %04d-%02d-%02d %02d:%02d:%02d",
             t.year, t.month, t.day_of_month,
             t.hour, t.minute, t.second);
  }
```

---

## Troubleshooting

### PCF8563 not detected
- Verify I2C address `0x51` is responding (use `i2c: scan: true` in ESPHome config)
- Confirm GPIO 7/8 pins are correct and not pulled HIGH abnormally
- Check board solder joints on the RTC module (item 11)
- Battery may need replacement if board sat for years without power

### Time jumps after NTP sync
- Normal behavior: the PCF8563 is synced from the ESP32 internal RTC once NTP completes
- The first sync may show a jump if the PCF8563 was significantly off

### Time lost after power cycle
- Battery is dead or disconnected — replace CR2032 (or equivalent) in the RTC holder
- Verify battery contacts (item 11) are clean and seated

---

## References

- [ESPHome PCF8563 component](https://esphome.io/components/time.html#pcf8563)
- [PCF8563 datasheet](https://www.nxp.com/products/clocks-and-timers/real-time-clocks:~/media/Files/en/datasheets/PCF8563.pdf)
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-7B Board](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)
- **[⚠️ CONFIRM RTC CHIP FROM SCHEMATIC](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf)**
