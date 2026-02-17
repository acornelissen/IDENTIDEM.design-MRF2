# Flashing the MRF2 Firmware (VS Code + PlatformIO)

This guide walks through building and flashing the firmware using VS Code with the PlatformIO IDE extension. It assumes you have the repo checked out and want to flash the `Firmware/` project.

If you prefer browser-based flashing, use the GitHub Pages updater:
`https://acornelissen.github.io/IDENTIDEM.design-MRF2/`

## Prerequisites
- VS Code installed
- PlatformIO IDE extension installed
- USB-C data cable (charge-only cables will not work)
- MRF2 powered on

## 1) Install VS Code extensions
1. Open VS Code.
2. Open the Extensions view (`Cmd/Ctrl+Shift+X`).
3. Install **PlatformIO IDE**.
4. (Optional) Install **Espressif IDF**. PlatformIO does not require it, but it can be useful for ESP tooling and serial utilities.

## 2) Open the project in PlatformIO
1. Click the PlatformIO icon in the Activity Bar to open **PIO Home**.
2. Choose **Open Project**.
3. Select the `Firmware/` folder (the folder that contains `platformio.ini`).
4. Wait for PlatformIO to finish installing toolchains and dependencies.

## 3) Build the firmware
1. In the PlatformIO toolbar (bottom bar), click **Build** (checkmark icon).
2. Confirm the build completes without errors.

## 4) Connect and select the serial port
1. Switch on the MRF2 and connect it via USB-C.
2. In the bottom bar, click the **Serial Port** selector and choose **Auto** or the correct port.
   - macOS examples: `/dev/tty.usbmodem*`
   - Windows examples: `COM3`, `COM4`
   - Linux examples: `/dev/ttyUSB0` or `/dev/ttyACM0`

## 5) Upload (flash)
1. Click **Upload** (right-arrow icon) in the PlatformIO toolbar.
2. Wait for the upload to finish.

If upload fails:
- Put the ESP32-S3 into bootloader mode: **hold BOOT**, tap **RESET**, then release BOOT and retry upload.
- Try a different USB-C cable or USB port.

## 6) Verify on device
1. Disconnect USB.
2. Power-cycle the MRF2.
3. The external display should show the firmware boot screen and version.

## 7) Serial monitor (optional)
1. Click **Monitor** (plug icon) in the PlatformIO toolbar.
2. Use **115200 baud** (matches `SERIAL_BAUD_RATE` in `Firmware/include/mrfconstants.h`).

For command-line build/upload/monitor, see `Firmware/README.md`.

## Troubleshooting
- **No serial port listed**: try a different cable/port or check USB permissions (Linux).
- **Build errors on first run**: let PlatformIO finish downloading toolchains, then retry.
- **Upload starts but fails to connect**: use BOOT/RESET sequence and retry.
