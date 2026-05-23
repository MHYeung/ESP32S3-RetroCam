# ESP32-S3 CAM Retro

Offline-first retro camera firmware for **ESP32-S3** camera boards (OV2640/OV5640 class), with SD storage, WS2812 status LED, SoftAP photo gallery, and USB MSC.

## Features

- **Short tap** (GPIO1) — save JPEG to `/sdcard/DCIM/`
- **Long press** — toggle SoftAP + web gallery (`http://192.168.4.1/`)
- **Triple tap** (boot or runtime) — USB mass storage on the **OTG** port
- **WS2812** status LED (default GPIO48)
- Gallery: session grouping (~4 h gaps), preview, single JPG or multi-file ZIP download

## Hardware

See [docs/HARDWARE_PINOUT.md](docs/HARDWARE_PINOUT.md). Typical setup:

| Item | Default |
|------|---------|
| Shutter / AP / MSC button | GPIO1 |
| WS2812 | GPIO48 |
| SD (1-bit) | CLK 39, CMD 38, D0 40 |
| Flash / monitor USB | UART port |
| SD card on PC | **OTG** USB port |

## Flash firmware (browser)

Firmware is built and published when you push a **version tag** (e.g. `v1.0.0`).

### One-time repo setup (maintainer)

1. Push a tag: `git tag v1.0.0 && git push origin v1.0.0`
2. Wait for the [Release firmware and Pages](.github/workflows/release-firmware.yml) workflow
3. **Settings → Pages** → deploy from branch **`gh-pages`**, folder **`/` (root)**
4. Replace `YOUR_GITHUB_USER` in [docs/index.html](docs/index.html) with your GitHub username

### Users

Open: **`https://<your-username>.github.io/esp32s3_cam_retro/`**

Use **Chrome** or **Edge**, USB **programming** port, then **Install firmware**. See [docs/FLASHING.md](docs/FLASHING.md).

## Build from source

Requires [ESP-IDF v5.5.x](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/index.html) and network once for the Component Manager.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Options: `idf.py menuconfig` → **Camera capture** (resolution, mirror/flip), **Wi-Fi SoftAP**, **Tact button**, **Status LED**.

Defaults are in [sdkconfig.defaults](sdkconfig.defaults). Do not commit personal `sdkconfig` (it is gitignored).

## Usage

1. Boot — LED shows camera health; no Wi-Fi until you enable it
2. Short tap — photo saved as `IMG#####.jpg` on SD
3. Long press (~2 s) — join AP **ESP32S3-CAM** (password `12345678`), open **http://192.168.4.1/**
4. Triple tap at boot (within ~4.5 s) or three quick taps while running — MSC on **OTG** USB

## Project layout

| Path | Role |
|------|------|
| `main/` | `app_main`, Wi-Fi AP, boot flow |
| `components/camera_bsp/` | Sensor pins + init |
| `components/app_input/` | Button, shutter, MSC gesture |
| `components/web_stream/` | HTTP gallery + ZIP |
| `components/usb_msc_mode/` | TinyUSB MSC |
| `docs/` | GitHub Pages installer (bins from CI on tag) |

## License

MIT — see [LICENSE](LICENSE).
