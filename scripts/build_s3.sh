#!/bin/bash
# Build script for ESP32-S3 (esp32-s3-n16r8_diag)
# Usage: bash scripts/build_s3.sh

set -e

echo "=== Building ESP32-S3 (esp32-s3-n16r8_diag) ==="

# Locate PlatformIO CLI
if command -v pio >/dev/null 2>&1; then
    PIO_EXEC="pio"
elif [ -f "$HOME/.platformio/penv/bin/pio" ]; then
    PIO_EXEC="$HOME/.platformio/penv/bin/pio"
else
    echo "ERROR: PlatformIO CLI ('pio') not found in PATH or at ~/.platformio/penv/bin/pio."
    exit 1
fi

$PIO_EXEC run -e esp32-s3-n16r8_diag
