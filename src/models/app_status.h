#pragma once

#include <Arduino.h>

#include "../power/battery_monitor.h"
#include "network_status.h"
#include "prediction_info.h"
#include "frequency_status.h"
#include "../storage/sd_logger.h"
#include "../ui/page_id.h"

struct AppCounters {
    uint32_t validFrames = 0;
    uint32_t gpsFrames = 0;
    uint32_t rejectedFrames = 0;
    uint32_t radioAbortedFrames = 0;
    int8_t peakRssiDbm = -127;
};

struct AppStatus {
    DisplayPage activePage = DisplayPage::Overview;
    AppCounters counters;
    LoggerStatus logger;
    BatteryState battery;

    bool signalLost = false;
    uint32_t msSinceLastValidFrame = 0;

    uint32_t localGpsChars = 0;
    uint32_t localGpsPassed = 0;
    uint32_t localGpsFailed = 0;

    uint8_t screenBrightnessPercent = 80;
    uint8_t effectiveScreenBrightnessPercent = 80;
    uint8_t keyboardBrightness = 0;
    bool displayDimmed = false;
    bool dimmingEnabled = true;
    uint32_t dimTimeoutSeconds = 60;
    uint32_t secondsUntilDim = 60;

    NetworkStatus network;
    PredictionInfo prediction;
    FrequencyStatus frequency;
};
