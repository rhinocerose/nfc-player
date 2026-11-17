# NFC Audio Player — Connections and Wiring

**Board:** ESP32-S3 DevKitC-1 N16R8
**Updated:** 2026-11-06
**Pin reference:** the GPIO number printed on the DevKitC-1 header (for example `GPIO4`
is the header pin labeled `4`).

> Power off and unplug the USB-C cable before changing wiring. Check for shorts between
> 3V3, 5V, and GND with a multimeter before applying power.

## 1. Signal connections

| ESP32-S3 signal | GPIO | Header label | Connects to | Notes |
|---|---:|---:|---|---|
| 3V3 | — | `3V3` | PN532 VCC, microSD VCC | 3.3 V only |
| 5V | — | `5V` | MAX98357A VIN | From USB/power bank |
| GND | — | `GND` | All module grounds | Common ground bus |
| I2S BCLK | 4 | `4` | MAX98357A BCLK | Short wire |
| I2S LRC | 5 | `5` | MAX98357A LRC | Short wire |
| I2S DIN | 6 | `6` | MAX98357A DIN | Short wire |
| SD CS | 10 | `10` | microSD CS | Chip select |
| SD MOSI | 11 | `11` | microSD MOSI/DI | Data to card |
| SD SCK | 12 | `12` | microSD SCK/CLK | Clock |
| SD MISO | 13 | `13` | microSD MISO/DO | Data from card |
| NFC SDA | 8 | `8` | PN532 SDA | 3.3 V I2C |
| NFC SCL | 9 | `9` | PN532 SCL | 3.3 V I2C |
| NFC RST | 47 | `47` | PN532 RSTPDN | Active low; driven by firmware |
| NFC IRQ | 21 | `21` | PN532 IRQ | Optional; polling does not require it |
| Button 1 | 1 | `1` | Button 1 → GND | Internal pull-up |
| Button 2 | 2 | `2` | Button 2 → GND | Reserved |
| Status LED | 7 | `7` | 330 Ω → LED anode | LED cathode → GND |

### Reserved / do not use

| GPIO | Why |
|---|---|
| 0, 3, 45, 46 | ESP32-S3 strapping pins |
| 19, 20 | Native USB D−/D+ |
| 26–37 | SPI flash and octal PSRAM |
| 43, 44 | UART0 |
| 48 | Onboard RGB LED on many dev boards |

The firmware pin map is in [`NFCPlayer/config.h`](NFCPlayer/config.h).

## 2. Power connections

| Source | Destination | Notes |
|---|---|---|
| USB-C power bank or computer | DevKitC-1 USB-C port | 5 V/2 A recommended |
| DevKitC-1 `5V` | MAX98357A `VIN` | Do not feed 5 V into `3V3` |
| DevKitC-1 `3V3` | PN532 `VCC` | PN532 draws roughly 100 mA |
| DevKitC-1 `3V3` | microSD adapter `VCC` | Use a 3.3 V adapter |
| DevKitC-1 `GND` | All grounds | Star ground at the board |

- Do **not** power the MAX98357A from `3V3`; it needs 5 V for its rated output.
- Do **not** connect an external 5 V supply to the `5V` pin while USB is plugged in unless
  the board is designed for power sharing.
- The DevKitC-1 has two USB-C ports. Use the **USB** (native) port for flashing and serial
  when using the default `CDCOnBoot=cdc` build. The other port is typically the UART bridge.
  `make board` shows which port appears (`/dev/ttyACM0` for native USB, `/dev/ttyUSB0` for
  UART).
- Add an inline USB-C switch if you want a physical off switch. Switch the 5 V line; do not
  switch the ground.

## 3. Module-specific wiring

### PN532 (I2C)

| PN532 pin | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |
| RSTPDN | GPIO47 |
| IRQ | GPIO21 (optional) |

- Set the board's SEL0/SEL1 jumpers to **I2C mode**. The firmware uses address `0x24`.
- The PN532 is 3.3 V logic. If you use a 5 V-only module, add level shifting.
- Keep the antenna behind 2–3 mm of plastic, away from metal and the ESP32 antenna.
- IRQ is not required; the firmware polls. Leave it unconnected if you prefer.

### microSD (SPI)

| microSD adapter pin | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | GPIO10 |
| MOSI / DI | GPIO11 |
| SCK / CLK | GPIO12 |
| MISO / DO | GPIO13 |

- Use a **3.3 V** adapter. If the adapter has a 5 V regulator/level shifter, power it from
  5 V and verify its logic outputs are 3.3 V.
- Keep the SPI wires short. A 100 µF capacitor across the card's 3.3 V/GND helps with
  marginal cards.
- Format the card as FAT32. The firmware creates `/audio` and `/config`.

### MAX98357A (I2S)

| MAX98357A pin | ESP32-S3 |
|---|---|
| VIN | 5V |
| GND | GND |
| BCLK | GPIO4 |
| LRC | GPIO5 |
| DIN | GPIO6 |
| Speaker + | Speaker + |
| Speaker − | Speaker − |

- The outputs are bridge-tied. **Never** connect either speaker terminal to ground.
- Fit a 470 µF electrolytic and a 100 nF ceramic across VIN/GND near the amp.
- Leave `SD` enabled and `GAIN` at its default (9 dB). Fit the gain resistor for 6 dB if it
  is too loud at low volume.
- Keep I2S wires short and away from the speaker leads.

### Buttons

| Button | ESP32-S3 | Other side |
|---|---|---|
| Button 1 (stop) | GPIO1 | GND |
| Button 2 (reserved) | GPIO2 | GND |

- Internal pull-ups are enabled; the button shorts the GPIO to GND.
- Optional 100 nF from each GPIO to GND for noise.
- Button 1 short press stops playback; hold 3 s stops and enters deep sleep.

### Status LED

```text
GPIO7 ── 330 Ω ── LED anode ── LED cathode ── GND
```

Use a diffused 3 mm LED and a light pipe in the enclosure.

## 4. Power and ground diagram

```text
USB-C 5 V ──────────────┬───────────────┬───────────────┐
                        │               │               │
                    DevKitC-1 5V     DevKitC-1 3V3    (do not mix)
                        │               │
                        │               ├── PN532 VCC
                        │               └── microSD VCC
                        │
                    MAX98357A VIN

GND ─────────────── common ground bus ── all modules
```

## 5. Connector list

| Connector | Pins | Use |
|---|---|---|
| J1 | 1×6 Dupont | MAX98357A: 5V, GND, BCLK, LRC, DIN (one spare) |
| J2 | 1×6 Dupont | microSD: 3V3, GND, CS, MOSI, SCK, MISO |
| J3 | 1×6 Dupont | PN532: 3V3, GND, SDA, SCL, RST, IRQ |
| J4 | 1×2 | Speaker +/− (JST or screw terminal) |
| J5 | 1×2 | Button 1 → GND |
| J6 | 1×2 | Button 2 → GND |
| J7 | 1×2 | LED + resistor |

Use 2.54 mm headers on the DevKitC-1 and female Dupont connectors on the modules. For a
permanent build, solder the wiring and strain-relieve the cables.

## 6. Assembly checklist

1. Power off and disconnect USB.
2. Solder headers to the DevKitC-1 and adapter boards.
3. Wire the common ground bus first.
4. Wire 3.3 V to the PN532 and microSD; wire 5 V to the amp only.
5. Wire I2S (GPIO4/5/6), SD (GPIO10–13), and NFC (GPIO8/9/47/21).
6. Wire the buttons to GND and the LED through 330 Ω.
7. Check continuity and look for 3V3–GND / 5V–GND shorts with a multimeter.
8. Insert a FAT32 microSD card.
9. Connect USB-C and run `make monitor PORT=...`; expect the boot log with PSRAM, SD,
   PN532, and the web server address.
10. Open the web UI, upload a file, and test a card.

## 7. Board and port notes

- The build targets the **N16R8** (16 MB flash, 8 MB octal PSRAM). The FQBN is in
  [`scripts/build.sh`](scripts/build.sh):
  `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc`.
- The fitted core is ESP32 Arduino **3.3.12**; the audio library is `ESP32-audioI2S` **3.4.4**.
- If you use the UART USB-C port instead of native USB, rebuild with `CDCOnBoot=default`
  (or change the port/FQBN in the scripts) so `Serial` goes to the UART bridge.
- `make board` lists the detected serial devices; use that port for `make flash` and
  `make monitor`.

## 8. Enclosure notes

- 2–3 mm plastic over the PN532 coil; no metal, foil, or conductive paint within 10 mm.
- Speaker grille with enough open area for 3 W; avoid rattling.
- Vent slots above the ESP32-S3; the board warms during playback and Wi-Fi.
- Cutouts for USB-C, buttons, and a light pipe for the LED.
- Label the AP SSID (`NFCPlayer-XXXX`), default AP password (`nfcplayer`), and
  `http://192.168.4.1`.
- Keep the power bank external or in a vented pocket; do not seal a Li-ion pack next to the
  amp.
