# SondeDeck

SondeDeck is a handheld RS41 radiosonde recovery receiver for the LilyGO T-Deck Plus.

It receives RS41 frames through the onboard SX1262 radio, decodes telemetry, shows recovery and navigation data on the built-in display, and can optionally log decoded sonde history to an SD card.

Current release: **v1.0**

> SondeDeck can query SondeHub predictions in read-only mode, but it is not affiliated with or endorsed by SondeHub.

## Hardware

Target hardware:

- LilyGO T-Deck Plus
- ESP32-S3
- SX1262 radio
- ST7789 TFT display
- T-Deck keyboard
- Trackball centre press
- Onboard GPS
- Optional microSD card

## Main features

- Vaisala RS41 receive and decode
- Fixed-frequency receive
- RS41 frequency preset stepping
- Basic RS41 preset scanning
- Local GPS position
- Range, bearing, and elevation from local GPS to sonde
- Recovery-focused display pages
- Last-seen / signal-lost handling
- SD card logging and per-sonde history
- Optional Wi-Fi
- Read-only SondeHub prediction lookup by serial
- Battery percentage display
- Screen brightness control
- Keyboard brightness control
- Splash, Help, and About pages

## Supported sondes

Currently supported:

- Vaisala RS41 / RS41-SG style frames

Not currently supported:

- DFM
- M10
- LMS6
- iMet
- MRZ
- Other sonde families

## Controls

```text
Q       Overview
W       Sonde details
E       Recovery
R       Local GPS
T       Logging
Y       Power
F       Frequency
O       Online

Space   Help -> About -> close
Ball    Help -> About -> close
U/I     Previous/next page
Z/X     Previous/next frequency preset
S       Scan on/off
L       Start/stop SD logging
A       Reset counters and peak RSSI
B       Cycle screen brightness
K       Cycle keyboard brightness
P       Toggle auto-dim
```

## Frequency management

Default frequency presets are configured in:

```text
src/config/frequency_config.h
```

The scanner is a simple RS41 preset stepper. It is not a wideband SDR scanner or waterfall display.

Scan behaviour:

```text
1. Listen on the current preset.
2. Wait briefly for an RS41 frame.
3. If no valid frame is decoded, step to the next preset.
4. If a valid RS41 frame is decoded, stop scanning and lock to that frequency.
```

## SD card logging

If an SD card is present, SondeDeck logs automatically.

Files created:

```text
/logs/index.csv
/logs/latest.csv
/logs/last_seen.txt
/logs/sondes/<SERIAL>/track.csv
/logs/sondes/<SERIAL>/summary.txt
```

If no SD card is present, the receiver continues normally and logging stays off.

## Wi-Fi and SondeHub prediction

Wi-Fi is optional.

Configure Wi-Fi in:

```text
src/config/wifi_config.h
```

Leave the SSID blank to keep Wi-Fi disabled.

SondeDeck uses SondeHub predictions in read-only mode:

```text
https://api.v2.sondehub.org/predictions?vehicles=<SERIAL>
```

The handheld does **not** upload:

```text
telemetry
listener/station position
chase-car position
recovery reports
```

Prediction only works when SondeHub already knows about the sonde, usually from a fixed receiver, home base station, or other local uploaders.

## Version and branding

Edit:

```text
src/config/version.h
```

This controls the version and splash/about text.

For the v1.0 release, use:

```cpp
constexpr const char* VERSION = "v1.0";
```

## Build

Install PlatformIO, then run:

```powershell
pio run
```

Upload:

```powershell
pio run -t upload
```

Serial monitor:

```powershell
pio device monitor -b 115200
```

Clean build:

```powershell
pio run -t clean
pio run
```

## Project layout

```text
boards/      PlatformIO board definition
lib/         Vendored libraries
src/         SondeDeck firmware source
```

Main source folders:

```text
src/config
src/decoder
src/gps
src/input
src/models
src/navigation
src/network
src/power
src/prediction
src/radio
src/semtech
src/storage
src/ui
```

## Important configuration files

```text
src/config/version.h         App name, splash name, version, author
src/config/wifi_config.h     Wi-Fi and SondeHub prediction settings
src/config/frequency_config.h RS41 frequency presets and scan timing
src/board_pins.h             T-Deck Plus pin mapping
```

## Known limitations

- RS41 only.
- Scanner is preset/channel stepping, not SDR waterfall scanning.
- Prediction depends on SondeHub already having the sonde.
- Prediction is read-only and requires Wi-Fi.
- No offline wind-model prediction.
- No SondeHub upload from the handheld.
- GPS may not fix indoors.
- Battery USB/charging indication may be inferred from voltage depending on hardware support.

## Safety and legal notes

This firmware is intended for hobby radiosonde recovery and local receive/decode use.

Before using it, check local rules for radio reception, land access, recovery, and any applicable aviation or radio regulations. Do not trespass, interfere with radiosonde operation, or rely on this device for safety-critical navigation.

## GitHub release checklist

Before committing or making the repository public:

- keep `src/config/wifi_config.h` blank of real credentials;
- make sure `.pio/` is ignored and not tracked;
- make sure `.vscode/` is ignored or intentionally excluded;
- do not commit firmware binaries;
- do not commit SD card logs;
- do not commit generated tree files;
- confirm the project builds from a clean checkout.

Suggested release commands:

```powershell
git status
git add .
git status
git commit -m "Release SondeDeck v1.0"
git tag v1.0
git push
git push --tags
```

## Licence

SondeDeck is released under **GPL-3.0-or-later**.

See:

```text
LICENSE
THIRD_PARTY.md
```
