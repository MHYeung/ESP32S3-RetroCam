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

## Troubleshooting

| Problem | Try |
|---------|-----|
| No serial port | Correct USB port, driver, BOOT button |
| Install fails | Erase flash when prompted; retry |
| Wrong chip | Board must be **ESP32-S3** (16 MB flash, OPI PSRAM) |
| Page has no firmware | Tag not built yet — check Actions tab |

## Flash from source (idf.py)

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

See the main [README](../README.md) for usage (button, AP gallery, MSC).
