# SondeDeck

**SondeDeck** is a handheld RS41 radiosonde recovery receiver firmware for the **LilyGO T-Deck Plus**.

It uses the onboard **SX1262** radio to receive RS41 radiosonde frames, decodes telemetry on-device, displays recovery/navigation information on the TFT, supports optional SD logging, and can provide a read-only phone dashboard over Wi-Fi.

> SondeDeck can query SondeHub predictions in read-only mode, but it is not affiliated with or endorsed by SondeHub.

## Current release

**Version:** v1.2

v1.2 adds read-only Web Mode, a phone-friendly live dashboard, saved Wi-Fi configuration from the device, a Google Maps sonde-position link, and a cleaner Web/Home/Info control bar.

## Supported hardware

SondeDeck currently targets:

- LilyGO T-Deck Plus
- ESP32-S3
- SX1262 radio
- ST7789 TFT display
- GT911 touchscreen
- Onboard GPS
- Optional microSD card

The firmware is currently developed and tested against the T-Deck Plus hardware layout.

## Supported radiosondes

Currently supported:

- Vaisala RS41 / RS41-SG style frames

Not currently supported:

- DFM
- M10
- LMS6
- iMet
- MRZ
- Other sonde families

## Main features

- RS41 receive and decode using the onboard SX1262 radio
- Recovery-focused TFT interface
- Touchscreen Home screen
- Keyboard and trackball controls retained
- Local GPS support
- Sonde GPS decoding
- Range, bearing, and elevation guidance from local GPS to sonde
- Last-seen and signal-lost handling
- SD card logging
- Frequency preset stepping
- Basic RS41 preset scanning
- Optional Wi-Fi
- Saved Wi-Fi configuration from the device UI
- Read-only Web Mode
- Phone-friendly live dashboard
- Google Maps link for sonde GPS position
- Read-only SondeHub prediction lookup by serial
- Battery percentage display
- Screen brightness control
- Keyboard brightness control
- Touch input enable/disable setting
- Splash, Help, status-icon Help, and About pages

## v1.2 highlights

v1.2 introduces a dedicated Web Mode for remote viewing.

This is intended for situations where the T-Deck is not easily accessible, for example if it is lifted higher on a drone and you want to check from your phone whether it is receiving a sonde.

Web Mode is deliberately **read-only**:

- no scan control
- no logging control
- no frequency control
- no settings control
- no reboot/control endpoints
- no SondeDeck control from the webpage

The receiver continues running while Web Mode is active:

- radio capture continues
- RS41 decoding continues
- local GPS continues
- SD logging continues if enabled
- SondeHub prediction lookup can continue if Wi-Fi is connected

Normal TFT pages are not drawn during Web Mode. The screen shows a static Web Mode splash/status screen and only **Space** exits Web Mode.

## Home screen

After the boot splash, SondeDeck opens to a Home screen with a 4x2 grid for the eight main pages:

- Overview
- Sonde
- Recovery
- Local GPS
- Logging
- Settings
- Frequency
- Online

Each Home tile has a small status dot where relevant.

### Home tile status dots

Overview:

- Green: Sonde, Recovery, Local GPS, and Frequency are all good
- Amber: one or more of those states is not ready

Sonde:

- Red: no sonde frame yet
- Amber: sonde frame seen, but no usable sonde GPS
- Green: sonde frames with usable GPS

Recovery:

- Green: local GPS and sonde GPS are both available
- Amber: one of local GPS or sonde GPS is missing
- Red: neither local GPS nor sonde GPS is available

Local GPS:

- Green: local GPS fix
- Red: no local GPS fix

Logging:

- Green: logging enabled
- Amber: SD available, logging disabled
- Red: no SD or SD error

Settings:

- No status dot

Frequency:

- Green: valid sonde/frequency lock
- Amber: scanning
- Red: no sonde on current frequency and scanning off

Online:

- Green: online/connected
- Amber: configured but not connected
- Red: not configured

## Bottom control bar

The bottom bar is now a control bar:

```text
[Web]                 [Home]                 [Info]
```

Controls:

- **Web** starts read-only Web Mode
- **Home** returns to the Home screen
- **Info** cycles Help -> Icons -> About

The old footer counter/status line was removed because those values are available on the relevant pages.

## Top status bar

The global top bar is shown across the normal TFT UI, including the Home screen.

Status fields:

```text
G12  local GPS fix with satellite count
G..  GPS data present, no fix yet
G--  no local GPS data/fix

S+   sonde frame with usable GPS
S?   sonde frame seen, no usable sonde GPS
S--  no sonde frame yet

R+   recovery ready: local GPS and sonde GPS available
R?   partial recovery state: one GPS source missing
R--  recovery not ready

LOG  SD logging active
SD   SD available, logging off
SD!  SD unavailable or error

LCK  valid frequency / sonde lock
SCN  scanning active
F--  no sonde on current frequency and scanning off

ONL  online / Wi-Fi connected
W..  Wi-Fi configured but not connected
W--  Wi-Fi not configured
```

The battery icon and battery percentage are shown on the right.

## Web Mode

Web Mode is a read-only local dashboard.

It uses Wi-Fi client/STA mode only. SondeDeck connects to an existing Wi-Fi network, such as your phone hotspot, and your phone opens the SondeDeck IP address.

Web Mode does **not** create its own access point.

### Starting Web Mode

From the TFT UI:

```text
Tap Web in the bottom control bar
```

or press:

```text
M
```

### Exiting Web Mode

Only this exits Web Mode:

```text
Space
```

While Web Mode is active:

- touch is ignored
- trackball is ignored
- keyboard is ignored except Space
- the web page is read-only
- normal TFT page drawing is stopped

### Web routes

```text
GET /
GET /api/status
```

There are no POST routes and no command/control endpoints.

### Web dashboard data

The phone dashboard shows:

- sonde heard / waiting state
- sonde serial
- frame number
- RSSI and peak RSSI
- sonde GPS state
- sonde latitude/longitude/altitude
- local GPS state
- local latitude/longitude/altitude
- range, bearing, and elevation
- frequency and scan state
- SD/logging state
- battery percentage
- Wi-Fi status and IP address

When sonde GPS is valid, the dashboard shows a read-only Google Maps link:

```text
Open sonde in Google Maps
```

The link opens:

```text
https://www.google.com/maps/search/?api=1&query=<lat>,<lon>
```

## Wi-Fi configuration

Wi-Fi can be configured from the Online page.

Controls:

```text
O       Online page
N       Edit SSID
C       Edit password

Enter   Save
Ball    Save
Backspace delete
Space   Cycle ABC / 123 / SYM input mode
```

Wi-Fi credentials are saved in ESP32 flash using Preferences.

There is no Wi-Fi scanning. SondeDeck simply tries the saved SSID/password.

If no saved credentials exist, SondeDeck can still fall back to compile-time values in:

```text
src/config/wifi_config.h
```

Leave the SSID blank to keep Wi-Fi disabled by default.

### Wi-Fi states

```text
W--  no SSID configured
W..  SSID configured but not connected
ONL  connected
```

### Wi-Fi text input modes

When editing Wi-Fi text, normal page/action controls are locked out.

The editor has three modes:

```text
ABC -> 123 -> SYM -> ABC
```

Use Space while editing to cycle modes.

123 mode:

```text
W E R  ->  1 2 3
S D F  ->  4 5 6
Z X C  ->  7 8 9
Q/P    ->  0 fallback
```

SYM mode:

```text
Q T Y U I O P  ->  # ( ) - ' " @
A G H J K      ->  * / _ ; :
V B N M        ->  ? ! , $
```

The physical printed `0`/microphone key may not report through the keyboard library on some units, so `Q` and `P` are used as fallback `0` keys in 123 mode.

## SondeHub predictions

Wi-Fi is optional.

SondeDeck can query SondeHub predictions by serial using the v2 predictions endpoint:

```text
https://api.v2.sondehub.org/predictions?vehicles=<SERIAL>
```

SondeDeck does not upload:

- telemetry
- listener/station position
- chase-car position
- recovery reports

Prediction lookup depends on SondeHub already knowing about the sonde from another receiver or uploader.

## Controls

Keyboard shortcuts:

```text
Q       Overview
W       Sonde details
E       Recovery
R       Local GPS
T       Logging
Y       Settings
F       Frequency
O       Online
H       Home

Space   Help -> Icons -> About -> return
Info    Help -> Icons -> About

U/I     Move between Home tiles / pages
Enter   Open selected Home tile
Ball    Return Home from pages / open selected Home tile from Home

L       Toggle SD logging
B       Cycle screen brightness
K       Cycle keyboard brightness
D       Toggle touch input on/off
S       Toggle scan on/off
Z/X     Previous/next frequency preset
A       Reset counters and peak RSSI
P       Toggle auto dim
M       Start Web Mode
```

Touch controls:

```text
Tap a Home tile     Open that page
Tap Web             Start read-only Web Mode
Tap Home            Return to Home
Tap Info            Help -> Icons -> About
```

Trackball and keyboard input remain available when touch input is disabled.

## Default UI settings

v1.2 defaults:

```text
Touch input: ON
Screen brightness: 75%
Keyboard brightness: 0%
Auto dim: OFF
```

## GPS configuration

The T-Deck Plus GPS configuration confirmed during testing is:

```text
Baud: 38400
ESP RX: GPIO44
ESP TX: GPIO43
```

Arduino `HardwareSerial::begin()` uses this order:

```cpp
begin(baud, config, rxPin, txPin)
```

So the working SondeDeck GPS setup is:

```cpp
serial_.begin(
    38400,
    SERIAL_8N1,
    BoardPins::GPS_RX,
    BoardPins::GPS_TX
);
```

## Frequency management

SondeDeck uses configured RS41 frequency presets and a basic preset scanner.

This is not an SDR waterfall scanner. It steps through configured frequencies and looks for usable RS41 frames.

Frequency controls:

```text
F       Frequency page
Z/X     Previous/next preset
S       Scan on/off
```

Frequency status:

```text
LCK     Valid sonde/frequency lock
SCN     Scanning
F--     No sonde on current frequency and scanning off
```

## SD card logging

If an SD card is present, SondeDeck can log sonde telemetry and recovery history.

Logging can be toggled from the Logging page or with:

```text
L
```

If no SD card is present, SondeDeck continues running normally without logging.

## Configuration files

Important project configuration files:

```text
src/config/version.h
src/config/wifi_config.h
src/config/frequency_config.h
src/board_pins.h
```

Version information is set in:

```text
src/config/version.h
```

For v1.2:

```cpp
constexpr const char* VERSION = "v1.2";
```

## Building

Install PlatformIO, then from the project root run:

```powershell
pio run
```

Upload to the T-Deck Plus:

```powershell
pio run -t upload
```

Open serial monitor:

```powershell
pio device monitor -b 115200
```

A clean build can be forced with:

```powershell
pio run -t clean
pio run
```

## Project layout

```text
boards/
  T-Deck.json

lib/
  rs1729_rs41_fec/
  TFT_eSPI/
  TinyGPSPlus/

src/
  board_pins.h
  config.h
  main.cpp

  config/
    frequency_config.h
    version.h
    wifi_config.h

  decoder/
  gps/
  input/
  models/
  navigation/
  network/
  power/
  prediction/
  radio/
  semtech/
  storage/
  ui/

platformio.ini
README.md
THIRD_PARTY.md
LICENSE
```

## Known limitations

- RS41 only
- Preset/channel scanning only
- No SDR waterfall
- No offline wind-model prediction
- No handheld SondeHub telemetry upload
- SondeHub predictions require internet and external SondeHub data
- Web Mode is read-only
- Web Mode requires the phone/device to be able to reach SondeDeck on the same Wi-Fi/hotspot network
- Local GPS may not fix indoors
- Battery USB/charging indication may be inferred from voltage depending on hardware support
- The physical printed `0`/microphone key may not report through the keyboard library

## Safety and legal notes

Only receive signals you are legally allowed to receive in your location.

Radiosonde recovery may involve access restrictions, private land, roads, weather, water, cliffs, trees, livestock, and other hazards. Follow local laws and obtain permission where required.

## License

SondeDeck is released under the GNU General Public License v3.0 or later.

See:

```text
LICENSE
THIRD_PARTY.md
```
