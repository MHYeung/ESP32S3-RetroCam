# Browser flashing

## Requirements

- **Chrome** or **Edge** (Web Serial API)
- **HTTPS** — use the GitHub Pages URL, not a raw file open
- USB **data** cable to the **UART / programming** port

## GitHub Pages setup (repo owner)

1. Push a version tag: `git tag v1.0.0 && git push origin v1.0.0`
2. Wait for the **Release firmware and Pages** workflow to finish
3. In the repo: **Settings → Pages → Build and deployment**
   - Source: **Deploy from a branch**
   - Branch: **gh-pages** / **/ (root)**
4. Open `https://<username>.github.io/esp32s3_cam_retro/`

After each release, open **`manifest.json`** on Pages and confirm every `parts[].path` starts with **`firmware/`** (not `bootloader/...`). Wrong paths cause **404** in the web installer.

## Troubleshooting

| Problem | Try |
|---------|-----|
| No serial port | Correct USB port, driver, BOOT button |
| Install fails | Erase flash when prompted; retry |
| Wrong chip | Board must be **ESP32-S3** (16 MB flash, OPI PSRAM) |
| Page has no firmware | Tag not built yet — check Actions tab |
| 404 on `bootloader/bootloader.bin` | Bad manifest paths — paths must be `firmware/*.bin`; push workflow fix and re-tag |
| Boot loop: `TG0WDT_SYS_RST` at `ets_loader.c` | See **Boot loop after web install** below |

### Boot loop after web install

If the serial log shows `POWERON`, then immediately `TG0WDT_SYS_RST` with `ets_loader.c` and **no** `I (...) boot: ESP-IDF` line, the chip never finished loading the **second-stage bootloader** from flash. That is almost always a **flash image / flash settings** problem, not your application code.

1. **Erase all flash** — In the web installer, accept **erase** when prompted (`new_install_prompt_erase` in the manifest). Or run `idf.py erase-flash` / `esptool.py erase_flash`.
2. **Re-flash from the GitHub Release** (same binaries CI built):
   ```bash
   esptool.py --chip esp32s3 -p COMx erase_flash
   esptool.py --chip esp32s3 -p COMx write_flash @flasher_args.json
   ```
   Download `flasher_args.json` plus the three `.bin` files from the release assets.
3. **Compare with a local build** — `idf.py build flash monitor` on your machine. If local flash boots but web/CI does not, re-tag after updating `sdkconfig.defaults` and wait for Actions to republish `gh-pages`.
4. **Check the module** — Firmware targets **ESP32-S3-WROOM-1 N16R8** (16 MB quad flash + octal PSRAM). An **8 MB** module or a non-S3 board will misbehave with this partition table.
5. **USB port** — Use the **UART / programming** port for install and serial monitor, not the OTG port used for USB MSC.

## Flash from source (idf.py)

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

See the main [README](../README.md) for usage (button, AP gallery, MSC).
