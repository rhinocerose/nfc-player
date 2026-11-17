# NFC Audio Player — ESP32-S3

A small NFC-triggered audio player built on an ESP32-S3. Tap a card, hear its song. Audio
lives on a local SD card and is uploaded from a phone or computer through the player's own
web interface. There is **no YouTube downloader, no Pi, and no external server**.

## Features

- ESP32-S3 runs the player, NFC reader, web server and SD storage.
- Browser upload of MP3, M4A/AAC, FLAC, OGG/Opus and WAV files directly to the SD card.
- One card maps to one song; tap to play, tap again to stop, tap another card to switch.
- Learn mode in the web UI: start learning, tap a card, assign a file.
- Home Wi-Fi with mDNS (`nfcplayer.local`) and a fallback access point
  (`NFCPlayer-XXXX`, `http://192.168.4.1`).
- Stop button; hold 3 s to stop and enter deep sleep (press again to wake/reboot).
- Optional web login (HTTP Basic Auth).
- No transcoding: the uploaded file is played exactly as uploaded.

## Hardware

The full purchase list is in [BOM.md](BOM.md); the complete connector and wiring plan is in
[CONNECTIONS.md](CONNECTIONS.md). The tables below are a quick summary.

| Part | Notes |
|---|---|
| ESP32-S3 DevKitC-1 **N16R8** | 16 MB flash, 8 MB octal PSRAM. PSRAM is required by the audio library. |
| microSD card + SPI adapter | FAT32, 32 GB recommended; 3.3 V logic |
| PN532 NFC breakout | I2C mode, address `0x24`, 3.3 V |
| MAX98357A I2S amplifier | 3 W into 4 Ω, 5 V supply |
| 3 W 4 Ω speaker | Enclosure-mounted |
| 2 momentary buttons | Stop and reserved |
| 1 LED + 330 Ω resistor | Status |
| USB-C power bank | 5 V/2 A; the DevKitC-1 is powered over USB-C |
| Enclosure, headers, wires | 2 mm plastic window in front of the NFC antenna |

## Wiring

[CONNECTIONS.md](CONNECTIONS.md) has the full pin table, connector list and assembly
checklist.

| ESP32-S3 pin | Connects to |
|---|---|
| GPIO4 | MAX98357A BCLK |
| GPIO5 | MAX98357A LRC |
| GPIO6 | MAX98357A DIN |
| GPIO10 | microSD CS |
| GPIO11 | microSD MOSI |
| GPIO12 | microSD SCK |
| GPIO13 | microSD MISO |
| GPIO8 | PN532 SDA |
| GPIO9 | PN532 SCL |
| GPIO21 | PN532 IRQ (optional) |
| GPIO47 | PN532 RSTPDN |
| GPIO1 | Button 1 (stop) → GND |
| GPIO2 | Button 2 (reserved) → GND |
| GPIO7 | LED anode → 330 Ω → GND |
| 3V3 | PN532 VCC, microSD VCC |
| 5V | MAX98357A VIN |
| GND | Common ground |

PN532 must be set to **I2C mode** with its SEL jumpers. On the common Adafruit-style board
the default is I2C; if the reader is not detected, check the board's printed SEL table and
the I2C address (`0x24`).

Do not connect the MAX98357A speaker outputs to ground; they are bridge-tied. Keep the
I2S and SD wires short, and place the NFC antenna behind a thin plastic window away from
metal.

## Build and flash

The project is a normal Arduino sketch (`NFCPlayer/NFCPlayer.ino`) built with the
**ESP32 Arduino core** through `arduino-cli`. There is no PlatformIO and no required
VS Code plugin.

Prerequisites: `git`, `curl`, `make`, and a Linux/macOS shell (or WSL/Git Bash on
Windows). The toolchain and libraries are installed **inside this checkout**, not in
your home directory.

```bash
make setup                       # download arduino-cli, ESP32 core and libraries
make build                       # compile for ESP32-S3 N16R8
make board                       # list serial ports
make flash PORT=/dev/ttyACM0     # upload
make monitor PORT=/dev/ttyACM0   # serial monitor at 115200
```

The scripts can also be called directly (`./scripts/setup.sh`, `build.sh`, `board.sh`,
`flash.sh`, `monitor.sh`). The board options are in `scripts/build.sh`:

```text
esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc
```

This selects 16 MB flash, octal PSRAM, the 16 MB partition table and USB CDC. The
firmware **will not decode audio without PSRAM**; a classic ESP32 is not supported.

`arduino-cli` can be used from any editor; the Arduino VS Code extension is optional.
There is no filesystem upload step: the HTML/CSS/JS is embedded in the firmware from
`NFCPlayer/web_ui.h`.

## First boot

1. Power the board. If no Wi-Fi is saved, it starts an access point named
   `NFCPlayer-XXXX` (last four hex digits of the MAC).
2. Join it with the default password `nfcplayer` (change it later in Settings).
3. Open `http://192.168.4.1`.
4. In **Settings → Wi-Fi**, enter your home network and reboot. On the home network the
   player is reachable at `http://nfcplayer.local` (or its DHCP address).
5. Set an optional web login password in Settings. If set, the browser prompts for
   username `admin` and that password.

Wi-Fi must be 2.4 GHz; the ESP32-S3 radio does not support 5 GHz.

## Using it

### Upload a song

Open **Files**, choose an audio file, and press **Upload**. The file streams to
`/audio/` on the SD card. The upload is written to a temporary file and renamed only when
complete, so an interrupted upload does not leave a half-file in the library.

- Maximum size: 64 MB.
- Supported: `.mp3`, `.m4a`, `.aac`, `.flac`, `.ogg`, `.opus`, `.wav`.
- Playback stops before the upload starts; the ESP32's web server handles one request at a
  time, so the UI is unresponsive during a large upload.
- The SD card must have at least 2 MB free after the upload.

### Learn a card

1. Open **Cards → Learn new card**.
2. Tap an NFC card on the reader within 60 seconds.
3. Choose the track and a label, then **Save card**.
4. Remove the card and test it.

A card maps to exactly one file. Re-learning an existing card overwrites its assignment.
Deleting a file also removes any card mappings that point at it.

### Play

- Tap a learned card: play from the beginning.
- Tap the same card again: stop.
- Tap a different card: stop the current song and play the new one.
- Lift the card: playback continues to the end.
- Stop button: stop immediately.
- Hold the Stop button for 3 seconds: stop and enter deep sleep. Press the button again
  to wake the board (it reboots).
- Unknown card: the LED flashes three times and nothing plays.

The second button is unassigned in this version.

## SD card layout

```text
/audio/
  song.mp3
  another-song.m4a
/config/
  cards.json
```

`cards.json` is small and is rewritten atomically (temp file + rename). Keep a copy if the
card contains valuable mappings.

## HTTP API

The UI is a thin client over these endpoints. All require the optional Basic Auth login.

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Embedded web UI |
| GET | `/api/status` | Player, network and SD status |
| GET | `/api/files` | List audio files and free space |
| POST | `/api/upload` | Multipart file upload (`file` field) |
| POST | `/api/play?name=...` | Play a file |
| POST | `/api/delete?name=...` | Delete a file and its card mappings |
| POST | `/api/stop` | Stop playback |
| POST | `/api/volume?level=0..21` | Set and save volume |
| POST | `/api/learn/start` | Start a 60 s learn window |
| GET | `/api/learn` | Poll learn UID |
| POST | `/api/learn/cancel` | Cancel learn mode |
| GET | `/api/cards` | List card mappings |
| POST | `/api/cards?uid=...&file=...&label=...` | Create/update a mapping |
| POST | `/api/cards/delete?uid=...` | Delete a mapping |
| GET | `/api/settings` | Read settings |
| POST | `/api/settings?...` | Update device/volume/AP/web password |
| POST | `/api/wifi?ssid=...&pass=...` | Save Wi-Fi and reboot |
| GET | `/api/wifi/scan` | Scan nearby networks |
| POST | `/api/reboot` | Reboot |

## Troubleshooting

| Symptom | Check |
|---|---|
| `no PSRAM found` on serial | Use an N16R8 board and the `qio_opi` memory setting; PSRAM is mandatory |
| SD mount failed | 3.3 V supply, CS/MOSI/MISO/SCK wiring, FAT32 card, try a smaller card |
| PN532 not detected | I2C SEL jumpers, 3.3 V, SDA/SCL, address `0x24`, reset pin |
| Upload returns 400 | File extension unsupported, file over 64 MB, or SD full |
| Upload returns 401 | Web login is enabled; reload the page and enter `admin` + password |
| AP does not appear | Wait 15 s after boot; check 2.4 GHz; power-cycle |
| `nfcplayer.local` does not resolve | Use the IP shown in Settings/status; some networks block mDNS |
| No sound | Speaker on amp outputs, 5 V at VIN, I2S pins, file format, volume |
| Card does nothing | Card is not learned, its file is missing, or the reader is not ready |

## Security and limits

- This is a LAN/AP device; do not port-forward it to the internet.
- HTTP is plain text. Set the AP password and, on shared networks, the web password.
- No OTA updates, no HTTPS, no multi-room, no playlists or resume.
- There is no YouTube or `yt-dlp` integration by design. Download audio elsewhere and
  upload it; only upload media you have the right to use.

## References

- ESP32-audioI2S: <https://github.com/schreibfaul1/ESP32-audioI2S>
- Adafruit PN532: <https://github.com/adafruit/Adafruit-PN532>
- Arduino ESP32 WebServer: <https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer>
- Arduino CLI: <https://arduino.github.io/arduino-cli/>
- ESP32 Arduino core: <https://github.com/espressif/arduino-esp32>

## License note

`ESP32-audioI2S` is GPL-3.0. If you distribute the combined firmware, comply with the
GPL-3.0 source-availability terms. Adafruit PN532, ArduinoJson and the Arduino core keep
their own licenses.
