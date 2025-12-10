// [Revision: v1.0] [Path: scripts/upload_spiffs.sh] [Date: 2025-12-10]
// Description: Helper script to upload `data/` to device SPIFFS using PlatformIO.

#!/bin/bash
set -e

# Ensure data directory exists and contains files
if [ ! -d "data" ]; then
  echo "Creating data/ directory"
  mkdir -p data
fi

# Upload filesystem image to device (replace environment name if needed)
pio run --target uploadfs -e esp32-s3-devkitc-1
