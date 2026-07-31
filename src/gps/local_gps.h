#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>

class LocalGps {
public:
    // T-Deck Plus MIA-M10Q/u-blox GPS confirmed working at 38400 baud.
    bool begin(uint32_t baud = 38400);
    void update();

    bool fixValid(uint32_t maxAgeMs = 5000);

    double latitude();
    double longitude();
    double altitudeMetres();

    uint8_t satellites();
    double hdop();
    uint32_t fixAgeMs();

    uint32_t charsProcessed();
    uint32_t failedChecksumCount();
    uint32_t passedChecksumCount();

private:
    HardwareSerial serial_ = HardwareSerial(1);
    TinyGPSPlus gps_;
};
