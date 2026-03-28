#!/bin/bash
# Flash script for ESP32-S2-Mini via DFU
# Usage: bash scripts/flash.sh
# Requires: dfu-util, esptool v5.x
# Procedure: plug board in with BOOT held, then run this script

set -e
BUILD=".pio/build/lolin_s2_mini"
ESPTOOL="${HOME}/.platformio/penv/bin/esptool"
VID_PID="303a:0002"

echo "=== ESP32-S2 DFU Flash ==="

# Check device is present
if ! lsusb | grep -q "$VID_PID"; then
    echo "ERROR: No Espressif device ($VID_PID) found."
    echo "Hold BOOT, plug in the board, then run this script."
    exit 1
fi

# Step 1: Send DFU detach (Runtime -> DFU download mode)
echo "Step 1: Sending DFU detach..."
dfu-util -d "$VID_PID" -e 2>&1 || true
sleep 2

# Step 2: Merge binaries
echo "Step 2: Merging binaries..."
"$ESPTOOL" --chip esp32s2 merge-bin \
    -o "$BUILD/merged.bin" \
    --flash-mode dio --flash-size 4MB \
    0x1000  "$BUILD/bootloader.bin" \
    0x8000  "$BUILD/partitions.bin" \
    0x10000 "$BUILD/firmware.bin"

# Step 3: Flash via DFU
echo "Step 3: Flashing via DFU..."
dfu-util -d "$VID_PID" -D "$BUILD/merged.bin" -a 0

echo ""
echo "=== Done! Unplug and re-plug the board to boot. ==="
