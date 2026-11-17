#!/usr/bin/env bash
# List serial ports and connected boards.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export ARDUINO_DIRECTORIES_DATA="$ROOT/.arduino15"
export ARDUINO_DIRECTORIES_USER="$ROOT/.arduino-user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$ROOT/.arduino15/staging"

exec "$ROOT/.tools/arduino-cli" board list
