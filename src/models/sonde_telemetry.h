#pragma once

#include <Arduino.h>

struct SondeTelemetry {
    bool valid = false;
    bool headerValid = false;
    bool fecValid = false;
    bool statusValid = false;
    bool gpsInfoValid = false;
    bool positionValid = false;

    bool gps1BlockSeen = false;
    bool gps3BlockSeen = false;
    bool gps1CrcValid = false;
    bool gps3CrcValid = false;

    char serial[9] = {};
    uint16_t frameNumber = 0;
    float batteryVoltage = NAN;

    uint16_t gpsWeek = 0;
    uint32_t gpsTowMs = 0;

    double latitude = NAN;
    double longitude = NAN;
    double altitudeMetres = NAN;

    double horizontalSpeedMps = NAN;
    double verticalSpeedMps = NAN;
    double headingDegrees = NAN;

    uint8_t satellites = 0;
    float speedAccuracyMps = NAN;
    float positionDop = NAN;

    int8_t rssiDbm = -127;
    uint8_t correctedErrors = 0;

    bool statusCrcValid = false;
    bool gpsInfoCrcValid = false;
    bool gpsPositionCrcValid = false;

    int32_t rawEcefXcm = 0;
    int32_t rawEcefYcm = 0;
    int32_t rawEcefZcm = 0;

    int16_t rawEcefVxCms = 0;
    int16_t rawEcefVyCms = 0;
    int16_t rawEcefVzCms = 0;

    uint8_t gps3Raw[25] = {};
    uint8_t gps3RawLength = 0;
};
