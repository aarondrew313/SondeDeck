#pragma once

#include <Arduino.h>

struct FrequencyStatus {
    bool scanEnabled = false;
    bool locked = false;

    uint8_t presetIndex = 0;
    uint8_t presetCount = 0;

    uint32_t currentFrequencyHz = 0;
    uint32_t bestFrequencyHz = 0;
    uint32_t scanDwellMs = 0;
    uint32_t msOnFrequency = 0;

    int8_t lastRssiDbm = -127;
    int8_t bestRssiDbm = -127;

    char mode[16] = "Fixed";
    char lockedSerial[12] = "";
};
