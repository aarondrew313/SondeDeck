# Third-party components

SondeDeck includes or depends on a small number of third-party components.

This file is a practical summary only. The licence files bundled with each component are the authoritative licence terms.

## Vendored components

### rs1729_rs41_fec

Path:

```text
lib/rs1729_rs41_fec/
```

Purpose:

- RS41 FEC/BCH support
- Wrapper around upstream RS41 FEC code

Licence information:

```text
lib/rs1729_rs41_fec/LICENSE
lib/rs1729_rs41_fec/src/README.md
```

The RS41 FEC component is GPL-licensed. SondeDeck is therefore distributed under **GPL-3.0-or-later**.

### TFT_eSPI

Path:

```text
lib/TFT_eSPI/
```

Purpose:

- ST7789 display support
- TFT drawing primitives
- Text and simple UI rendering

Licence information:

```text
lib/TFT_eSPI/license.txt
```

### TinyGPSPlus

Path:

```text
lib/TinyGPSPlus/
```

Purpose:

- Parsing local GPS NMEA data from the T-Deck Plus GPS module

Licence / metadata information:

```text
lib/TinyGPSPlus/library.properties
```

If publishing a formal long-term release, consider adding the upstream TinyGPSPlus licence file into the vendored library folder if it is not already present.

## PlatformIO dependencies

### Semtech SX126x driver

Purpose:

- SX1262 low-level radio driver

This dependency is pulled by PlatformIO during build and is not committed under `lib/` in the SondeDeck source tree.

Licence information is provided by the dependency package pulled by PlatformIO.

## External services

### SondeHub

SondeDeck can query SondeHub predictions in read-only mode by sonde serial.

The handheld does not upload telemetry, listener position, chase-car position, or recovery reports.

SondeDeck is not affiliated with or endorsed by SondeHub.
