#!/usr/bin/env bash
# Compile the sketch for the ESP32-S3 DevKitC-1 N16R8.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export ARDUINO_DIRECTORIES_DATA="$ROOT/.arduino15"
export ARDUINO_DIRECTORIES_USER="$ROOT/.arduino-user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$ROOT/.arduino15/staging"

CLI="$ROOT/.tools/arduino-cli"
if [ ! -x "$CLI" ]; then
    echo "arduino-cli not found; run ./scripts/setup.sh first." >&2
    exit 1
fi

FQBN="${FQBN:-esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc}"

echo "Building $FQBN"
"$CLI" compile \
    --fqbn "$FQBN" \
    --build-path "$ROOT/.build" \
    --warnings default \
    "$ROOT/NFCPlayer"
