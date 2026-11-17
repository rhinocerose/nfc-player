#!/usr/bin/env bash
# Install the command-line toolchain and libraries into this checkout.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export ARDUINO_DIRECTORIES_DATA="$ROOT/.arduino15"
export ARDUINO_DIRECTORIES_USER="$ROOT/.arduino-user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$ROOT/.arduino15/staging"

CLI="$ROOT/.tools/arduino-cli"

if [ ! -x "$CLI" ]; then
    echo "Downloading Arduino CLI..."
    mkdir -p "$ROOT/.tools"
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
        | BINDIR="$ROOT/.tools" sh
fi

echo "Arduino CLI: $("$CLI" version)"

echo "Installing ESP32 core..."
"$CLI" core update-index
"$CLI" core install esp32:esp32@3.3.12

echo "Installing libraries..."
"$CLI" lib install "ArduinoJson@7.4.3"
"$CLI" lib install "Adafruit BusIO@1.17.4"
"$CLI" lib install "Adafruit PN532@1.3.4"

# ESP32-audioI2S is not in the Arduino Library Manager at the version we need.
if [ ! -d "$ARDUINO_DIRECTORIES_USER/libraries/ESP32-audioI2S" ]; then
    echo "Cloning ESP32-audioI2S 3.4.4..."
    git clone --depth 1 --branch 3.4.4 \
        https://github.com/schreibfaul1/ESP32-audioI2S.git \
        "$ARDUINO_DIRECTORIES_USER/libraries/ESP32-audioI2S"
fi

echo
echo "Installed cores:"
"$CLI" core list
echo
echo "Installed libraries:"
"$CLI" lib list

echo
echo "ESP32-S3 board options:"
"$CLI" board details -b esp32:esp32:esp32s3 || true

echo
echo "Setup complete. Run 'make build' or './scripts/build.sh'."
