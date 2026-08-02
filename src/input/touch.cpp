#include "touch.h"

#include <Wire.h>
#include <TouchDrvGT911.hpp>

#include "../board_pins.h"

namespace {
constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;
constexpr int8_t TOUCH_RST_PIN = -1;
constexpr uint8_t TOUCH_INT_PIN = 16;
constexpr uint32_t TAP_MAX_MS = 650;
constexpr int16_t TAP_MAX_MOVE = 20;

TouchDrvGT911 touch;

const uint8_t touchAddresses[] = {
    0x5D,
    0x14
};
}

bool TouchInput::begin() {
    Serial.println("TOUCH: starting GT911 input");

    pinMode(TOUCH_INT_PIN, INPUT);

    // Keyboard and touch share the T-Deck I2C bus.
    Wire.begin(BoardPins::I2C_SDA, BoardPins::I2C_SCL);
    Wire.setClock(400000);

    touch.setPins(TOUCH_RST_PIN, TOUCH_INT_PIN);

    for (uint8_t i = 0; i < sizeof(touchAddresses); ++i) {
        const uint8_t address = touchAddresses[i];

        if (touch.begin(Wire, address)) {
            available_ = true;

            touch.setMaxCoordinates(SCREEN_W, SCREEN_H);
            touch.setSwapXY(true);
            touch.setMirrorXY(false, true);

            Serial.printf(
                "TOUCH: GT911 ready at 0x%02X int=%u size=%dx%d\n",
                address,
                TOUCH_INT_PIN,
                SCREEN_W,
                SCREEN_H
            );

            return true;
        }
    }

    available_ = false;
    Serial.println("TOUCH: GT911 not detected, continuing without touch");
    return false;
}

TouchEvent TouchInput::poll() {
    TouchEvent event;

    if (!available_) {
        return event;
    }

    const bool pressed = touch.isPressed();
    const uint32_t now = millis();

    if (!pressed) {
        if (lastPressed_) {
            const uint32_t heldMs =
                now >= pressedAtMs_ ? now - pressedAtMs_ : 0;

            const bool tapMovement =
                abs(lastX_ - pressedX_) <= TAP_MAX_MOVE &&
                abs(lastY_ - pressedY_) <= TAP_MAX_MOVE;

            event.type =
                heldMs <= TAP_MAX_MS && tapMovement
                    ? TouchEventType::Tap
                    : TouchEventType::Up;
            event.x = lastX_;
            event.y = lastY_;
            event.points = 0;
        }

        lastPressed_ = false;
        return event;
    }

    int16_t x[5] = {};
    int16_t y[5] = {};
    const uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());

    if (touched == 0) {
        return event;
    }

    event.x = x[0];
    event.y = y[0];
    event.points = touched;

    if (!lastPressed_) {
        lastPressed_ = true;
        pressedAtMs_ = now;
        pressedX_ = event.x;
        pressedY_ = event.y;
        lastX_ = event.x;
        lastY_ = event.y;

        event.type = TouchEventType::Down;
        return event;
    }

    const bool moved =
        abs(event.x - lastX_) > 2 ||
        abs(event.y - lastY_) > 2;

    lastX_ = event.x;
    lastY_ = event.y;

    if (moved) {
        event.type = TouchEventType::Move;
        return event;
    }

    return event;
}
