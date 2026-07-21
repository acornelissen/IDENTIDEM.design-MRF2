#!/usr/bin/env bash
# Regenerate the pixel-exact user-manual screenshots from the real firmware
# draw code. Run from anywhere; paths resolve relative to Firmware/.
#
#   Firmware/scripts/generate-screenshots.sh
#
# Builds the native_screenshots env and runs the generator, which overwrites
# the SVGs in Documentation/user-manual/images/.
set -euo pipefail

FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$FIRMWARE_DIR"

pio run -e native_screenshots
./.pio/build/native_screenshots/program

echo "Screenshots regenerated in Documentation/user-manual/images/"
