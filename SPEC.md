# NFC Audio Player — ESP32-S3 Specification

**Status:** Implemented; `make build` verified (Arduino CLI, ESP32 core 3.3.12)
**Version:** 2.1
**Last updated:** 2026-11-06
**Target hardware:** ESP32-S3 DevKitC-1 N16R8, single board
**Repository:** `nfc-player`

**Related documents:** [README.md](README.md) · [BOM.md](BOM.md) · [CONNECTIONS.md](CONNECTIONS.md)

---

## Contents

1. [Overview](#1-overview)
2. [Hardware](#2-hardware)
3. [Firmware Architecture](#3-firmware-architecture)
4. [Storage and Data Model](#4-storage-and-data-model)
5. [Web Interface and API](#5-web-interface-and-api)
6. [Network](#6-network)
7. [Build and Flash](#7-build-and-flash)
8. [Testing and Acceptance](#8-testing-and-acceptance)
9. [Risks and Limitations](#9-risks-and-limitations)
10. [Milestones](#10-milestones)
11. [Appendices](#11-appendices)

---

## 1. Overview

### 1.1 Purpose

A child-friendly NFC audio player. A physical card is placed on the reader; the player looks
up the card's assigned song and plays it through a small speaker. Audio files are placed on
the local SD card by uploading them through the player's own web interface from a phone or
computer. There is no external server, no downloader service, and no transcoding step.

### 1.2 Goals

- **Card in, music out.** A known card starts its song in under one second; tapping it
  again stops.
- **One board.** The ESP32-S3 runs NFC, playback, the web server, and SD storage.
- **Browser uploads.** Drag/pick an audio file in the web UI; it is streamed to the SD card
  and appears in the library without removing the card.
- **Offline playback.** Wi-Fi is only needed for uploads and card management.
- **Kid-proof.** Two physical buttons, a status LED, no display, and a web UI that is only
  needed by the parent.
- **Recoverable.** Interrupted uploads leave no partial library entries; card mappings are
  stored in a small JSON file and survive reboots.

### 1.3 Non-goals (v1)

- No YouTube/`yt-dlp` integration, no URL downloads, and no cloud service.
- No audio transcoding or loudness normalization; the uploaded file is played as-is.
- No playlists, queues, shuffle, resume position, or crossfade. One card = one song.
- No display, touchscreen, battery fuel gauge, or internal charger.
- No multi-room, OTA update, HTTPS, or public internet exposure.
- No support for the original ESP32 (the audio library requires PSRAM and a multi-core
  ESP32-S3/P4-class chip).

### 1.4 Target user

A parent or maker who can flash an ESP32-S3, connect a handful of jumper wires, and use a
phone browser. The child only touches cards and the Stop button.

### 1.5 Key decisions

| Area | Decision |
|---|---|
| Board | ESP32-S3 DevKitC-1 N16R8: 16 MB flash, 8 MB octal PSRAM, Wi-Fi, USB-C |
| Audio | `ESP32-audioI2S` 3.4.4 + MAX98357A I2S amp + 3 W 4 Ω speaker |
| NFC | PN532 over I2C at address `0x24`, 3.3 V |
| Storage | One microSD card over SPI, FAT32, `/audio` + `/config` |
| Upload | Browser multipart POST, streamed to a temp file, atomic rename |
| Library | Flat files; no database; metadata is the filename |
| Cards | UID → filename mapping in `/config/cards.json` |
| Settings | ESP32 NVS via `Preferences` |
| Network | STA with 15 s timeout; fallback AP after 60 s; mDNS `nfcplayer.local` |
| Auth | Optional HTTP Basic Auth (`admin` + configured password) |
| Buttons | Button 1 stop / 3 s hold = deep sleep; button 2 reserved |
| LED | One GPIO status LED |

---

## 2. Hardware

### 2.1 Bill of materials

Prices are approximate USD, excluding shipping and tools. The detailed purchase list
(including optional spares and sourcing notes) is in [BOM.md](BOM.md).

| Item | Specification | Qty | Unit | Total |
|---|---|---:|---:|---:|
| ESP32-S3 DevKitC-1 | N16R8, 16 MB flash, 8 MB PSRAM, USB-C | 1 | $15 | $15 |
| microSD card | 32 GB, FAT32, high endurance | 1 | $10 | $10 |
| microSD SPI adapter | 3.3 V, level-shifted if needed | 1 | $3 | $3 |
| PN532 NFC breakout | 13.56 MHz, I2C, 3.3 V | 1 | $15 | $15 |
| NTAG215 stickers | 25 mm, 504 bytes | 20 | $0.60 | $12 |
| MAX98357A breakout | I2S mono class-D, 3.2 W | 1 | $6 | $6 |
| Speaker | 3 W, 4 Ω, 40–50 mm | 1 | $5 | $5 |
| Tactile buttons | 6 mm through-hole | 2 | $0.50 | $1 |
| LED + passives | 3 mm LED, 330 Ω, 470 µF, 100 µF, 100 nF | 1 set | $4 | $4 |
| Headers and wires | 2.54 mm, Dupont | 1 set | $5 | $5 |
| USB-C data cable | 0.5 m, data + power | 1 | $6 | $6 |
| Power bank | 10,000 mAh, 5 V/2 A, USB-C | 1 | $25 | $25 |
| Enclosure | 3D-printed PLA/PETG | 1 | $8 | $8 |
| **Total** | | | | **~$115** |

Optional: inline USB-C switch (~$4), heatsink (~$2), spare SD card (~$10), second cable
(~$6). See [BOM.md](BOM.md).

### 2.2 Pinout

The complete wiring list, connector plan and assembly checklist is in
[CONNECTIONS.md](CONNECTIONS.md).

| ESP32-S3 pin | Direction | Connects to |
|---|---|---|
| GPIO4 | Out | MAX98357A BCLK |
| GPIO5 | Out | MAX98357A LRC |
| GPIO6 | Out | MAX98357A DIN |
| GPIO10 | Out | microSD CS |
| GPIO11 | Out | microSD MOSI |
| GPIO12 | Out | microSD SCK |
| GPIO13 | In | microSD MISO |
| GPIO8 | I/O | PN532 SDA |
| GPIO9 | Out | PN532 SCL |
| GPIO21 | In | PN532 IRQ (optional) |
| GPIO47 | Out | PN532 RSTPDN |
| GPIO1 | In | Button 1 (stop) → GND |
| GPIO2 | In | Button 2 (reserved) → GND |
| GPIO7 | Out | LED → 330 Ω → GND |
| 3V3 | Power | PN532 VCC, microSD VCC |
| 5V | Power | MAX98357A VIN |
| GND | Power | Common ground |

**Do not use:** GPIO0/3/45/46 (strapping), GPIO19/20 (native USB), GPIO26–37 (flash and
octal PSRAM), GPIO43/44 (UART0), GPIO48 (onboard RGB LED on many boards).

### 2.3 Wiring notes

**MAX98357A**

- BCLK/LRC/DIN from GPIO4/5/6; VIN from 5 V; GND common.
- Place a 470 µF electrolytic and a 100 nF ceramic across VIN/GND near the amp.
- Speaker connects to the amp's `+` and `−` only. The outputs are bridge-tied; never
  connect either speaker terminal to ground.
- Leave `SD` enabled and `GAIN` at its default (9 dB). If it is too loud at low volume,
  fit the gain resistor for 6 dB.

**microSD**

- Use a 3.3 V adapter. Some modules include a 5 V regulator and level shifter; if so,
  power it from 5 V and connect the logic pins to the ESP32's 3.3 V levels.
- Keep the SPI wires short. Add a 100 µF capacitor across the card's 3.3 V/GND if reads
  are unreliable.
- The card must be FAT32. The firmware creates `/audio` and `/config` on first mount.

**PN532**

- Set the module to **I2C mode** with its SEL0/SEL1 jumpers (check the printed table on
  the board; the common Adafruit-style board ships in I2C mode).
- I2C address is `0x24`. Power from 3.3 V. SDA/SCL are GPIO8/GPIO9.
- RSTPDN is driven from GPIO47; IRQ is optional and not used for polling.
- Keep the antenna behind a 2–3 mm plastic window, away from metal and the ESP32 antenna.
- Range is typically 2–5 cm with an NTAG215 sticker.

**Buttons and LED**

- Buttons use internal pull-ups; the other side goes to GND. 50 ms software debounce.
- LED is active-high through 330 Ω. Patterns are driven by `NFCPlayer/status_led.cpp`.

### 2.4 Power

- ESP32-S3 active with Wi-Fi: roughly 0.5–1.0 W; the amp can add up to ~3 W peaks.
- A 10,000 mAh USB-C power bank runs the device for days at idle and many hours at
  playback; the exact figure depends on the speaker level and Wi-Fi.
- The board is powered over USB-C. Use a 5 V/2 A source; do not back-feed the 5 V pin while
  USB is connected.
- Long-press Stop enters deep sleep. On the next button press the board wakes and reboots.
  Deep sleep does not cut the power bank; unplug it for storage.
- There is no battery gauge in v1.

### 2.5 Enclosure

- 2–3 mm plastic over the PN532 coil; no metal, foil, or conductive paint nearby.
- Speaker grille with enough open area for 3 W; avoid rattling panels.
- Vent slots above the ESP32-S3; the board warms up during playback and Wi-Fi.
- Cutouts for USB-C, the two buttons, and a light pipe for the LED.
- A label with the AP SSID and default AP password, plus `http://192.168.4.1`.
- The power bank can live in an external pocket or be strapped underneath.

---

## 3. Firmware Architecture

### 3.1 File map

| File | Responsibility |
|---|---|
| `NFCPlayer/NFCPlayer.ino` | Boot order and the main loop |
| `NFCPlayer/config.h` | Pins, limits, defaults, firmware version |
| `NFCPlayer/settings.cpp` | NVS-backed settings (Wi-Fi, AP/web password, device name, volume) |
| `NFCPlayer/storage.cpp` | SD mount, filename policy, file list, atomic commit |
| `NFCPlayer/cards.cpp` | Card map in RAM and `/config/cards.json` |
| `NFCPlayer/nfc_reader.cpp` | PN532 I2C polling, UID debounce, learn capture |
| `NFCPlayer/audio_player.cpp` | `ESP32-audioI2S` wrapper, volume, EOF |
| `NFCPlayer/player.cpp` | Idle/playing/learning state machine and deep sleep |
| `NFCPlayer/buttons.cpp` | Debounce, short/long press |
| `NFCPlayer/status_led.cpp` | Non-blocking LED patterns |
| `NFCPlayer/net.cpp` | STA/AP fallback, mDNS, reboot |
| `NFCPlayer/web_server.cpp` | HTTP routes, JSON, multipart upload, auth |
| `NFCPlayer/web_ui.h` | Embedded HTML/CSS/JS (no filesystem upload step) |

### 3.2 Boot order

1. Serial starts, PSRAM is checked.
2. Settings are loaded from NVS; defaults are applied.
3. LED starts in boot pattern.
4. SD card mounts; `/audio` and `/config` are created; stale temp upload is removed.
5. Card mappings are loaded from `/config/cards.json`.
6. Wi-Fi STA is attempted for 15 s; otherwise the fallback AP starts.
7. Web server starts on port 80.
8. Audio I2S is configured; PN532 is initialized; buttons and the player state machine are
   started.
9. LED settles to idle (or AP mode), and the device is ready.

### 3.3 Main loop

```text
audioLoop()   -> audio.loop(), EOF handling
nfcLoop()     -> PN532 poll, debounce, learn capture
buttonsLoop() -> stop / long-press
playerLoop()  -> timeout and playback state reconciliation
netLoop()     -> STA retry / AP fallback
webLoop()     -> WebServer.handleClient()
ledLoop()     -> non-blocking pattern
delay(1)
```

The web server is single-client and synchronous. A large upload blocks the loop; playback
is stopped before the upload starts. This is an accepted v1 tradeoff.

### 3.4 Player state machine

States: `IDLE`, `PLAYING`, `LEARNING`.

| From | Event | To | Action |
|---|---|---|---|
| IDLE | known card tap | PLAYING | Play mapped file, LED playing |
| IDLE | unknown/missing card | IDLE | LED error flash, log |
| PLAYING | same card tap | IDLE | Stop, LED idle |
| PLAYING | another card tap | PLAYING | Stop current, play new |
| PLAYING | card removed | PLAYING | Continue |
| PLAYING | audio EOF | IDLE | Clear current, LED idle |
| PLAYING | Stop button | IDLE | Stop, LED idle |
| IDLE | Stop button | IDLE | No-op |
| IDLE/PLAYING | Learn start | LEARNING | Stop, LED learning, 60 s window |
| LEARNING | card tap | LEARNING | Capture first UID only |
| LEARNING | save/cancel/timeout | IDLE | Clear window, LED idle |
| any | Stop held 3 s | sleeping | Stop, SD flush, deep sleep |

Unknown cards are not stored. There is no pending-card list: a card must be tapped during
a learn window to be assigned.

### 3.5 NFC polling

- `Adafruit_PN532` over `Wire` at 100 kHz.
- Poll `readPassiveTargetID(..., 20 ms)` so playback and the web server keep getting
  serviced.
- A UID event fires when the UID changes, or when the same UID reappears after it has been
  absent for 1.5 s and at least 2 s have passed since the last event. Holding a card does
  not repeat.
- Learn mode captures the first UID for 60 s; later taps are ignored until the window
  closes or the web UI cancels it.
- If the reader fails to initialize, the firmware retries every 5 s and the dashboard shows
  `nfc: false`. Playback from the web and buttons still work.

### 3.6 Audio

- `Audio::setPinout(GPIO4, GPIO5, GPIO6)` and `Audio::setVolumeSteps(21)`.
- Files are played with `connecttoFS(SD, "/audio/<name>")`.
- Supported by the library: MP3, M4A/AAC, FLAC, OGG/Opus, WAV. Other formats fail to play;
  they can still be uploaded only if their extension is in the allowlist, so the UI
  prevents most mistakes.
- Volume 0–21, default 15, persisted in NVS.
- EOF is delivered by the library's info callback; the player returns to idle.
- There is no software volume normalization. Loud and quiet files keep their own levels.

### 3.7 Buttons and LED

- Stop: short press stops playback. Hold 3 s stops playback, flushes the SD card, and calls
  `esp_deep_sleep_start()` with GPIO1 as the wake source.
- Button 2: reserved, logs a press, no action.
- LED: boot slow blink, idle pulse every 4 s, solid while playing, fast blink while
  learning, 500 ms blink in AP mode, three quick blinks on error.

### 3.8 Settings (NVS namespace `nfcplayer`)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `ssid` | string | empty | Home Wi-Fi SSID |
| `pass` | string | empty | Home Wi-Fi password |
| `ap_pass` | string | `nfcplayer` | Fallback AP password (≥ 8 chars) |
| `web_pass` | string | empty | Optional Basic Auth password |
| `dev_name` | string | `nfcplayer` | mDNS hostname |
| `volume` | uint8 | 15 | 0–21 |

Settings are loaded at boot; Wi-Fi changes require a reboot. The AP password change also
requires a reboot.

---

## 4. Storage and Data Model

### 4.1 SD layout

```text
/audio/
  <song>.mp3
  <song>.m4a
  .upload.tmp        only during an upload, removed on boot
/config/
  cards.json
  cards.tmp          only during a save
```

- FAT32, 32 GB recommended.
- The firmware creates the directories on first mount.
- There is no database. The filename is the display name; the card map points at it.
- Keep at least 2 MB free after an upload. The UI shows free space.

### 4.2 Card map

`/config/cards.json`:

```json
{
  "cards": [
    {
      "uid": "04A1B2C3D4E5F6",
      "file": "bedtime-song.mp3",
      "label": "Bedtime song"
    }
  ]
}
```

- UID is uppercase hex, no separators; 8 or 14 characters (4- or 7-byte UID).
- One UID maps to one file. The same file may be used by several cards.
- `cardsUpsert` updates an existing UID or appends a new one.
- Deleting a file removes its card mappings.
- The file is written to `cards.tmp`, then the old file is removed and the temp is renamed.
  A power cut during the rewrite can lose the map; keep a copy if it matters.

### 4.3 Upload atomicity

1. Browser sends `multipart/form-data` with one file field.
2. `UPLOAD_FILE_START`: authenticate, validate extension, choose a unique filename, check
   the 64 MB limit, check free space, stop playback, open `/audio/.upload.tmp`.
3. `UPLOAD_FILE_WRITE`: append each `HTTPUpload` chunk to the temp file; reject if the total
   exceeds 64 MB or a write fails.
4. `UPLOAD_FILE_END`: flush/close; rename to the final name; reply JSON.
5. On abort/error: close, delete the temp file, reply with an error.

The library entry only appears after a successful rename.

### 4.4 Filename policy

- Strip any path components (`/`, `\`).
- Keep ASCII letters, digits, spaces, `_`, `-`; replace everything else with `_`.
- Base name is capped at 48 characters; extension is lowercased.
- Allowed extensions: `mp3`, `m4a`, `aac`, `flac`, `ogg`, `opus`, `wav`.
- If the name exists, append `-1`, `-2`, ... up to `-99`.
- `.` and `..` are rejected.

---

## 5. Web Interface and API

### 5.1 Pages

| Path | Purpose |
|---|---|
| `/` | Single-page UI: Play, Files, Cards, Settings |

The UI is embedded in `NFCPlayer/web_ui.h` and served from flash. It has no external fonts,
scripts, or CDN dependencies, so it works on the fallback AP.

- **Play:** now playing, position/duration, stop, volume.
- **Files:** upload form with progress, file list with play/delete, free space.
- **Cards:** learn button, UID display, file/label picker, known-card list, delete.
- **Settings:** device name, volume, AP password, optional web password, Wi-Fi scan/save,
  system info, reboot.

### 5.2 Authentication

- If `web_pass` is empty, the UI is open on the LAN/AP.
- If set, every route uses HTTP Basic Auth with username `admin`.
- The browser prompts on page load; uploads and fetches reuse the credentials.
- The AP itself uses WPA2 with `ap_pass`.
- HTTP is plain text; this is a trusted-LAN device. Do not port-forward it.

### 5.3 API

All endpoints require auth when enabled.

| Method | Path | Request | Response |
|---|---|---|---|
| GET | `/api/status` | — | state, file, card, volume, position, duration, ip, ap, ap_ssid, rssi, nfc, sd, free, total, uptime, version |
| GET | `/api/files` | — | `files[{name,size}]`, free, total |
| POST | `/api/upload` | multipart `file` | `{ok,name,size}` |
| POST | `/api/play` | `?name=` | `{ok,file}` |
| POST | `/api/delete` | `?name=` | `{ok}` |
| POST | `/api/stop` | — | `{ok}` |
| POST | `/api/volume` | `?level=0..21` | `{ok,volume}` |
| POST | `/api/learn/start` | — | `{ok,active}` |
| GET | `/api/learn` | — | `{active,uid}` |
| POST | `/api/learn/cancel` | — | `{ok}` |
| GET | `/api/cards` | — | `cards[{uid,file,label,exists}]` |
| POST | `/api/cards` | `?uid=&file=&label=` | `{ok,uid,file}` |
| POST | `/api/cards/delete` | `?uid=` | `{ok}` |
| GET | `/api/settings` | — | device, volume, ap_pass, ap, ap_ssid, ip, ssid, web_auth, fw, free, total, nfc, sd |
| POST | `/api/settings` | `?device=&volume=&ap_pass=&web_pass=&web_clear=` | `{ok,message,reboot_required}` |
| POST | `/api/wifi` | `?ssid=&pass=` | `{ok,message}` then reboot |
| GET | `/api/wifi/scan` | — | `{networks[{ssid,rssi,secure}],count}` |
| POST | `/api/reboot` | — | `{ok,message}` |

Error format:

```json
{"error": "message"}
```

Status codes: 200, 400 (bad input / unsupported file), 401 (login), 404 (missing),
413 (over 64 MB), 500 (SD/write/playback failure).

### 5.4 Upload limits

- Maximum file size: 64 MB.
- One upload at a time; the web server is single-client.
- Playback stops at upload start.
- If the SD card is missing, the upload is rejected.
- If the free-space check fails, the upload is rejected before writing.
- The UI shows a browser-side progress bar from `XMLHttpRequest.upload.onprogress`.

---

## 6. Network

### 6.1 Station mode

- At boot, if an SSID is stored, `WiFi.begin()` is attempted for 15 s.
- On success: DHCP address, mDNS `nfcplayer.local`, web server on port 80.
- If it fails: fallback AP starts.
- If the connection drops later, the firmware retries every 30 s and starts the AP after
  60 s without a connection.

### 6.2 Fallback access point

- SSID: `NFCPlayer-XXXX`, where `XXXX` is the low 16 bits of the eFuse MAC in hex.
- WPA2 password from `ap_pass` (default `nfcplayer`; change it).
- IP: `192.168.4.1`; open `http://192.168.4.1`.
- AP and STA are not concurrent: saving Wi-Fi reboots the board.
- The AP SSID and default password are printed on the enclosure label.

### 6.3 mDNS

- `MDNS.begin(dev_name)` publishes `http://<dev_name>.local`.
- Some networks block multicast DNS; use the IP shown in Settings/status instead.
- Changing the device name requires a reboot before mDNS reflects it.

### 6.4 Time

The device does not need accurate time for playback or uploads. There is no NTP dependency.

---

## 7. Build and Flash

### 7.1 Toolchain

- `arduino-cli` with the ESP32 Arduino core. No PlatformIO and no mandatory editor plugin.
- The toolchain, core and libraries are installed inside the checkout under `.tools/`,
  `.arduino15/` and `.arduino-user/`.
- Board FQBN: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc`
  (ESP32-S3, 16 MB flash, 8 MB octal PSRAM, USB CDC).
- Libraries installed by `scripts/setup.sh`:
  - ArduinoJson 7.4.3
  - Adafruit BusIO 1.17.4
  - Adafruit PN532 1.3.4
  - ESP32-audioI2S 3.4.4 (cloned from GitHub into the local library directory)

### 7.2 Commands

```bash
make setup                       # install arduino-cli, ESP32 core and libraries
make build                       # compile
make board                       # list serial ports
make flash PORT=/dev/ttyACM0     # upload
make monitor PORT=/dev/ttyACM0   # serial monitor at 115200
```

Verified build (2026-11-06):

```text
Sketch uses 1,974,239 bytes (62%) of program storage space. Maximum is 3,145,728 bytes.
Global variables use 64,060 bytes (19%) of dynamic memory, leaving 263,620 bytes.
```

Equivalent scripts: `./scripts/setup.sh`, `./scripts/build.sh`, `./scripts/board.sh`,
`./scripts/flash.sh`, `./scripts/monitor.sh`. The sketch is `NFCPlayer/NFCPlayer.ino`; all sources live in that
folder, so there is no build-system-specific layout.

Expected boot output:

```text
=== NFC Player 1.0.0 ===
[boot] PSRAM: 8388608 bytes
[sd] mounted, total=... used=...
[cards] loaded N mappings
[net] joined <ssid> -> <ip>
[web] listening on port 80
[nfc] PN532 ready, firmware=0x...
[boot] ready: station mode  http://<ip>/
```

### 7.3 Board variants

The FQBN above is for the N16R8 variant. For another ESP32-S3 module, change the
`PSRAM`/`FlashSize`/`PartitionScheme` menu options in `scripts/build.sh` and
`scripts/flash.sh`. A board without PSRAM cannot decode audio with this firmware.

A board without PSRAM can run the web UI and uploads but will fail to decode audio.

---

## 8. Testing and Acceptance

### 8.1 Static/build checks

- `make build` completes with no errors.
- No warnings in project files (library warnings are acceptable).
- `grep` confirms no leftover downloader/Pi code paths.
- The embedded UI is served and parses without JavaScript console errors.

### 8.2 Hardware bring-up

| Check | Expected |
|---|---|
| Serial boot | PSRAM size > 0, SD mounted, web listening |
| `aplay` equivalent | Not applicable; audio begins on first card/web play |
| PN532 | `nfc: ready` in status; UID appears in serial on tap |
| SD | File upload appears in the Files list |
| Buttons | Stop stops; 3 s hold enters deep sleep; press wakes/reboots |
| LED | Boot, idle, playing, learning, AP, error patterns |
| Speaker | Clean audio at moderate volume, no clipping at 21 |

### 8.3 End-to-end matrix

| ID | Steps | Expected |
|---|---|---|
| E1 | Fresh boot, no Wi-Fi | AP appears; `http://192.168.4.1` loads |
| E2 | Save home Wi-Fi, reboot | Joins LAN; `nfcplayer.local` or IP works |
| E3 | Upload a 5 MB MP3 | Progress completes; file listed; playback works |
| E4 | Upload an M4A/AAC file | Listed; plays |
| E5 | Upload a 70 MB file | 413 error; no file listed |
| E6 | Upload `.txt` renamed `.mp3` | Rejected or fails to play; never corrupts library |
| E7 | Interrupt an upload (Wi-Fi off) | No partial file in the list; temp cleaned on reboot |
| E8 | Learn a card, assign a song | Card listed; tap plays |
| E9 | Tap the same card again | Stops |
| E10 | Tap card A, then card B | A stops, B plays |
| E11 | Lift the card mid-song | Playback continues |
| E12 | Let the song end | Idle |
| E13 | Stop button | Immediate stop |
| E14 | Hold Stop 3 s | Deep sleep; button wakes/reboots |
| E15 | Unknown card | LED error; no playback |
| E16 | Delete a mapped file | Card mapping removed; card no longer plays |
| E17 | Web login set, reload | Browser prompts; wrong password rejected |
| E18 | Power loss during upload | Reboot; no partial file |
| E19 | 50 upload/play cycles | No leak, no crash, SD still mounts |
| E20 | SD removed | Upload rejected; UI/status shows `sd: false` |

### 8.4 Acceptance criteria

| Criterion | Target |
|---|---|
| Boot to web UI (STA) | ≤ 20 s |
| Boot to fallback AP | ≤ 20 s |
| Tap-to-audio | ≤ 1 s for a local file |
| Stop latency | ≤ 300 ms |
| Upload 5 MB over 2.4 GHz | ≤ 30 s |
| Playback formats | MP3, M4A/AAC, FLAC, OGG/Opus, WAV |
| Card mappings survive 20 reboots | Yes |
| Uploads survive power loss | No partial file; temp removed |
| Deep sleep wake | Button press reboots and plays |
| UI | Works on a phone with no external assets |

---

## 9. Risks and Limitations

| Risk | Impact | Mitigation |
|---|---|---|
| Board lacks PSRAM or wrong memory config | Audio will not decode | Use N16R8; verify `qio_opi`; check boot log |
| PN532 I2C mode/address wrong | No card reads | Set SEL jumpers to I2C; check `0x24`; verify 3.3 V |
| SD SPI wiring/level issues | Mount or upload failures | Short wires, 3.3 V, 100 µF decoupling, FAT32 |
| Single-client web server blocks during upload | UI freezes, playback stops | Stop playback on upload; keep uploads < 64 MB; document |
| No transcoding | Unsupported codec won't play | Extension allowlist and README guidance |
| Wi-Fi/AP fallback flakiness | Cannot reach UI | Retry/AP fallback; reboot; use IP instead of mDNS |
| HTTP Basic Auth over plain HTTP | Password visible on LAN | Trusted LAN only; set strong AP/web passwords; no port forwarding |
| SD wear/corruption | Lost files/mappings | High-endurance card, atomic writes, backup `cards.json` |
| Deep sleep only wakes on button 1 | Device appears off | Document; press Stop to wake; power-cycle otherwise |
| Audio library/board compatibility | Build or playback issues | Pin library 3.4.4; ESP32 Arduino core 3.x; verify pin map |
| Content legality | Copyright/terms | Personal-use only; user is responsible for uploaded files |

There is **no downloader** in this design. Files must be obtained elsewhere and uploaded
through the browser.

---

## 10. Milestones

| Milestone | Deliverable | Exit criterion |
|---|---|---|
| M0 | Board, SD, PN532, amp, buttons wired | Boot log shows PSRAM, SD, PN532; speaker click/pop only |
| M1 | Web UI + upload + play | Upload an MP3 and play it from the browser |
| M2 | NFC + card map + learn | Tap a learned card and hear its song |
| M3 | STA/AP fallback, settings, auth | Phone joins AP, configures Wi-Fi, logs in |
| M4 | Enclosure, labels, deep sleep, soak | 50 upload/play cycles and 20 reboots pass |

---

## 11. Appendices

### Appendix A — Default AP and login

- AP SSID: `NFCPlayer-XXXX` (MAC suffix)
- AP password: `nfcplayer` (change in Settings)
- Web URL: `http://192.168.4.1` or `http://nfcplayer.local`
- Optional web login: username `admin`, password empty by default

### Appendix B — Boot/main loop order

```text
setup:
  Serial, PSRAM check
  settingsBegin()
  ledBegin()
  storageBegin() -> cardsBegin()
  netBegin() -> webBegin()
  audioBegin() -> nfcBegin()
  buttonsBegin() + handlers
  playerBegin()
  LED base mode

loop:
  audioLoop()
  nfcLoop()
  buttonsLoop()
  playerLoop()
  netLoop()
  webLoop()
  ledLoop()
  delay(1)
```

### Appendix C — Card map example

```json
{
  "cards": [
    {"uid": "04A1B2C3D4E5F6", "file": "twinkle.mp3", "label": "Twinkle"},
    {"uid": "04A1B2C3D4E5F7", "file": "bedtime.m4a", "label": "Bedtime"}
  ]
}
```

### Appendix D — Useful serial commands

There is no shell. Use the web UI or reflash. Serial output is the only diagnostic channel.

```text
make monitor PORT=/dev/ttyACM0
```

### Appendix E — References

- ESP32-audioI2S: <https://github.com/schreibfaul1/ESP32-audioI2S>
- Adafruit PN532: <https://github.com/adafruit/Adafruit-PN532>
- Arduino ESP32 WebServer: <https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer>
- ArduinoJson: <https://arduinojson.org/>
- Arduino CLI: <https://arduino.github.io/arduino-cli/>
- ESP32 Arduino core: <https://github.com/espressif/arduino-esp32>
- ESP32-S3 datasheet: <https://www.espressif.com/en/products/socs/esp32-s3>

---

## Appendix F — Explicitly removed from the previous design

The earlier Raspberry Pi / `yt-dlp` architecture is deleted, not supported by this
firmware. In particular there is:

- no Python, FastAPI, SQLite, `yt-dlp`, Deno, `ffmpeg`, or Pi;
- no URL ingestion or background downloader;
- no server-to-SD sync protocol.

Audio is uploaded directly through the ESP32 web UI and stored on the SD card.
