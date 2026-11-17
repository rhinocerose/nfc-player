# NFC Audio Player — Toddler UX Specification

**Status:** Approved for implementation; not yet implemented
**Applies to:** firmware 1.0.0 (SPEC.md 2.1), after [BUGFIXES.md](BUGFIXES.md)
**Target release:** firmware 1.2.0
**Last updated:** 2026-11-17

**Related documents:** [SPEC.md](SPEC.md) · [BUGFIXES.md](BUGFIXES.md) · [BOM.md](BOM.md) ·
[CONNECTIONS.md](CONNECTIONS.md)

This document specifies how the player behaves for its real user: a toddler who places,
lifts, drops and chews cards, holds buttons down, and cannot read LED patterns. It also
covers the parent-side web UI changes those behaviors need.

Implement BUGFIXES.md first. Where this document changes a bug-fix decision, it says so
under [Changes to BUGFIXES.md](#changes-to-bugfixesmd).

---

## Contents

| ID | Title |
|---|---|
| [UX-01](#ux-01-presence-play-model) | Presence play model |
| [UX-02](#ux-02-short-resume) | Short resume |
| [UX-03](#ux-03-cards-play-a-file-or-a-folder) | Cards play a file or a folder |
| [UX-04](#ux-04-end-of-content-and-per-card-repeat) | End of content and per-card repeat |
| [UX-05](#ux-05-buttons-pause-and-next) | Buttons: Pause and Next |
| [UX-06](#ux-06-volume-knob-with-parent-limits) | Volume knob with parent limits |
| [UX-07](#ux-07-per-card-volume-offset) | Per-card volume offset |
| [UX-08](#ux-08-feedback-sounds) | Feedback sounds |
| [UX-09](#ux-09-bedtime-sleep-timer-night-window-fade-out) | Bedtime: sleep timer, night window, fade-out |
| [UX-10](#ux-10-clock) | Clock |
| [UX-11](#ux-11-card-setup-from-the-last-unknown-card) | Card setup from the last unknown card |
| [UX-12](#ux-12-physical-and-safety-requirements) | Physical and safety requirements |
| [Data model](#data-model-changes) | `cards.json`, NVS and API changes |
| [Hardware](#hardware-changes) | Pin, wiring and BOM changes |

---

## UX-01 Presence play model

Replaces tap-to-toggle (SPEC.md §3.4).

### Behavior

| Situation | Result |
|---|---|
| Known card placed, nothing playing | Card-recognized chime, then the card's content plays (or resumes per UX-02) |
| Card stays on the reader | Keeps playing |
| Card removed | Playback pauses immediately |
| Same card placed again | Resumes per UX-02 |
| Different known card placed | Previous card's pause state is dropped; chime; new card plays from the start |
| Unknown card placed | Unknown-card sound (UX-08); nothing else changes; UID recorded for UX-11 |
| Known card whose file/folder is missing | Unknown-card sound; logged |
| Card placed during quiet hours without the bedtime flag | Quiet-hours sound (UX-09); nothing plays |
| Web UI "Play" on a file with no card on the reader | Plays as today; placing a card takes over; removing no card does nothing |

### Detection

1. **Placed:** first successful read of a UID that was not present.
2. **Removed:** no read of the present UID for **800 ms** (`NFC_ABSENT_MS`, reduced from
   1500 ms). A single missed poll must not pause playback.
3. **Bounce guard:** a removal followed by the same UID within 800 ms is not an event (it
   never became "removed").
4. The 2 s cooldown (`NFC_COOLDOWN_MS`) is removed; presence replaces it.
5. Pause latency (card lifted → silence) ≤ 1 s. Place-to-audio ≤ 1 s, chime included.

### Pausing

Pause uses the audio library's pause (`pauseResume()`), not stop, so the decoder position is
kept. The amp (BF-04) is shut down 20 ms after a pause and enabled before resume.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| U1.1 | Place known card | Chime, audio ≤ 1 s |
| U1.2 | Lift card | Silence ≤ 1 s |
| U1.3 | Replace within 5 min | Resumes from the pause point |
| U1.4 | Slide card around on the reader for 10 s | No pause/resume flicker |
| U1.5 | Card A playing, swap to card B | B starts from its beginning; A's position forgotten |
| U1.6 | Unknown card | Unknown sound; current state unchanged |

---

## UX-02 Short resume

1. Only the **most recently paused card** keeps a resume point: its UID, track index
   (for folders), and position in seconds.
2. The resume point is kept for **5 minutes** after the pause. After that, or if a
   different card is placed, it is dropped and the next placement starts from the
   beginning.
3. Resume points live in RAM only. Nothing is written to SD; a reboot or power-off drops
   them.
4. While paused with the card still away, the player is otherwise idle (LED idle pattern).
5. Resuming after a pause longer than 10 s rewinds **3 s** so the child hears a little
   context. A shorter pause resumes exactly.
6. If the sleep timer (UX-09) expired during the pause, the card starts as a new placement
   and the timer is not re-armed.

---

## UX-03 Cards play a file or a folder

1. A card maps to either:
   - a **file** in `/audio` (as now), or
   - a **folder** `/audio/<folder>/`, played in case-insensitive name order, auto-advancing.
2. Folders are one level deep. Folder names follow the filename policy (SPEC.md §4.4)
   without an extension: ASCII letters, digits, space, `_`, `-`; ≤ 48 characters.
3. Only files with allowed audio extensions are played; others in the folder are skipped.
4. An empty folder, or a folder whose files all fail to play, counts as missing content
   (unknown-card sound).
5. A track that fails to decode is skipped after logging; playback moves to the next track.
6. **Web UI, Files page:** create folder, rename folder, delete folder (with its files and
   its card mappings, after confirmation), upload into a chosen folder or into the root,
   move a file between root and folders.
7. **Web UI, Cards page:** the target picker lists folders (marked as folders, with track
   count) and files.
8. Deleting a file inside a mapped folder does not remove the mapping; deleting the folder
   does.
9. Upload limits (64 MB per file, BF-01 checks) apply per file.

API additions are listed under [Data model changes](#data-model-changes).

---

## UX-04 End of content and per-card repeat

1. Each card has a **repeat** flag, off by default, set on the Cards page.
2. **Repeat off:** after the last track (or the single file) ends, playback stops and the
   player stays silent while the card remains on the reader. Removing and replacing the card
   plays from the beginning. No resume point is kept.
3. **Repeat on:** after the last track, playback starts again from the first track (or the
   file's beginning), indefinitely while the card is on the reader.
4. A sleep timer (UX-09) still stops a repeating card.

---

## UX-05 Buttons: Pause and Next

Supersedes BF-04 step 2 (Stop button behavior).

| Button | GPIO | Press | Hold |
|---|---|---|---|
| Button 1 — **Pause** | 1 | Playing → pause; paused with card on reader → resume | Nothing extra |
| Button 2 — **Next** | 2 | Folder: next track (from the last track: first track if repeat is on, otherwise stop as at end of content). Single file: no action | Nothing extra |

1. Each press acts once, on press after 50 ms debounce. Holding never triggers anything else
   and never auto-repeats.
2. If the card is lifted while button-paused, the pause becomes a normal card-removed pause
   (UX-02); replacing the card resumes playback, because the child expects the card to
   start it.
3. Next while paused moves to the next track and starts playing.
4. With no card and nothing playing, both buttons do nothing (no sound).
5. Web UI play (no card): Pause pauses/resumes; Next does nothing.

---

## UX-06 Volume knob with parent limits

### Hardware

- 10 kΩ linear potentiometer, ends to 3V3 and GND, wiper to **GPIO7** (ADC1_CH6), 100 nF
  from wiper to GND. The status LED moves from GPIO7 to **GPIO15**; see
  [Hardware changes](#hardware-changes).

### Behavior

1. The parent sets **minimum** and **maximum** volume (0–21) on the Settings page.
   Defaults: min 4, max 12. Max ≥ min + 2 is enforced.
2. The knob's full travel maps linearly onto **min..max**. The knob's ends always reach the
   limits, so a child can never exceed the max.
3. Read the ADC every 50 ms, average the last 8 readings, and change the volume only when
   the mapped value moves by a whole step with ±0.4-step hysteresis, so a resting knob never
   jitters.
4. The knob is the only live volume control. The web UI's volume slider is replaced by the
   min/max limits plus a read-only "current volume" indicator.
5. Effective volume = clamp(knob volume + card offset (UX-07), min, active max), where
   active max is the night cap during the night window (UX-09) and the parent max otherwise.
6. At boot the knob position is read before audio starts; there is no stored volume.
7. When the knob reaches either end of its travel, play the volume-limit tick (UX-08) once;
   it re-arms after the knob moves 2 steps away from that end.

---

## UX-07 Per-card volume offset

1. Each card has an **offset** of −6..+6 volume steps, default 0, set on the Cards page with
   a "test" button that plays the card's first track at the resulting level.
2. Applied as in UX-06 step 5; the result is still clamped to min..active max.
3. Feedback sounds ignore the offset.

---

## UX-08 Feedback sounds

### Events

| Event | Sound | Length | When |
|---|---|---|---|
| `start` | Short rising jingle | ≤ 1.5 s | Player ready for cards after boot (BF-03 ≤ 3 s target includes it) |
| `card` | Bright chime | ≤ 400 ms | Known card placed, before its content |
| `unknown` | Soft low "boop" (friendly, not a buzzer) | ≤ 500 ms | Unknown card, or missing content |
| `limit` | Single soft tick | ≤ 100 ms | Knob reaches either end (UX-06 step 7) |
| `quiet` | Gentle "shh"/descending tone | ≤ 1 s | Non-bedtime card during quiet hours (UX-09) |

### Rules

1. **Built-in defaults:** each sound is compiled into the firmware (short mono MP3 or WAV,
   ≤ 20 KB each). At boot, if missing or from an older firmware version, they are written to
   `/sounds/` on the internal FFat partition (the 9 MB partition in `app3M_fat9M_16MB`) and
   played from there with `connecttoFS(FFat, ...)`.
2. **Parent overrides:** a file `/sounds/<event>.<ext>` on the SD card (any allowed audio
   extension) replaces the built-in sound for that event. Max 200 KB, max 3 s; longer files
   are cut at 3 s.
3. **Web UI, Settings → Sounds:** per event, play the current sound, upload a replacement,
   or revert to the built-in.
4. Sounds work with no SD card (built-ins from FFat).
5. Sounds play at the current effective volume without card offset, but never below
   volume 4, so feedback is audible even with the knob at its minimum; still clamped to the
   active max.
6. A sound interrupts nothing important: `card` plays before content; `unknown`, `quiet`
   and `limit` play only while nothing is playing or briefly duck (pause) current audio and
   then resume it.
7. A parent setting "Feedback sounds: on/off" (default on) disables all except `start`.

---

## UX-09 Bedtime: sleep timer, night window, fade-out

### Sleep timer

1. Play page offers **15 / 30 / 60 min** and **Cancel**. The remaining time is shown.
2. When it expires, playback fades out over 10 s (see fade rules) and stops, even with the
   card on the reader. The card must be removed and replaced to play again.
3. The timer is one-shot: cleared on expiry, cancel, or reboot.
4. LED shows a slow "timer active" breathing pattern while armed (parent-facing).

### Night window

5. Settings: **night start** and **night end** times (default 19:00–07:00), and two
   options:
   - **Night volume cap** (0–21, default 8, must be ≥ min): replaces the parent max inside
     the window.
   - **Quiet hours** (on/off, default off): inside the window, only cards with the
     **bedtime** flag play; other known cards play the `quiet` sound.
6. Each card has a **bedtime** flag (default off), set on the Cards page.
7. Entering the window while a card plays above the night cap: volume ramps down to the
   cap over 5 s. Entering quiet hours while a non-bedtime card plays: it fades out and
   stops.
8. Without a valid clock (UX-10), the night window is inactive and the Settings page says
   so.

### Per-card fade-out

9. Each card has a **fade** flag (default off). With fade on and repeat off, the last 30 s
   of the card's content (last track of a folder, or the single file) fade linearly to
   silence. If the content is shorter than 60 s, fade over its last half.
10. With repeat on, the fade flag has no effect.

### Fade rules

11. Fades step the library volume down one step at a time, evenly spaced over the fade
    duration, then pause/stop, then restore the effective volume for the next playback.
12. A fade in progress is cancelled by removing the card (normal pause) or by pressing
    Pause.

---

## UX-10 Clock

1. **NTP** (`configTzTime`) when the station is connected to the home network.
2. **Browser time:** when the web UI loads, it POSTs the phone's current epoch time to
   `/api/time` (with the BF-06 header). Accepted only if no NTP sync happened in the last
   24 h.
3. **Time zone:** Settings page, POSIX TZ string chosen from a short list plus "custom";
   default `UTC0`. Stored in NVS.
4. The clock is lost at power-off (no RTC); the status API reports `clock: "ntp" |
   "browser" | "none"`.
5. Time-based rules (UX-09 night window) run only when `clock != "none"`.

---

## UX-11 Card setup from the last unknown card

1. The firmware keeps the **most recent unknown UID** placed on the reader, with the time it
   was seen, in RAM.
2. The Cards page shows it ("New card seen 12 s ago: 04A1B2C3D4E5F6") with an **Assign**
   button that opens the assignment form pre-filled with that UID.
3. It is cleared once assigned, or after 10 minutes.
4. Learn mode stays as an alternative.
5. API: `GET /api/learn` gains `last_unknown: {uid, age_s}` (null when none).

---

## UX-12 Physical and safety requirements

### Card recess

1. A recess in the lid over the PN532 antenna, sized for the chosen card plus 2 mm
   clearance on each side and 2–3 mm deep, so a placed card lies flat and centred.
2. Lid thickness under the recess 2 mm; no metal within 10 mm.
3. A finger notch on one side so a toddler can lift the card out.
4. The card must read anywhere it can sit in the recess (verified in U12.1).

### Controls

5. Pause and Next: **24–30 mm arcade-style buttons**, distinct colours, with a simple icon
   moulded or printed on the cap (pause bars, arrow).
6. Volume knob: large cap (≥ 30 mm diameter), knurled, firmly pressed or set-screwed onto
   the shaft; end stops at both ends.

### Cards

7. NFC tags are sealed **inside** cards or blocks: laminated cards ≥ 54 × 85 mm (credit-card
   size) and ≥ 0.8 mm thick, or wooden/printed blocks. No exposed stickers.
8. Every card and block is larger than a choking-test cylinder (≥ 45 mm in every
   direction).
9. Cards carry a picture of their content so the child can choose without reading.

### Enclosure

10. Closed with screws (not snap fits); no openings larger than 5 mm except the speaker
    grille.
11. Speaker grille: holes ≤ 3 mm, or a slot pattern backed by mesh, so a finger or pencil
    can't reach the cone.
12. The USB-C cable has internal strain relief; the inline power switch (BF-04) is reachable
    by a parent but recessed or on the back.
13. No sharp edges; rounded corners ≥ 3 mm radius.
14. Survives a 1 m drop onto a hard floor with no opening of the case.

### Acceptance

| ID | Steps | Expected |
|---|---|---|
| U12.1 | Place a card at every extreme position in the recess | Reads every time |
| U12.2 | Try to poke a 3 mm rod through the grille | Cannot touch the speaker cone |
| U12.3 | 1 m drop ×5 on each face | Case stays closed; device works |
| U12.4 | Pull the USB cable with 20 N | Nothing inside moves |

---

## Data model changes

### `cards.json`

```json
{
  "cards": [
    {
      "uid": "04A1B2C3D4E5F6",
      "type": "folder",
      "target": "Bedtime songs",
      "label": "Bedtime",
      "repeat": false,
      "offset": -2,
      "fade": true,
      "bedtime": true
    },
    {
      "uid": "04A1B2C3D4E5F7",
      "type": "file",
      "target": "twinkle.mp3",
      "label": "Twinkle",
      "repeat": true,
      "offset": 0,
      "fade": false,
      "bedtime": false
    }
  ]
}
```

- `type`: `"file"` (target is a filename in `/audio`) or `"folder"` (target is a folder
  name under `/audio`).
- Missing optional fields take defaults: `repeat=false`, `offset=0`, `fade=false`,
  `bedtime=false`.
- **Migration:** a 1.x entry with `file` and no `type` loads as `type: "file"`,
  `target: <file>`; the next save writes the new format.

### NVS keys (namespace `nfcplayer`)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `vol_min` | uint8 | 4 | Knob minimum |
| `vol_max` | uint8 | 12 | Knob maximum |
| `night_on` | bool | false | Night volume cap enabled |
| `night_vol` | uint8 | 8 | Night volume cap |
| `quiet_on` | bool | false | Quiet hours enabled |
| `night_start` | uint16 | 1140 | Minutes after midnight (19:00) |
| `night_end` | uint16 | 420 | Minutes after midnight (07:00) |
| `tz` | string | `UTC0` | POSIX TZ |
| `sounds_on` | bool | true | Feedback sounds |

The `volume` key is removed (the knob is the volume; see Changes to BUGFIXES.md).

### API

| Method | Path | Request | Response |
|---|---|---|---|
| GET | `/api/files` | `?folder=` (optional) | adds `folders[{name,count}]` at root |
| POST | `/api/upload` | adds `?folder=` | unchanged |
| POST | `/api/folders` | `?name=` | `{ok,name}` |
| POST | `/api/folders/rename` | `?from=&to=` | `{ok}` (updates card targets) |
| POST | `/api/folders/delete` | `?name=` | `{ok}` (removes files and mappings) |
| POST | `/api/move` | `?name=&from=&to=` | `{ok}` |
| POST | `/api/cards` | `?uid=&type=&target=&label=&repeat=&offset=&fade=&bedtime=` | `{ok,uid}` |
| POST | `/api/pause` | — | `{ok,paused}` |
| POST | `/api/timer` | `?minutes=0\|15\|30\|60` (0 cancels) | `{ok,remaining_s}` |
| POST | `/api/time` | `?epoch=` | `{ok,clock}` |
| GET | `/api/sounds` | — | `sounds[{event,source:"builtin"\|"sd"}]` |
| POST | `/api/sounds/upload` | `?event=`, multipart `file` | `{ok}` |
| POST | `/api/sounds/revert` | `?event=` | `{ok}` |
| GET | `/api/status` | — | adds `paused`, `track`, `track_count`, `timer_s`, `clock`, `night`, `quiet`, `volume_effective` |

`/api/volume` is removed. `/api/settings` gains the new NVS fields and drops `volume`.
All POSTs require the BF-06 header.

---

## Hardware changes

| Change | Detail |
|---|---|
| Volume knob | 10 kΩ linear pot, wiper → **GPIO7** (ADC1_CH6), ends → 3V3/GND, 100 nF wiper → GND |
| Status LED | Moves **GPIO7 → GPIO15** (330 Ω → LED → GND) |
| Buttons | 24–30 mm arcade buttons on GPIO1 (Pause) and GPIO2 (Next), replacing 6 mm tactiles |
| Amp shutdown | GPIO14, as in BF-04 |
| Lid | Card recess over the PN532 (UX-12) |
| Cards | Sealed laminated cards or blocks replace loose stickers |

Why GPIO7: the knob needs an ADC1 pin (ADC2 is unusable while Wi-Fi is on). ADC1 is
GPIO1–10; all are in use except GPIO3, which is a strapping pin. GPIO7 is freed by moving
the LED to GPIO15, which has no ADC requirement.

BOM additions: potentiometer + large knob (~$3), two arcade buttons (~$4, replacing $1 of
tactiles), card lamination pouches or blank PVC NFC cards (~$10 for 20, replacing the
stickers).

---

## Changes to BUGFIXES.md

| BUGFIXES item | Change |
|---|---|
| BF-04 step 2 | Button 1 becomes Pause (UX-05), not Stop |
| BF-04 acceptance T4.1 | "Hold Pause 10 s while playing: pauses; nothing else happens" |
| BF-07b | Applies only until 1.2.0. With the knob there is no stored volume; the deferred-write mechanism is reused for the new NVS keys (write only changed keys) |
| BF-08 step 5 | Error LED pattern unchanged; no sound for reader recovery |
| BF-01 | Size checks apply to uploads into folders and to sound uploads (200 KB limit) |

---

## Implementation order

1. UX-01 presence model + UX-02 resume (core behavior change; replaces the state machine)
2. UX-05 buttons
3. Hardware: LED move, knob; then UX-06 knob + limits, UX-07 offsets
4. UX-03 folders and UX-04 repeat (data model migration lands here)
5. UX-08 sounds (needs FFat setup)
6. UX-10 clock, then UX-09 bedtime features
7. UX-11 last unknown card
8. UX-12 enclosure and cards (can proceed in parallel with firmware)
9. Fold everything into SPEC.md 3.0 (the play model changes, so a major doc version)

Each item adds its acceptance rows to SPEC.md §8.3 and passes `make build` with no new
warnings in project files.
