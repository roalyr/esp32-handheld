#!/bin/bash
# Flash script for ESP32-S3 (esp32-s3-n16r8_diag)
# Usage: bash scripts/flash_s3.sh [upload_port]
# Example: bash scripts/flash_s3.sh /dev/ttyACM0

set -e

echo "=== Flashing ESP32-S3 (esp32-s3-n16r8_diag) ==="

# Locate PlatformIO CLI
if command -v pio >/dev/null 2>&1; then
    PIO_EXEC="pio"
elif [ -f "$HOME/.platformio/penv/bin/pio" ]; then
    PIO_EXEC="$HOME/.platformio/penv/bin/pio"
else
    echo "ERROR: PlatformIO CLI ('pio') not found in PATH or at ~/.platformio/penv/bin/pio."
    exit 1
fi

# Determine upload port if supplied
UPLOAD_PORT=""
if [ -n "$1" ]; then
    UPLOAD_PORT="$1"
fi

if [ -n "$UPLOAD_PORT" ]; then
    echo "Uploading to custom port: $UPLOAD_PORT"
    $PIO_EXEC run -t upload -e esp32-s3-n16r8_diag --upload-port "$UPLOAD_PORT"
else
    echo "Uploading using default configuration (auto-detect or platformio.ini)"
    $PIO_EXEC run -t upload -e esp32-s3-n16r8_diag
fi

echo "=== Flashing completed successfully! ==="
