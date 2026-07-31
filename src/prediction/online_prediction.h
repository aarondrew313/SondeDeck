#pragma once

#include <Arduino.h>

#include "../models/navigation_info.h"
#include "../models/prediction_info.h"
#include "../models/sonde_telemetry.h"

class OnlinePredictionClient {
public:
    void begin();

    bool configured() const;

    void update(
        const SondeTelemetry& telemetry,
        bool sondeGpsUsable,
        bool wifiConnected,
        const NavigationInfo& navigation
    );

    PredictionInfo info() const;

private:
    bool requestPrediction(
        const SondeTelemetry& telemetry,
        const NavigationInfo& navigation
    );

    bool parseResponse(
        const String& response,
        const SondeTelemetry& telemetry,
        const NavigationInfo& navigation
    );

    void calculateTargetNavigation(
        const NavigationInfo& navigation
    );

    void clearResult();
    void setStatus(const char* status);
    void safeCopy(char* destination, size_t destinationLength, const char* source);

    bool configured_ = true;
    bool available_ = false;
    bool lastRequestOk_ = false;
    bool requestInProgress_ = false;

    uint32_t lastAttemptMs_ = 0;
    uint32_t lastSuccessMs_ = 0;

    double latitude_ = NAN;
    double longitude_ = NAN;
    double altitudeMetres_ = NAN;

    bool landed_ = false;
    int32_t etaSeconds_ = -1;

    bool targetNavValid_ = false;
    double targetRangeMetres_ = NAN;
    double targetBearingDegrees_ = NAN;

    char vehicle_[16] = "";
    char source_[32] = "SondeHub";
    char predictionTime_[32] = "";
    char status_[80] = "";
};
