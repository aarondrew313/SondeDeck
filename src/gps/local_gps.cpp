#include "local_gps.h"

#include <math.h>

#include "../board_pins.h"

bool LocalGps::begin(uint32_t baud) {
    // Arduino HardwareSerial order is:
    // begin(baud, config, rxPin, txPin)
    //
    // On this T-Deck Plus, GPS works with:
    // RX = GPIO44
    // TX = GPIO43
    //
    // BoardPins::GPS_RX is GPIO44 and BoardPins::GPS_TX is GPIO43.
    serial_.begin(
        baud,
        SERIAL_8N1,
        BoardPins::GPS_RX,
        BoardPins::GPS_TX
    );

    Serial.printf(
        "GPS UART started: baud=%lu rx=%d tx=%d\n",
        static_cast<unsigned long>(baud),
        BoardPins::GPS_RX,
        BoardPins::GPS_TX
    );

    return true;
}

void LocalGps::update() {
    while (serial_.available() > 0) {
        gps_.encode(static_cast<char>(serial_.read()));
    }
}

bool LocalGps::fixValid(uint32_t maxAgeMs) {
    return gps_.location.isValid() &&
           gps_.location.age() <= maxAgeMs;
}

double LocalGps::latitude() {
    return gps_.location.isValid() ? gps_.location.lat() : NAN;
}

double LocalGps::longitude() {
    return gps_.location.isValid() ? gps_.location.lng() : NAN;
}

double LocalGps::altitudeMetres() {
    return gps_.altitude.isValid() ? gps_.altitude.meters() : NAN;
}

uint8_t LocalGps::satellites() {
    if (!gps_.satellites.isValid()) {
        return 0;
    }

    const uint32_t value = gps_.satellites.value();

    return value > 255 ? 255 : static_cast<uint8_t>(value);
}

double LocalGps::hdop() {
    return gps_.hdop.isValid() ? gps_.hdop.hdop() : NAN;
}

uint32_t LocalGps::fixAgeMs() {
    return gps_.location.isValid() ? gps_.location.age() : 0xFFFFFFFFUL;
}

uint32_t LocalGps::charsProcessed() {
    return gps_.charsProcessed();
}

uint32_t LocalGps::failedChecksumCount() {
    return gps_.failedChecksum();
}

uint32_t LocalGps::passedChecksumCount() {
    return gps_.passedChecksum();
}
