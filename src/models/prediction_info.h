#pragma once

#include <Arduino.h>

struct PredictionInfo {
    bool configured = true;
    bool available = false;
    bool lastRequestOk = false;
    bool requestInProgress = false;

    uint32_t lastAttemptMs = 0;
    uint32_t lastSuccessMs = 0;

    double latitude = NAN;
    double longitude = NAN;
    double altitudeMetres = NAN;

    bool landed = false;
    int32_t etaSeconds = -1;

    bool targetNavValid = false;
    double targetRangeMetres = NAN;
    double targetBearingDegrees = NAN;

    char vehicle[16] = "";
    char source[32] = "SondeHub";
    char predictionTime[32] = "";
    char status[80] = "";
};
