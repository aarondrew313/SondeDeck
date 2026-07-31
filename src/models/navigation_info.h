#pragma once

#include <Arduino.h>

struct NavigationInfo {
    bool localFixValid = false;
    bool sondeFixValid = false;
    bool navValid = false;

    double localLatitude = NAN;
    double localLongitude = NAN;
    double localAltitudeMetres = NAN;

    uint8_t localSatellites = 0;
    double localHdop = NAN;
    uint32_t localFixAgeMs = 0;

    double distanceMetres = NAN;
    double bearingDegrees = NAN;
    double elevationDegrees = NAN;
    double straightLineMetres = NAN;
    double relativeAltitudeMetres = NAN;
};
