# NFC Audio Player — Bill of Materials

**Target:** ESP32-S3 DevKitC-1 N16R8 (16 MB flash, 8 MB octal PSRAM)
**Updated:** 2026-11-06
**Currency:** approximate USD, excluding shipping and tools

> PSRAM is mandatory. The firmware uses `ESP32-audioI2S`, which needs a multi-core
> ESP32-S3 and PSRAM. A classic ESP32 (WROOM/DevKit) will not run this build.

## Core build

| # | Item | Specification | Qty | Unit | Total | Notes |
|---:|---|---|---:|---:|---:|---|
| 1 | ESP32-S3 DevKitC-1 N16R8 | 16 MB flash, 8 MB octal PSRAM, USB-C | 1 | $15 | $15 | N16R8/N16R8V; avoid the N8 no-PSRAM variant |
| 2 | microSD card | 32 GB, high endurance, FAT32 | 1 | $10 | $10 | 16 GB is enough; 64 GB optional |
| 3 | microSD SPI adapter | 3.3 V, CS/SCK/MOSI/MISO | 1 | $3 | $3 | Some cards need a 100 µF cap |
| 4 | PN532 NFC breakout | 13.56 MHz, I2C, 3.3 V | 1 | $15 | $15 | Adafruit 364 or equivalent |
| 5 | NTAG215 stickers | 25 mm, 504 B | 20 | $0.60 | $12 | One per song; NTAG213 also works |
| 6 | MAX98357A breakout | I2S mono class-D, 3.2 W | 1 | $6 | $6 | Adafruit 3006 or equivalent |
| 7 | Speaker | 3 W, 4 Ω, 40–50 mm | 1 | $5 | $5 | Enclosure-mounted |
| 8 | Tactile buttons | 6 mm through-hole | 2 | $0.50 | $1 | Stop + reserved |
| 9 | Status LED | 3 mm diffused, red or green | 1 | $1 | $1 | With 330 Ω resistor |
| 10 | Passives | 470 µF, 100 µF, 100 nF, 330 Ω | 1 set | $3 | $3 | Amp/SD decoupling, LED, buttons |
| 11 | Headers and wires | 2.54 mm male/female, Dupont | 1 set | $5 | $5 | Solder headers to the dev board |
| 12 | USB-C data cable | 0.5 m, data + power | 1 | $6 | $6 | Needed for flashing/monitor |
| 13 | Power bank | 10,000 mAh, 5 V/2 A, USB-C | 1 | $25 | $25 | Prefer always-on/low-current mode |
| 14 | Enclosure | 3D-printed PLA/PETG | 1 | $8 | $8 | 2 mm plastic over the NFC antenna |
| **Core total** | | | | | **~$115** | |

## Optional / recommended

| Item | Specification | Qty | Unit | Total | Why |
|---|---|---:|---:|---:|---|
| Inline USB-C switch | 5 V/3 A | 1 | $4 | $4 | Fully disconnect the power bank |
| Heatsink | 14 × 14 mm aluminium | 1 | $2 | $2 | SoC cooling during playback |
| Spare microSD | 32 GB high endurance | 1 | $10 | $10 | Backup card |
| Second USB-C cable | 0.5 m, 5 V/2 A | 1 | $6 | $6 | Dedicated power cable |
| **Optional total** | | | | **~$22** | |

## Tools not included

- Soldering iron, solder, flux, and heat-shrink.
- Multimeter for continuity/short checks.
- Wire strippers and small side cutters.
- 3D printer or enclosure service.
- MicroSD reader for formatting/backups.
- Computer with a USB-C port for flashing.

## Power budget

| Mode | Estimated 5 V current | Notes |
|---|---:|---|
| Idle, Wi-Fi connected, reader polling | 0.10–0.25 A | ESP32-S3 with Wi-Fi |
| Playback, moderate volume | 0.30–0.70 A | Amp average + ESP32 |
| Peak | ~1.0 A | 3 W amp burst |
| Deep sleep | a few mA | Power bank may still supply |

A 10,000 mAh bank should run the player for many hours to days depending on volume and
Wi-Fi; measure the actual build. There is no fuel gauge.

## Sourcing notes

- **ESP32-S3 board:** search for `ESP32-S3-DevKitC-1 N16R8` or `N16R8V`. Confirm the listing
  says 16 MB flash and 8 MB PSRAM. The plain `esp32-s3-devkitc-1` may be the N8/no-PSRAM
  variant in some stores.
- **PN532:** Adafruit 364 is the reference board. Many clones work if set to I2C mode and
  powered from 3.3 V.
- **MAX98357A:** Adafruit 3006 or a generic breakout. It is mono; the Pi/ESP32 I2S bus is
  stereo, but the amp mixes/selects a channel.
- **microSD adapter:** use a 3.3 V adapter. If it has a 5 V regulator and level shifter,
  power it from 5 V and verify the logic levels.
- **Power bank:** choose one that does not shut off at low current, and that can supply at
  least 2 A peaks.
- **Tags:** NTAG215 stickers are common and cheap. NTAG213 is sufficient for UIDs.

## Compatibility

| Part | Requirement |
|---|---|
| MCU | ESP32-S3 with PSRAM; classic ESP32 is not supported |
| Audio library | `ESP32-audioI2S` 3.4.4 |
| NFC library | Adafruit PN532 1.3.4 + BusIO |
| JSON | ArduinoJson 7.4.3 |
| Core | ESP32 Arduino core 3.3.12 |
| Flash | 16 MB recommended for the 3 MB app + 16 MB layout |
| SD | FAT32 |

## Firmware size reference

The verified build uses **1,974,239 bytes (62%)** of the 3 MB app partition and
**64,060 bytes (19%)** of internal RAM. Audio buffers use PSRAM.
