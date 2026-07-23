# ESPHome Native-IDF REQUIRES Omission Bug Report Draft

## Summary

On ESPHome `2026.7.0`, a native ESP-IDF rebuild can fail after reconfigure because the generated `src/CMakeLists.txt` does not provide all required built-in ESP-IDF component dependencies to the `src` component.

In this project, the missing dependencies observed were:

- `esp_ringbuf`
- `esp_http_server`

The failure occurs on an ESP32-P4 project using the ESP-IDF framework, `api:`, `web_server:`, `captive_portal:`, and `esp32_hosted:`.

## Environment

- ESPHome: `2026.7.0`
- Framework: native ESP-IDF
- ESP-IDF: `5.5.4`
- Target board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
- MCU: ESP32-P4
- Host OS: macOS (Apple Silicon)

## Project Context

This repro was observed on a hardware-specific ESP32-P4 firmware project with:

- `esp32:` `variant: esp32p4`
- `framework:` `type: esp-idf`
- `esp32_hosted:` for ESP32-C6 SDIO Wi-Fi
- `web_server:` enabled
- `api:` enabled
- `captive_portal:` enabled
- LVGL display/touch stack enabled

The issue appears to be in ESPHome's native-IDF generated build metadata, not in any ESP32-P4 GPIO or peripheral configuration.

## Reproduction Pattern

1. Run a normal compile on the project.
2. Force a native-IDF reconfigure path, for example by removing the generated CMake cache.
3. Re-run compile.

Example flow used locally:

```bash
rm -f .esphome/build/esp32-p4-coolroom/build/CMakeCache.txt
esphome compile esp32-p4-coolroom.yaml
```

## Observed Failure

Compilation fails during the native-IDF build because generated sources include headers from built-in IDF components that are not listed in the generated `src/CMakeLists.txt` `REQUIRES` list.

Observed errors included:

```text
fatal error: freertos/ringbuf.h: No such file or directory
```

and:

```text
fatal error: esp_http_server.h: No such file or directory
```

ESPHome/IDF then reports that the missing components should be added to `idf_component_register(... REQUIRES ...)` in generated `src/CMakeLists.txt`.

## Generated File State

The generated file looked like this:

```cmake
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "." "esphome"
    REQUIRES ${ESPHOME_PROJECT_BUILTIN_COMPONENTS}
)
```

On this project, successful rebuild required:

```cmake
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "." "esphome"
    REQUIRES ${ESPHOME_PROJECT_BUILTIN_COMPONENTS} esp_http_server esp_ringbuf
)
```

## Expected Behavior

ESPHome should generate a `src/CMakeLists.txt` whose `REQUIRES` list is sufficient for clean native-IDF rebuilds, including reconfigure-triggered rebuilds.

At minimum, if generated `src` sources include headers requiring built-in ESP-IDF components such as `esp_ringbuf` or `esp_http_server`, those components should be present in the generated `REQUIRES` list automatically.

## Workaround Used Locally

This project now uses a repo-owned compile-helper workaround:

- run `esphome compile`
- detect the known native-IDF missing-`REQUIRES` failure
- patch generated `.esphome/build/<config>/src/CMakeLists.txt`
- retry `ninja all` and `ninja size`

That workaround restores reliable local compiles, but it should not be necessary if ESPHome generated the correct `REQUIRES` metadata up front.

## Notes

- This was observed on an ESP32-P4 project, but the root cause appears to be generic to ESPHome's native-IDF generated `src` component metadata rather than specific to ESP32-P4 hardware.
- The issue was triggered reliably only on the reconfigure/clean-build path; an already-warm build tree could compile successfully without exposing it.