# Firmware layout (gstack-style)

This project splits **board pins**, **camera bring-up**, and **HTTP streaming** into separate `components/` so you can add features without growing a single `main.c`.

## Suggested growth path for FreeRTOS

| Layer | Role |
|--------|------|
| `main/` | `app_main`: chip-wide init order (NVS → camera → Wi‑Fi → services), then delegate to modules |
| `components/camera_bsp/` | Pin mux + `camera_config_t` defaults for your PCB revision |
| `components/web_stream/` | HTTP routes (swap MJPEG for WebSocket / API later) |
| `components/<feature>/` | One folder per peripheral or product feature (SD card, BLE, motor, etc.) |
| `docs/HARDWARE_PINOUT.md` | GPIO truth table + links to upstream pin headers |

## Task model (when you outgrow “everything in callbacks”)

- Run **Wi‑Fi / HTTP** and **camera capture** on different priorities; protect shared state with a **queue** or **ring buffer** of `camera_fb_t *` ownership (never return a framebuffer while another task still reads it).
- Read **ESP-IDF** → *FreeRTOS* → stack watermark APIs during bring-up so streaming does not silently overflow the HTTP worker stack.

## Build

```text
idf.py set-target esp32s3
idf.py menuconfig   # App Wi-Fi → set APP_WIFI_SSID / APP_WIFI_PASSWORD
idf.py build flash monitor
```

### If CMake says `unknown name` for `esp32-camera`

1. Run `idf.py fullclean` then `idf.py build` so the Component Manager re-fetches `managed_components/` (needs network once).
2. Confirm the Component Manager is enabled (do **not** set `IDF_COMPONENT_MANAGER=0`).
3. After a successful fetch you should see `managed_components/espressif__esp32-camera/`; `REQUIRES esp32-camera` in CMake matches the **registry component name** (see [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)).
4. Offline fallback: clone the driver into `components/esp32-camera` (folder name must be `esp32-camera`) and remove the `espressif/esp32-camera` lines from the `idf_component.yml` files so you do not register the same component twice.

Browser: `http://<device-ip>/` (MJPEG at `/stream`).
