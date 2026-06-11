#!/bin/bash
# Build script for ESP32-S2-Mini (lolin_s2_mini)
# Usage: bash scripts/build_s2.sh

set -e

echo "=== Building ESP32-S2 (lolin_s2_mini) ==="

# Locate PlatformIO CLI
if command -v pio >/dev/null 2>&1; then
    PIO_EXEC="pio"
elif [ -f "$HOME/.platformio/penv/bin/pio" ]; then
    PIO_EXEC="$HOME/.platformio/penv/bin/pio"
else
    echo "ERROR: PlatformIO CLI ('pio') not found in PATH or at ~/.platformio/penv/bin/pio."
    exit 1
fi

$PIO_EXEC run -e lolin_s2_mini
