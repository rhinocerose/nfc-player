#!/usr/bin/env bash
# Open the serial monitor.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export ARDUINO_DIRECTORIES_DATA="$ROOT/.arduino15"
export ARDUINO_DIRECTORIES_USER="$ROOT/.arduino-user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$ROOT/.arduino15/staging"

CLI="$ROOT/.tools/arduino-cli"
PORT="${1:-${PORT:-/dev/ttyACM0}}"

exec "$CLI" monitor -p "$PORT" -c baudrate=115200
