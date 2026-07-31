#include "trackball.h"

#include "../board_pins.h"

namespace {
constexpr uint32_t PRESS_DEBOUNCE_MS = 300;
}

bool TrackballInput::begin() {
    // Keep only the centre press. The ball movement pins are intentionally not
    // used because their direction/pulse behaviour is too noisy for reliable UI
    // navigation on this build.
    pinMode(BoardPins::TRACKBALL_PRESS, INPUT_PULLUP);
    return true;
}

TrackballEvent TrackballInput::poll() {
    const uint32_t now = millis();
    const bool pressed = digitalRead(BoardPins::TRACKBALL_PRESS) == LOW;

    if (pressed &&
        !lastPressState_ &&
        now - lastPressMs_ >= PRESS_DEBOUNCE_MS) {
        lastPressState_ = true;
        lastPressMs_ = now;
        return TrackballEvent::Press;
    }

    if (!pressed) {
        lastPressState_ = false;
    }

    return TrackballEvent::None;
}
