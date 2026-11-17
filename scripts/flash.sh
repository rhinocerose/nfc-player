#!/usr/bin/env bash
# Upload the last build to the board.
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
if [ ! -d "$ROOT/.build" ]; then
    echo "No build found; run ./scripts/build.sh first." >&2
    exit 1
fi

PORT="${1:-${PORT:-/dev/ttyACM0}}"
FQBN="${FQBN:-esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc}"

echo "Uploading to $PORT ($FQBN)"
"$CLI" upload \
    -p "$PORT" \
    --fqbn "$FQBN" \
    --input-dir "$ROOT/.build" \
    "$ROOT/NFCPlayer"
