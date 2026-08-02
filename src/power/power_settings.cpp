#include "power_settings.h"

namespace {
constexpr uint8_t SCREEN_LEVELS[] = {25, 50, 75, 100};
constexpr uint8_t KEYBOARD_LEVELS[] = {0, 25, 50, 75, 100};
constexpr uint8_t DIMMED_SCREEN_PERCENT = 12;
constexpr uint32_t DIM_TIMEOUT_MS = 60000;

template <typename T, size_t N>
constexpr size_t countOf(const T (&)[N]) {
    return N;
}
}

void PowerSettings::begin() {
    screenLevelIndex_ = 2;     // 75%
    keyboardLevelIndex_ = 0;   // off
    dimmingEnabled_ = false;
    displayDimmed_ = false;
}

uint8_t PowerSettings::screenBrightnessPercent() const {
    return SCREEN_LEVELS[screenLevelIndex_];
}

uint8_t PowerSettings::effectiveScreenBrightnessPercent() const {
    if (displayDimmed_) {
        return DIMMED_SCREEN_PERCENT;
    }

    return screenBrightnessPercent();
}

uint8_t PowerSettings::keyboardBrightness() const {
    return KEYBOARD_LEVELS[keyboardLevelIndex_];
}

uint8_t PowerSettings::helpKeyboardBrightness() const {
    return 100;
}

bool PowerSettings::dimmingEnabled() const {
    return dimmingEnabled_;
}

bool PowerSettings::displayDimmed() const {
    return displayDimmed_;
}

uint32_t PowerSettings::dimTimeoutMs() const {
    return DIM_TIMEOUT_MS;
}

uint32_t PowerSettings::dimTimeoutSeconds() const {
    return DIM_TIMEOUT_MS / 1000;
}

void PowerSettings::cycleScreenBrightness() {
    screenLevelIndex_ =
        (screenLevelIndex_ + 1) % countOf(SCREEN_LEVELS);
    displayDimmed_ = false;
}

void PowerSettings::increaseScreenBrightness() {
    if (screenLevelIndex_ + 1 < countOf(SCREEN_LEVELS)) {
        ++screenLevelIndex_;
    }

    displayDimmed_ = false;
}

void PowerSettings::decreaseScreenBrightness() {
    if (screenLevelIndex_ > 0) {
        --screenLevelIndex_;
    }

    displayDimmed_ = false;
}

void PowerSettings::cycleKeyboardBrightness() {
    keyboardLevelIndex_ =
        (keyboardLevelIndex_ + 1) % countOf(KEYBOARD_LEVELS);
}

bool PowerSettings::toggleDimmingEnabled() {
    dimmingEnabled_ = !dimmingEnabled_;

    if (!dimmingEnabled_) {
        displayDimmed_ = false;
    }

    return dimmingEnabled_;
}

bool PowerSettings::setDisplayDimmed(bool dimmed) {
    if (!dimmingEnabled_) {
        dimmed = false;
    }

    if (displayDimmed_ == dimmed) {
        return false;
    }

    displayDimmed_ = dimmed;
    return true;
}
