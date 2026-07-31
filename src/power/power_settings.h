#pragma once

#include <Arduino.h>

class PowerSettings {
public:
    void begin();

    uint8_t screenBrightnessPercent() const;
    uint8_t effectiveScreenBrightnessPercent() const;
    uint8_t keyboardBrightness() const;
    uint8_t helpKeyboardBrightness() const;

    bool dimmingEnabled() const;
    bool displayDimmed() const;
    uint32_t dimTimeoutMs() const;
    uint32_t dimTimeoutSeconds() const;

    void cycleScreenBrightness();
    void increaseScreenBrightness();
    void decreaseScreenBrightness();

    void cycleKeyboardBrightness();

    bool toggleDimmingEnabled();

    bool setDisplayDimmed(bool dimmed);

private:
    uint8_t screenLevelIndex_ = 2;
    uint8_t keyboardLevelIndex_ = 0;
    bool dimmingEnabled_ = false;
    bool displayDimmed_ = false;
};
