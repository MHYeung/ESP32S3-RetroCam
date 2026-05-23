# Firmware layout (gstack-style)

This project splits **board pins**, **camera bring-up**, and **HTTP streaming** into separate `components/` so you can add features without growing a single `main.c`.

## Suggested growth path for FreeRTOS

| Layer | Role |
|--------|------|
| `main/` | `app_main`: NVS → LittleFS → boot MSC gate → camera → SD FAT → wifi_ap (radio off) → status_led → LCD preview → shutter task (long-press toggles AP + gallery) |
| `components/camera_bsp/` | Pin mux + `camera_config_t` defaults for your PCB revision |
| `components/web_stream/` | DCIM photo gallery: `GET /` (listing) + `GET /photo?name=` (serve JPEG). Started/stopped with SoftAP. |
| `components/status_led/` | WS2812 on GPIO2 -- camera health, AP/client state, shutter pulse |
| `components/storage_sd/` | SDMMC 1-bit + FAT mount at `/sdcard` |
| `components/store_littlefs/` | Internal flash LittleFS at `/littlefs` |
| `components/ui_lcd_gc9a01/` | GC9A01 + JPEG preview task |
| `components/app_input/` | Boot triple-tap MSC detect + shutter task |
| `components/usb_msc_mode/` | TinyUSB MSC-only cold boot path |
| `components/<feature>/` | One folder per peripheral or product feature (SD card, BLE, motor, etc.) |
| `docs/HARDWARE_PINOUT.md` | GPIO truth table + links to upstream pin headers |

## Task model (when you outgrow “everything in callbacks”)

- Run **Wi‑Fi / HTTP** and **camera capture** on different priorities; protect shared state with a **queue** or **ring buffer** of `camera_fb_t *` ownership (never return a framebuffer while another task still reads it).
- Read **ESP-IDF** → *FreeRTOS* → stack watermark APIs during bring-up so streaming does not silently overflow the HTTP worker stack.

## Build

```text
idf.py set-target esp32s3
idf.py menuconfig   # SoftAP SSID/password, SD/LCD/button GPIOs, LED GPIO
idf.py build flash monitor
```

### If CMake says `unknown name` for `esp32-camera`

1. Run `idf.py fullclean` then `idf.py build` so the Component Manager re-fetches `managed_components/` (needs network once).
2. Confirm the Component Manager is enabled (do **not** set `IDF_COMPONENT_MANAGER=0`).
3. After a successful fetch you should see `managed_components/espressif__esp32-camera/`; `REQUIRES esp32-camera` in CMake matches the **registry component name** (see [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)).
4. Offline fallback: clone the driver into `components/esp32-camera` (folder name must be `esp32-camera`) and remove the `espressif/esp32-camera` lines from the `idf_component.yml` files so you do not register the same component twice.

Default offline. Long-press GPIO1 (~2 s) starts SoftAP. Connect phone to **ESP32S3-CAM** (password `12345678`), then open `http://192.168.4.1/` to browse saved photos. Long-press again to stop the AP.
