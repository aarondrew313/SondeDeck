#pragma once

#include <Arduino.h>

#include "../models/frequency_status.h"

class FrequencyManager {
public:
    void begin();

    uint32_t currentFrequencyHz() const;

    uint32_t selectNextPreset();
    uint32_t selectPreviousPreset();

    bool toggleScan();
    void stopScan();

    bool scanEnabled() const;
    bool locked() const;

    bool shouldAdvanceScan(bool receiverSearching, uint32_t nowMs) const;
    uint32_t advanceScan(uint32_t nowMs);

    void noteTuned(uint32_t nowMs);
    void noteFrameRssi(int8_t rssiDbm);
    void noteValidFrame(const char* serial, int8_t rssiDbm, uint32_t nowMs);

    FrequencyStatus status(uint32_t nowMs) const;

private:
    void findDefaultPreset();
    void copySerial(const char* serial);

    uint8_t presetIndex_ = 0;
    bool scanEnabled_ = false;
    bool locked_ = false;

    uint32_t lastTuneMs_ = 0;

    int8_t lastRssiDbm_ = -127;
    int8_t bestRssiDbm_ = -127;
    uint32_t bestFrequencyHz_ = 0;

    char lockedSerial_[12] = "";
};
