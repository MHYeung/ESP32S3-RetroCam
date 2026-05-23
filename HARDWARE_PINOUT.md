# ESP32-S3-CAM (WROOM-1 N16R8 + OV2640) -- pin reference

Third-party ESP32-S3 camera boards are **not** fully standardized. Treat this table as a **bring-up default**, then verify against your exact PCB (continuity or vendor schematic).

## Online sources (priority)

| Source | What to use it for |
|--------|---------------------|
| [Espressif `esp32-camera` -- `camera_pinout.h` (BOARD_ESP32S3_WROOM)](https://github.com/espressif/esp32-camera/blob/master/examples/camera_example/main/camera_pinout.h) | **OV2640 DVP <-> GPIO** mapping used by this firmware's default |
| [Arduino-ESP32 `CameraWebServer` / `camera_pins.h` -- `CAMERA_MODEL_ESP32S3_EYE`](https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h) | Same electrical mapping as the S3-EYE style bus (common on WROOM camera boards) |
| [Espressif ESP32-S3-WROOM-1 datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.html) | Module pin <-> **GPIO number** (not the same as FPC pad order) |
| [Prusa ESP32-S3-CAM community notes](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam/blob/master/doc/ESP32-S3-CAM/README.md) | Warns there is often **no official** vendor schematic; PCB reverse-engineering / clones vary |

## OV2640 (parallel DVP) -- default for this repo

Aligned with **`BOARD_ESP32S3_WROOM`** in `esp32-camera` (see `components/camera_bsp/include/camera_bsp_pins.h`).

| Signal | GPIO | Notes |
|--------|------|--------|
| XCLK | 15 | LEDC-generated sensor clock |
| SIOD (SDA) | 4 | SCCB / I2C to sensor |
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
| PWDN | -1 | Prusa/diymore style: SD CMD uses **GPIO38** -- keep PWDN **-1** so the sensor is not driven from CMD. |
| RESET | -- | Not routed on many clones (`-1` = driver uses software reset path) |

## Breakout headers (silkscreen GPIO numbers)

Your board exposes **logical GPIO** labels on the headers (not the WROOM module pad index). Any GPIO **not** consumed by the camera FPC / strapping / USB may be used for peripherals.

**Strapping / boot (ESP32-S3 TRM + WROOM datasheet):** GPIO0 is **BOOT**; avoid driving conflicting levels at reset. **UART0** is often wired to the USB bridge: **TX = GPIO43**, **RX = GPIO44** on many S3 modules (confirm on your exact board if serial is dead).

## When the camera probe fails (`0x105` or init errors)

- Wrong **PWDN** / **RESET** / **SCCB** pins for your clone.
- **PSRAM** not enabled or wrong **OPI** mode in `sdkconfig` (N16R8 usually needs octal PSRAM).
- See Espressif discussion: [esp32-camera issue on S3 N16R8 / routing](https://github.com/espressif/esp32-camera/issues/802).

## Documentation to read (active learning)

1. **ESP32-S3 Technical Reference Manual** -- *IO MUX / GPIO Matrix* (why any pin is not arbitrary at reset).
2. **ESP-IDF Programming Guide** -- *ESP32-S3* -> **External RAM (SPIRAM)** (why JPEG + stream needs PSRAM).
3. **ESP-IDF** -- **esp_http_server** (thread safety: which APIs are safe from which context).

## SD card (Prusa / diymore `module_ESP32-S3-CAM.h`)

| Signal | GPIO | Notes |
|--------|------|--------|
| CLK | 39 | SDMMC 1-bit |
| CMD | 38 | Must not be used as camera PWDN when SD is populated |
| D0 | 40 | |

FAT is mounted at **`/sdcard`** for JPEG photos (`/sdcard/DCIM`). **USB MSC** uses the same pins without `esp_vfs_fat` mounted.

## GC9A01 SPI (defaults in `menuconfig`)

| Signal | GPIO |
|--------|------|
| SCLK | 41 |
| MOSI | 42 |
| CS | 45 |
| DC | 46 |
| RST | 3 |
| BL | menuconfig (`APP_LCD_PIN_BL`, default off) |

SPI host defaults to **SPI2** (`CONFIG_APP_LCD_SPI_HOST=2`).

## Tact button (GPIO1)

Default **GPIO1** (active low, internal pull-up).

| Gesture | Action |
|---------|--------|
| Triple-tap within `APP_BOOT_MSC_WINDOW_MS` after reset (start tapping right after reset) | USB MSC cold-boot |
| Triple-tap anytime while running (3 quick taps within `APP_TRIPLE_TAP_WINDOW_MS`) | USB MSC (unmounts SD, use native USB OTG port) |
| Short tap | Capture JPEG to `/sdcard/DCIM/IMG#####.jpg`; LED white pulse |
| Long press >= `APP_LONG_PRESS_MS` (default 2 s) | Toggle SoftAP + HTTP gallery on/off |

## Status LED (WS2812 -- GPIO2)

| LED colour | Meaning |
|------------|---------|
| Dim green (solid) | Offline, camera healthy |
| Red slow blink | Offline, camera fault (probe failed) |
| Brief white pulse | Shutter saved a JPEG |
| Blue (solid) | SoftAP on, no phone connected |
| Cyan (solid) | SoftAP on, phone connected -- browse `http://192.168.4.1/` |

## USB mass storage

Use the **native USB OTG** data port on the ESP32-S3 (not the UART USB bridge). See [ESP-IDF USB Device](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html).

## Internal LittleFS

Partition label **`storage`**, mounted at **`/littlefs`** for small app metadata (not the SD card).
