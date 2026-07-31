#pragma once

#include <Arduino.h>

#include "../models/app_status.h"
#include "../models/navigation_info.h"
#include "../models/sonde_telemetry.h"
#include "page_id.h"

class SondeDisplay {
public:
    bool begin();

    void resetScreen();

    void setBrightnessPercent(uint8_t percent);

    void showSplash(bool promptVisible);

    void showBoot(
        const char* title,
        const char* status
    );

    void showPage(
        DisplayPage page,
        const SondeTelemetry* telemetry,
        bool gpsPositionUsable,
        const NavigationInfo& navigation,
        const AppStatus& status
    );

    void showDecodeFailure(
        const char* reason,
        int8_t rssiDbm,
        const AppStatus& status
    );

private:
    bool ready_ = false;
};
