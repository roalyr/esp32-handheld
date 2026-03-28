#!/bin/bash
# SPIFFS upload is disabled — filesystem removed from firmware.
# Will be replaced with SD card support when hardware is wired.
echo "ERROR: SPIFFS is disabled. Filesystem will use SD card (not yet wired)."
exit 1

set -e

BUILD=".pio/build/lolin_s2_mini"
ESPTOOL="${HOME}/.platformio/penv/bin/esptool"
VID_PID="303a:0002"

# Ensure data directory exists
if [ ! -d "data" ]; then
  echo "Creating data/ directory"
  mkdir -p data
fi

# Build the SPIFFS image
echo "=== ESP32-S2 SPIFFS Upload (DFU) ==="
echo "Step 1: Building SPIFFS image..."
pio run --target buildfs -e lolin_s2_mini

SPIFFS_BIN="$BUILD/spiffs.bin"
if [ ! -f "$SPIFFS_BIN" ]; then
  echo "ERROR: SPIFFS image not found at $SPIFFS_BIN"
  exit 1
fi

# Check device is present
if ! lsusb | grep -q "$VID_PID"; then
    echo "ERROR: No Espressif device ($VID_PID) found."
    echo "Hold BOOT, plug in the board, then run this script."
    exit 1
fi

# Get SPIFFS partition offset from partition table
# Default for a 4MB flash with default partitions: 0x290000
SPIFFS_OFFSET="0x290000"

# DFU detach (Runtime -> Download mode)
echo "Step 2: Sending DFU detach..."
dfu-util -d "$VID_PID" -e 2>&1 || true
sleep 2

# Merge SPIFFS image at correct offset into a flashable binary
echo "Step 3: Merging SPIFFS image..."
"$ESPTOOL" --chip esp32s2 merge-bin \
    -o "$BUILD/spiffs_merged.bin" \
    --flash-mode dio --flash-size 4MB \
    "$SPIFFS_OFFSET" "$SPIFFS_BIN"

# Flash via DFU
echo "Step 4: Flashing SPIFFS via DFU..."
dfu-util -d "$VID_PID" -D "$BUILD/spiffs_merged.bin" -a 0

echo ""
echo "=== Done! Unplug and re-plug the board to boot. ==="
