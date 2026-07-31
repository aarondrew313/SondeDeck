#include "keyboard.h"

#include <Wire.h>

#include "../board_pins.h"

bool TDeckKeyboard::begin() {
    Wire.begin(BoardPins::I2C_SDA, BoardPins::I2C_SCL);
    Wire.setClock(400000);

    setDefaultBacklight(120);
    setBacklight(0);

    return true;
}

char TDeckKeyboard::readKey() {
    Wire.requestFrom(
        static_cast<uint8_t>(BoardPins::KEYBOARD_ADDR),
        static_cast<uint8_t>(1)
    );

    if (Wire.available() < 1) {
        return 0;
    }

    return static_cast<char>(Wire.read());
}

bool TDeckKeyboard::continuePressed() {
    const char key = readKey();

    if (key == 0) {
        return false;
    }

    const bool accepted =
        key == ' ' ||
        key == '\r' ||
        key == '\n';

    if (!accepted) {
        return false;
    }

    const uint32_t now = millis();

    if (now - lastAcceptedMs_ < 250) {
        return false;
    }

    lastAcceptedMs_ = now;
    return true;
}

void TDeckKeyboard::setBacklight(uint8_t brightness) {
    Wire.beginTransmission(BoardPins::KEYBOARD_ADDR);
    Wire.write(BoardPins::KEYBOARD_BRIGHTNESS_CMD);
    Wire.write(brightness);
    Wire.endTransmission();
}

void TDeckKeyboard::setDefaultBacklight(uint8_t brightness) {
    Wire.beginTransmission(BoardPins::KEYBOARD_ADDR);
    Wire.write(BoardPins::KEYBOARD_DEFAULT_BRIGHTNESS_CMD);
    Wire.write(brightness);
    Wire.endTransmission();
}
