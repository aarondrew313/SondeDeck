#pragma once

#include <Arduino.h>

enum class TouchEventType {
    None,
    Down,
    Move,
    Up,
    Tap
};

struct TouchEvent {
    TouchEventType type = TouchEventType::None;
    int16_t x = -1;
    int16_t y = -1;
    uint8_t points = 0;
};

class TouchInput {
public:
    bool begin();
    TouchEvent poll();

    bool available() const {
        return available_;
    }

private:
    bool available_ = false;
    bool lastPressed_ = false;
    int16_t lastX_ = -1;
    int16_t lastY_ = -1;
    int16_t pressedX_ = -1;
    int16_t pressedY_ = -1;
    uint32_t pressedAtMs_ = 0;
};
