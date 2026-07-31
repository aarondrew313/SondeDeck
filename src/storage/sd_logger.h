#pragma once

#include <Arduino.h>

#include "../models/navigation_info.h"
#include "../models/sonde_telemetry.h"
#include "../power/battery_monitor.h"

struct LoggerStatus {
    bool available = false;
    bool enabled = false;
    bool lastWriteOk = false;
    uint32_t framesLogged = 0;

    char activeSerial[12] = "";
    char activePath[64] = "";
    char historyRoot[24] = "";
    char lastError[80] = "";
};

class SdLogger {
public:
    bool begin();

    bool available() const;
    bool enabled() const;

    void setEnabled(bool enabled);
    bool toggleEnabled();

    void resetSessionCounter();

    bool logFrame(
        const SondeTelemetry& telemetry,
        bool gpsPositionUsable,
        const NavigationInfo& navigation,
        const BatteryState& battery,
        uint32_t timestampMs
    );

    LoggerStatus status() const;

private:
    bool ensureReady();
    bool ensureBaseDirectories();
    bool ensureSondeDirectory(const char* serial, char* sondeDir, size_t sondeDirLength);

    bool writeHeaderIfNeeded(const char* path);
    bool appendTrackFrame(
        const char* path,
        const SondeTelemetry& telemetry,
        bool gpsPositionUsable,
        const NavigationInfo& navigation,
        const BatteryState& battery,
        uint32_t timestampMs
    );

    bool writeSummary(
        const char* path,
        const SondeTelemetry& telemetry,
        bool gpsPositionUsable,
        const NavigationInfo& navigation,
        const BatteryState& battery,
        uint32_t timestampMs
    );

    bool writeLastSeen(
        const SondeTelemetry& telemetry,
        bool gpsPositionUsable,
        const NavigationInfo& navigation,
        const BatteryState& battery,
        uint32_t timestampMs
    );

    bool noteSondeSeen(
        const char* serial,
        const char* trackPath,
        const char* summaryPath,
        uint32_t timestampMs
    );

    void normaliseSerial(const char* input, char* output, size_t outputLength);
    void setError(const char* message);
    void clearError();

    bool available_ = false;
    bool enabled_ = false;
    bool directoriesReady_ = false;
    bool lastWriteOk_ = false;

    uint32_t framesLogged_ = 0;
    uint32_t activeSondeFrames_ = 0;
    uint32_t activeSondeFirstSeenMs_ = 0;

    char activeSerial_[12] = "";
    char activePath_[64] = "";
    char lastError_[80] = "";
};
