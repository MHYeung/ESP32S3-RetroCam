# ESP32-S3-CAM (WROOM-1 N16R8 + OV2640) — pin reference

Third-party ESP32-S3 camera boards are **not** fully standardized. Treat this table as a **bring-up default**, then verify against your exact PCB (continuity or vendor schematic).

## Online sources (priority)

| Source | What to use it for |
|--------|---------------------|
| [Espressif `esp32-camera` — `camera_pinout.h` (BOARD_ESP32S3_WROOM)](https://github.com/espressif/esp32-camera/blob/master/examples/camera_example/main/camera_pinout.h) | **OV2640 DVP ↔ GPIO** mapping used by this firmware’s default |
| [Arduino-ESP32 `CameraWebServer` / `camera_pins.h` — `CAMERA_MODEL_ESP32S3_EYE`](https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h) | Same electrical mapping as the S3-EYE style bus (common on WROOM camera boards) |
| [Espressif ESP32-S3-WROOM-1 datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.html) | Module pin ↔ **GPIO number** (not the same as FPC pad order) |
| [Prusa ESP32-S3-CAM community notes](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam/blob/master/doc/ESP32-S3-CAM/README.md) | Warns there is often **no official** vendor schematic; PCB reverse-engineering / clones vary |

## OV2640 (parallel DVP) — default for this repo

Aligned with **`BOARD_ESP32S3_WROOM`** in `esp32-camera` (see `components/camera_bsp/include/camera_bsp_pins.h`).

| Signal | GPIO | Notes |
|--------|------|--------|
| XCLK | 15 | LEDC-generated sensor clock |
| SIOD (SDA) | 4 | SCCB / I²C to sensor |
| SIOC (SCL) | 5 | SCCB |
| D7 (Y9) | 16 | Data MSB of byte lane (sensor-dependent naming) |
| D6 (Y8) | 17 | |
| D5 (Y7) | 18 | |
| D4 (Y6) | 12 | |
| D3 (Y5) | 10 | |
| D2 (Y4) | 8 | |
| D1 (Y3) | 9 | |
| D0 (Y2) | 11 | |
| VSYNC | 6 | |
| HREF | 7 | |
| PCLK | 13 | |
| PWDN | 38 | If your module floats or ties PWDN differently, set to `-1` in `camera_bsp_pins.h` and retest |
| RESET | — | Not routed on many clones (`-1` = driver uses software reset path) |

## Breakout headers (silkscreen GPIO numbers)

Your board exposes **logical GPIO** labels on the headers (not the WROOM module pad index). Any GPIO **not** consumed by the camera FPC / strapping / USB may be used for peripherals.

**Strapping / boot (ESP32-S3 TRM + WROOM datasheet):** GPIO0 is **BOOT**; avoid driving conflicting levels at reset. **UART0** is often wired to the USB bridge: **TX = GPIO43**, **RX = GPIO44** on many S3 modules (confirm on your exact board if serial is dead).

## When the camera probe fails (`0x105` or init errors)

- Wrong **PWDN** / **RESET** / **SCCB** pins for your clone.  
- **PSRAM** not enabled or wrong **OPI** mode in `sdkconfig` (N16R8 usually needs octal PSRAM).  
- See Espressif discussion: [esp32-camera issue on S3 N16R8 / routing](https://github.com/espressif/esp32-camera/issues/802).

## Documentation to read (active learning)

1. **ESP32-S3 Technical Reference Manual** — *IO MUX / GPIO Matrix* (why any pin is not arbitrary at reset).  
2. **ESP-IDF Programming Guide** — *ESP32-S3* → **External RAM (SPIRAM)** (why JPEG + stream needs PSRAM).  
3. **ESP-IDF** — **esp_http_server** (thread safety: which APIs are safe from which context).
