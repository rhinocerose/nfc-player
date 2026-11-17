# NFC Audio Player — Bug-Fix Specification

**Status:** Approved for implementation; not yet implemented
**Applies to:** firmware 1.0.0 (SPEC.md 2.1)
**Target release:** firmware 1.1.0
**Last updated:** 2026-11-17

**Related documents:** [SPEC.md](SPEC.md) · [BOM.md](BOM.md) · [CONNECTIONS.md](CONNECTIONS.md)

This document specifies fixes for defects found in a review of the 1.0.0 firmware and
documents. Each item states the defect, the required behavior, the files affected, and the
acceptance checks. Line numbers refer to the 1.0.0 sources and are a starting point only.

Once all items are implemented, fold the new behavior into SPEC.md and bump it to 2.2.

---

## Contents

| ID | Title | Severity |
|---|---|---|
| [BF-01](#bf-01-upload-size-and-free-space-checks-never-run) | Upload size and free-space checks never run | High |
| [BF-02](#bf-02-ap-fallback-is-permanent) | AP fallback is permanent | High |
| [BF-03](#bf-03-boot-blocks-on-wi-fi-before-audio-and-nfc) | Boot blocks on Wi-Fi before audio and NFC | High |
| [BF-04](#bf-04-remove-deep-sleep-add-amp-shutdown-control) | Remove deep sleep; add amp shutdown control | High |
| [BF-05](#bf-05-card-map-lost-on-power-cut-during-save) | Card map lost on power cut during save | Medium |
| [BF-06](#bf-06-cross-site-requests-can-change-the-player) | Cross-site requests can change the player | High |
| [BF-07](#bf-07-playing-state-can-stick-volume-writes-wear-nvs) | Playing state can stick; volume writes wear NVS | Medium |
| [BF-08](#bf-08-nfc-reader-is-never-rechecked-after-init) | NFC reader is never rechecked after init | Medium |
| [BF-09](#bf-09-ap-password-is-shared-and-exposed) | AP password is shared and exposed | Medium |
| [BF-10](#bf-10-documentation-errors) | Documentation errors | Low |

---

## BF-01 Upload size and free-space checks never run

### Defect

`handleUploadChunk()` (`NFCPlayer/web_server.cpp:445-452`) checks `up.totalSize` at
`UPLOAD_FILE_START`. The ESP32 WebServer resets `totalSize` to 0 at that point
(`Parsing.cpp:700`) and only accumulates it per chunk. The 64 MB pre-check and the
free-space check are therefore dead code, and an upload can fill the SD card. The per-chunk
64 MB check still works.

### Required behavior

1. The server determines an **expected size** before opening the temp file:
   - If the request has a `size` query parameter (`POST /api/upload?size=<bytes>`) that
     parses as a positive integer, use it.
   - Otherwise use the request's `Content-Length` header. It includes multipart overhead
     (a few hundred bytes), which is acceptable for limit checks.
   - If neither is available, the expected size is unknown.
2. `WebServer::collectHeaders()` is called in `webBegin()` so `Content-Length` is readable.
3. At `UPLOAD_FILE_START`, with a known expected size:
   - expected size > 64 MB → reject with **413**, `{"error":"file is larger than 64 MB"}`;
   - free space < expected size + 2 MB → reject with **507**,
     `{"error":"not enough free space on the SD card"}`.
4. With an unknown expected size, the upload proceeds and the per-chunk checks apply.
5. Per-chunk enforcement stays: abort when the running total exceeds 64 MB (413), and add a
   per-chunk free-space guard (abort when free space drops below 2 MB, 507). Both remove the
   temp file.
6. A `size` parameter that disagrees with the bytes actually received is not an error; the
   received count is authoritative and is what the response reports.
7. The embedded UI (`NFCPlayer/web_ui.h`, `xhr.open('POST', '/api/upload')`) appends
   `?size=<file.size>`.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T1.1 | Upload 70 MB file from the UI | 413 before any data is written; no temp file left |
| T1.2 | Same with `curl` (no `size` parameter) | 413 from `Content-Length`; no temp file left |
| T1.3 | Card with 10 MB free; upload 20 MB file | 507 before writing; library unchanged |
| T1.4 | `?size=1000` but send a 20 MB file to a card with 10 MB free | Aborted mid-upload with 507; temp removed |
| T1.5 | Normal 5 MB upload | 200; reported size equals bytes received |

SPEC.md §5.3 status codes gain **507**.

---

## BF-02 AP fallback is permanent

### Defect

`netLoop()` (`NFCPlayer/net.cpp:79`) returns immediately once `apActive` is set, and
`startAp()` switches to `WIFI_AP` (`net.cpp:35`), dropping station mode. After the home
network drops for 60 s (for example a router reboot), the player stays on its own AP until
it is power-cycled.

### Required behavior

1. When station mode has been disconnected for `AP_FALLBACK_AFTER_MS` (60 s), switch to
   `WIFI_AP_STA` and start the fallback AP. Station mode stays enabled.
2. While the AP is up, keep retrying the home network every `STA_RETRY_MS` (30 s).
3. When station mode reconnects, stop the AP (`WiFi.softAPdisconnect(true)`), return to
   `WIFI_STA`, and restart mDNS on the station interface.
4. Do not stop the AP while a client is associated to it
   (`WiFi.softAPgetStationNum() > 0`); stop it once the client count reaches 0 and station
   mode is still connected.
5. With no SSID stored, behavior is unchanged: AP only, no station retries.
6. `/api/status` and `/api/settings` report `ap` and `ip` for the current state; when both
   interfaces are up, `ip` is the station address and a new field `ap_ip` is the AP address.
7. LED shows the AP pattern only while the AP is up.

Note: in `WIFI_AP_STA` mode the AP follows the station's channel, so AP clients may be
briefly disconnected when a station retry scans channels. This is accepted.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T2.1 | Joined to home Wi-Fi; power off router 90 s | AP appears after ~60 s |
| T2.2 | Power router back on | Player rejoins within ~30 s of router being up; AP disappears |
| T2.3 | Phone joined to AP when router returns | AP stays up until the phone leaves |
| T2.4 | No SSID stored | AP only, as in 1.0.0 |
| T2.5 | Playback during T2.1–T2.2 | Uninterrupted |

---

## BF-03 Boot blocks on Wi-Fi before audio and NFC

### Defect

`setup()` calls `netBegin()` (`NFCPlayer/NFCPlayer.ino:53`) before `audioBegin()` and
`nfcBegin()` (`:56-57`). `netBegin()` busy-waits up to 15 s for the station connection
(`net.cpp:59`). Cards do nothing for up to 15 s after power-on.

### Required behavior

1. New boot order:
   1. Serial, PSRAM check
   2. Settings (NVS)
   3. LED, buttons, amp shutdown pin (BF-04)
   4. SD mount, card map load (with BF-05 recovery)
   5. Audio, NFC, player state machine
   6. Network start (non-blocking)
   7. Web server
2. `netBegin()` calls `WiFi.begin()` and returns immediately. `netLoop()` tracks the
   connection: on success it starts mDNS and logs the IP; if not connected within
   `STA_CONNECT_TIMEOUT_MS` (15 s) of boot, it starts the fallback AP per BF-02.
3. The web server starts at boot regardless of network state and serves on whichever
   interface comes up.
4. No `delay()` longer than 10 ms in `setup()` other than the PN532 reset timing.
5. Target: **power-on to audio from a known card ≤ 3 s**, measured with a known card already
   on the reader at power-on and with a card tapped 3 s after power-on.
6. A card already on the reader at boot triggers playback once (the NFC first-seen event
   fires normally).

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T3.1 | Saved SSID out of range; power on; tap card at 3 s | Audio by 3 s after power-on |
| T3.2 | Card on reader at power-on | Audio ≤ 3 s after power-on |
| T3.3 | Saved SSID in range | Joins in the background; UI reachable ≤ 20 s |
| T3.4 | No SSID stored | AP up ≤ 5 s; cards play ≤ 3 s |

SPEC.md §3.2, §8.4 (add "Power-on to card audio ≤ 3 s") and Appendix B change accordingly.

---

## BF-04 Remove deep sleep; add amp shutdown control

### Defect

The long-press deep sleep (`NFCPlayer/player.cpp:167-178`) is unreliable:

- GPIO1 has no RTC pull-up during sleep, so the ext0 wake pin can float and wake randomly.
- The PN532 and MAX98357A stay powered during sleep.
- Typical power banks switch off at deep-sleep current, so the wake button then does
  nothing and the device looks broken.
- A toddler holding the Stop button for 3 s triggers it.

### Required behavior

**Deep sleep is removed.**

1. Delete `playerShutdown()`, the `esp_sleep` include and wake configuration, and the
   long-press handler wiring in `NFCPlayer.ino:60`.
2. The Stop button has one action: a press stops playback as soon as it is debounced
   (on press, not release), once per press. Holding the button does nothing further. Stop
   latency stays ≤ 300 ms.
3. `BUTTON_LONG_MS` and the long-press code path are removed.
4. Power is switched with the inline USB-C switch, which becomes a **required** BOM item.

**Amp shutdown control is added.**

5. MAX98357A `SD` pin connects to **GPIO14** (`PIN_AMP_SD` in `config.h`).
6. GPIO14 is driven LOW (amp shut down) from the start of `setup()` until a playback starts.
7. Before `connecttoFS()`, drive GPIO14 HIGH and wait ≥ 5 ms before audio starts.
8. On stop, EOF, or playback failure, drive GPIO14 LOW after the I2S output has gone silent
   (≥ 20 ms after `stopSong()`).
9. HIGH on the MAX98357A `SD` pin selects the (L+R)/2 mono mix only if the breakout's
   on-board pull-up resistor network sets it; driving the pin directly to 3.3 V selects the
   left channel on most breakouts. Implementation must confirm against the breakout used
   and, if needed, keep the on-board resistor and drive the pin through it (open-drain LOW,
   released for HIGH). Record the chosen method in CONNECTIONS.md.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T4.1 | Hold Stop 10 s while playing | Playback stops; device stays on; nothing else happens |
| T4.2 | Idle with ear at speaker | No audible hiss |
| T4.3 | Start and stop 20 times | No audible pop at start or stop (subjective, at volume 15) |
| T4.4 | Play a stereo test file | Both channels audible in the mono mix |
| T4.5 | Inline switch off/on | Clean boot; BF-03 timing holds |

Doc changes: SPEC.md §1.5, §2.2 pinout (add GPIO14), §2.3, §2.4, §3.4 (remove `sleeping`
row), §3.7, §8.2, §8.3 E14 (replace with T4.1), §8.4 (remove deep-sleep wake), §9 (remove
deep-sleep risk), §10 M4; CONNECTIONS.md §1, §3 MAX98357A and Buttons, §5 J1 (use the spare
pin for SD); BOM.md core table (inline switch moves in) and power budget (remove deep sleep
row); README.md lines 15 and 145.

---

## BF-05 Card map lost on power cut during save

### Defect

`cardsSave()` (`NFCPlayer/cards.cpp:101`) writes `cards.tmp`, removes `cards.json`, then
renames. A power cut between the remove and the rename leaves no `cards.json`, and
`cardsBegin()` does not look at `cards.tmp`, so all mappings are lost.

### Required behavior

1. The save sequence is unchanged: write and close `cards.tmp`, remove `cards.json`, rename
   `cards.tmp` → `cards.json`.
2. In `cardsBegin()`, before loading:
   - If `cards.json` exists and parses, load it and delete any `cards.tmp`.
   - Else if `cards.tmp` exists and parses as a valid card map, remove `cards.json` (if
     present), rename `cards.tmp` → `cards.json`, log `[cards] recovered from cards.tmp`,
     and load it.
   - Else, if `cards.json` exists but does not parse, rename it to `cards.bad` (overwriting
     any previous one), log the error, and start with an empty map.
3. "Parses" means `deserializeJson` succeeds and `cards` is an array.
4. Remove the "power cut can lose the map" caveat from SPEC.md §4.2 and describe the
   recovery instead.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T5.1 | Delete `cards.json`, leave a valid `cards.tmp`; boot | Mappings recovered; `cards.json` exists |
| T5.2 | Valid `cards.json` and a stale `cards.tmp` | `cards.json` loaded; `cards.tmp` removed |
| T5.3 | Corrupt `cards.json`, no tmp | Empty map; `cards.bad` holds the old file |
| T5.4 | Cut power repeatedly while saving a card (10×) | Every boot has either the old or the new map |

---

## BF-06 Cross-site requests can change the player

### Defect

All state-changing endpoints are `POST` with query parameters and, by default, no
authentication. Any web page a parent opens on the same network can send
`fetch('http://nfcplayer.local/api/wifi?ssid=x', {method:'POST', mode:'no-cors'})`
and change Wi-Fi, delete files, reboot, or change passwords. Basic Auth does not prevent this
once the browser has cached credentials.

### Required behavior

1. Every `POST` route, including `/api/upload`, requires the header `X-NFC-Player: 1`.
   Requests without it get **403** `{"error":"missing X-NFC-Player header"}` and have no
   side effects.
2. For `/api/upload`, the header is checked at `UPLOAD_FILE_START`; without it, no temp file
   is created (same pattern as the existing auth check).
3. The check runs before the auth check, so unauthenticated cross-site requests also get
   403 rather than a login prompt.
4. The server sends no `Access-Control-Allow-*` headers and answers `OPTIONS` requests with
   **405**, so browsers never approve a cross-origin request carrying the header.
5. `WebServer::collectHeaders()` (BF-01) includes `X-NFC-Player`.
6. The embedded UI adds the header to every `fetch` POST (`web_ui.h:192` helper) and the
   upload `XMLHttpRequest`.
7. `GET` routes stay read-only and do not require the header.
8. SPEC.md §5.2 and README document the header and a `curl` example:
   `curl -X POST -H 'X-NFC-Player: 1' http://nfcplayer.local/api/stop`.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T6.1 | From another origin, `fetch(..., {method:'POST', mode:'no-cors'})` to `/api/stop` while playing | Playback continues; server logged 403 |
| T6.2 | Same with the custom header set | Browser sends a preflight; it fails; no request reaches the handler |
| T6.3 | `curl -X POST /api/reboot` without header | 403; no reboot |
| T6.4 | Embedded UI: every action and upload | Works as before |
| T6.5 | Cross-origin form POST (`<form method=post>`) to `/api/upload` | 403; no temp file |

---

## BF-07 Playing state can stick; volume writes wear NVS

### BF-07a Playing state can stick

**Defect.** `playerLoop()` (`NFCPlayer/player.cpp:158`) detects end of playback through
`audioIsPlaying()`, which returns the wrapper's own `playing` flag. That flag is only cleared
by the EOF event or an explicit stop. If the decoder stops without an EOF event (decode error,
truncated or corrupt file, SD read error), the player stays in `PLAYING`: LED solid, status
shows playing, and the next tap of the same card is treated as "stop".

**Required behavior.**

1. `audioIsPlaying()` returns `playing && audio.isRunning()`.
2. `audioLoop()` clears `playing` and calls the EOF handler when `playing` is set but
   `audio.isRunning()` has been false for ≥ 500 ms after the start of playback (the existing
   500 ms grace in `playerLoop()` moves here).
3. When this path fires without an EOF event, log `[audio] stopped without EOF: <file>` and
   flash the error LED pattern.

**Acceptance.**

| ID | Steps | Expected |
|---|---|---|
| T7.1 | Map a card to a truncated MP3 (first 50 KB of a file) | Plays briefly, returns to idle; LED idle |
| T7.2 | Map a card to a `.mp3` that is random bytes | Error flash; idle within 1 s |
| T7.3 | After T7.1, tap the same card | Plays again (not treated as stop) |
| T7.4 | Normal EOF | Idle, no error flash |

### BF-07b Volume writes wear NVS

**Defect.** Each volume change calls `settingsSetVolume()` → `settingsSave()`
(`NFCPlayer/settings.cpp:86-89`), which rewrites all six NVS keys. Dragging the UI slider
issues many writes, wearing flash and stalling the loop.

**Required behavior.**

1. Volume changes apply to the audio output immediately.
2. The volume is persisted by writing **only** the `volume` key, once the volume has not
   changed for **2 s**. `settingsLoop()` (new, called from `loop()`) performs the deferred
   write.
3. A pending volume write is flushed before any reboot (`netReboot()`).
4. `settingsSave()` writes only keys whose value differs from what is stored
   (`Preferences` `getString`/`getUChar` compare before `put*`).

**Acceptance.**

| ID | Steps | Expected |
|---|---|---|
| T7.5 | Drag slider 0 → 21 → 10 within 2 s | Volume follows; exactly one NVS write (serial log) |
| T7.6 | Change volume, reboot after 1 s via UI | New volume restored after boot |
| T7.7 | Save settings with only the device name changed | Only `dev_name` written |

---

## BF-08 NFC reader is never rechecked after init

### Defect

`nfcReady()` becomes true after the first successful init (`NFCPlayer/nfc_reader.cpp:73`)
and is never re-evaluated. PN532 modules on I2C can hang; `readPassiveTargetID()` then
reports "no card" forever and the player silently stops responding to cards until a reboot.

### Required behavior

1. Count **consecutive I2C/command failures**: a poll where the PN532 does not ACK the
   command or returns a malformed frame counts as a failure; "no card in field" does not.
   This needs a poll wrapper that distinguishes the two (the Adafruit API returns `false` for
   both; use `sendCommandCheckAck` + `readDetectedPassiveTargetID`, or `inListPassiveTarget`
   with explicit ACK checking).
2. After **5 consecutive failures**, mark the reader not ready and reset it.
3. Every **60 s** while no card is present, call `getFirmwareVersion()`. If it returns 0,
   mark the reader not ready and reset it.
4. Reset = drive RSTPDN (GPIO47) LOW ≥ 10 ms, HIGH, wait 100 ms, re-run `begin()`,
   `SAMConfig()`, `setPassiveActivationRetries(1)`. If it fails, the existing 5 s retry
   (`NFC_REINIT_MS`) continues.
5. While not ready, `/api/status` reports `nfc: false` and the LED shows the error pattern
   once per failed recovery attempt (not continuously).
6. Log each reset: `[nfc] reader not responding; resetting (reason: errors|health)`.
7. Recovery must not block the main loop for more than 150 ms per attempt.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T8.1 | While idle, briefly short SDA to GND (via 100 Ω) for 1 s | Reader recovers ≤ 65 s later; cards play |
| T8.2 | Unplug PN532 VCC 5 s, replug | `nfc:false` then `true`; cards play without reboot |
| T8.3 | 1 h idle soak | No spurious resets in the log |
| T8.4 | Playback during a recovery | Audio uninterrupted |

---

## BF-09 AP password is shared and exposed

### Defect

- Every device ships with the same AP password `nfcplayer` (`NFCPlayer/config.h:74`), printed
  in the README and on enclosure labels.
- `GET /api/settings` returns the AP password in plain text (`web_server.cpp:285`).

### Required behavior

1. **Per-device default.** When no `ap_pass` is stored in NVS, the AP password is derived
   from the eFuse MAC:
   - input: the 6-byte eFuse MAC followed by the ASCII string `nfcplayer-ap-v1`;
   - SHA-256 (mbedTLS) of the input;
   - output: the first 10 characters produced by mapping successive digest bytes to the
     alphabet `abcdefghjkmnpqrstuvwxyz23456789` (31 characters; skip bytes ≥ 248 to avoid
     modulo bias);
   - lower case, no ambiguous characters (`0 o 1 l i`), so it is easy to type from a label.
2. The derived password is not stored in NVS; it is recomputed at boot, so it survives NVS
   erase and stays matched to the label.
3. It is printed on serial at every boot: `[net] default AP password: <pw>`.
4. A parent-set password (≥ 8 chars) overrides it as today. Setting `ap_pass` to an empty
   string via the API clears it and returns to the derived default.
5. `GET /api/settings` no longer returns `ap_pass`. It returns `ap_pass_custom: true|false`.
   The UI shows "Default (printed on label)" or "Custom" and a field to set a new one.
6. `DEFAULT_AP_PASS` is removed from `config.h`.

**Accepted limitation.** The firmware is open source and the AP's BSSID is derived from the
MAC, so someone who knows this scheme and the device's MAC can compute the default. The
derived password stops casual access and shared-default attacks only; parents who need more
should set a custom password. State this in SPEC.md §5.2 and §9.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| T9.1 | Two boards, NVS erased | Different AP passwords; each matches its serial log |
| T9.2 | Erase NVS, reboot | Same derived password as before |
| T9.3 | Set custom password; reboot | Custom one in use; `ap_pass_custom: true` |
| T9.4 | Clear custom password | Derived default restored after reboot |
| T9.5 | `GET /api/settings` | No password value in the response |

Doc changes: README.md:105, SPEC.md §2.5, §3.8 (`ap_pass` default "derived"), §6.2,
Appendix A; CONNECTIONS.md:198 (label shows the device's own password).

---

## BF-10 Documentation errors

Fix these in the docs; no firmware change except where noted.

| # | Location | Problem | Fix |
|---|---|---|---|
| D1 | SPEC.md:170 vs `config.h:63` | Debounce stated as 50 ms; firmware uses 30 ms | Set firmware to 50 ms (`BUTTON_DEBOUNCE_MS`) to match the spec; the extra 20 ms is within the 300 ms stop-latency budget |
| D2 | SPEC.md:149, CONNECTIONS.md:114 | "Fit the gain resistor for 6 dB" is wrong for MAX98357A | GAIN floating = 9 dB; GAIN → GND = 12 dB; GAIN → VIN = 6 dB; 100 kΩ GAIN → VIN = 3 dB; 100 kΩ GAIN → GND = 15 dB. Recommend 6 dB (wire to VIN) or 3 dB (100 kΩ to VIN) for a child's player |
| D3 | SPEC.md:532-534 | "Board without PSRAM" stated twice, contradicting on whether the UI works | Keep one sentence: UI and uploads work, audio does not decode |
| D4 | SPEC.md:605, README.md:194 | Refers to `qio_opi`; the FQBN uses `PSRAM=opi` | Say "`PSRAM=opi` (OPI PSRAM) in the FQBN" |
| D5 | BOM.md:68 | "the Pi/ESP32 I2S bus" — leftover from the Pi design | "the ESP32 I2S bus" |
| D6 | SPEC.md:596, :576 | Deep-sleep acceptance rows | Replaced by BF-03/BF-04 rows |
| D7 | SPEC.md §4.3 | Says size and free space are checked at upload start | Replace with BF-01 behavior |

---

## Implementation order

1. **BF-03** boot order (touches `setup()`, which BF-02/04/05 also touch)
2. **BF-02** network recovery (builds on the non-blocking `netBegin()` from BF-03)
3. **BF-04** remove deep sleep, amp shutdown (hardware change: one wire)
4. **BF-06** custom header (UI and server together); **BF-01** upload size (shares
   `collectHeaders()` with BF-06)
5. **BF-05**, **BF-07**, **BF-08**, **BF-09** — independent, any order
6. **BF-10** doc fixes, then fold everything into SPEC.md 2.2

Each item lands with its acceptance rows added to SPEC.md §8.3 and passes `make build` with
no new warnings in project files.

## Hardware change summary

| Change | Item |
|---|---|
| MAX98357A `SD` → GPIO14 (method per BF-04 step 9) | BF-04 |
| Inline USB-C switch becomes required | BF-04 |
| Optional: GAIN strap to VIN (6 dB) or 100 kΩ to VIN (3 dB) | BF-10 D2 |
| Enclosure label: device's own AP password | BF-09 |
