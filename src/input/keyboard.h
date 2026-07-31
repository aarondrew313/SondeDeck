#pragma once

#include <Arduino.h>

class TDeckKeyboard {
public:
    bool begin();

    char readKey();

    bool continuePressed();

    void setBacklight(uint8_t brightness);
    void setDefaultBacklight(uint8_t brightness);

private:
    uint32_t lastAcceptedMs_ = 0;
};
