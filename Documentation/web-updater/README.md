# MRF2 Browser Firmware Updater (GitHub Pages)

This repo includes a static firmware updater site in `docs/` that uses Web Serial + ESP Web Tools to flash MRF2 firmware in-browser.

## Public URL

`https://acornelissen.github.io/IDENTIDEM.design-MRF2/`

## How it works

1. The GitHub Actions workflow `.github/workflows/firmware-updater-pages.yml` builds firmware from `Firmware/` using PlatformIO.
2. The workflow copies required ESP32-S3 images into `firmware/latest/` inside the Pages artifact:
   - `bootloader.bin` @ `0x0000` (Adafruit TinyUF2 bootloader)
   - `partitions.bin` @ `0x8000`
   - `boot_app0.bin` @ `0xE000`
   - `tinyuf2.bin` @ `0x2D0000`
   - `firmware.bin` @ `0x10000`
3. The workflow generates `firmware/latest/manifest.json` for ESP Web Tools.
4. The static app in `docs/` points the install button to `./firmware/latest/manifest.json`.

## Browser requirements

- Desktop Chrome or Edge (Web Serial support)
- HTTPS context (GitHub Pages provides this)
- USB data cable

## Operational notes

- Workflow trigger is `push` to `main` (and `workflow_dispatch`).
- If the site shows `Not published yet`, run the workflow or merge to `main`.
- If `boot_app0.bin` path changes in PlatformIO packages, update the workflow copy path.
