#include <Arduino.h>
#include <SPI.h>

#include "../board_pins.h"

extern "C" {
#include <sx126x_hal.h>
}

namespace {
SPISettings radioSpiSettings(4000000, MSBFIRST, SPI_MODE0);

bool waitUntilReady(uint32_t timeoutMs = 1000) {
    const uint32_t started = millis();

    while (digitalRead(BoardPins::RADIO_BUSY) == HIGH) {
        if (millis() - started >= timeoutMs) {
            return false;
        }
        delayMicroseconds(10);
    }

    return true;
}
}

extern "C" sx126x_hal_status_t sx126x_hal_write(
    const void*,
    const uint8_t* command,
    const uint16_t commandLength,
    const uint8_t* data,
    const uint16_t dataLength
) {
    if (!waitUntilReady()) {
        return SX126X_HAL_STATUS_ERROR;
    }

    SPI.beginTransaction(radioSpiSettings);
    digitalWrite(BoardPins::RADIO_NSS, LOW);

    for (uint16_t i = 0; i < commandLength; ++i) {
        SPI.transfer(command[i]);
    }

    for (uint16_t i = 0; i < dataLength; ++i) {
        SPI.transfer(data[i]);
    }

    digitalWrite(BoardPins::RADIO_NSS, HIGH);
    SPI.endTransaction();

    return SX126X_HAL_STATUS_OK;
}

extern "C" sx126x_hal_status_t sx126x_hal_read(
    const void*,
    const uint8_t* command,
    const uint16_t commandLength,
    uint8_t* data,
    const uint16_t dataLength
) {
    if (!waitUntilReady()) {
        return SX126X_HAL_STATUS_ERROR;
    }

    SPI.beginTransaction(radioSpiSettings);
    digitalWrite(BoardPins::RADIO_NSS, LOW);

    for (uint16_t i = 0; i < commandLength; ++i) {
        SPI.transfer(command[i]);
    }

    for (uint16_t i = 0; i < dataLength; ++i) {
        data[i] = SPI.transfer(0x00);
    }

    digitalWrite(BoardPins::RADIO_NSS, HIGH);
    SPI.endTransaction();

    return SX126X_HAL_STATUS_OK;
}

extern "C" sx126x_hal_status_t sx126x_hal_reset(const void*) {
    digitalWrite(BoardPins::RADIO_RESET, LOW);
    delayMicroseconds(200);
    digitalWrite(BoardPins::RADIO_RESET, HIGH);
    delay(5);

    return SX126X_HAL_STATUS_OK;
}

extern "C" sx126x_hal_status_t sx126x_hal_wakeup(const void*) {
    SPI.beginTransaction(radioSpiSettings);
    digitalWrite(BoardPins::RADIO_NSS, LOW);
    delayMicroseconds(500);
    digitalWrite(BoardPins::RADIO_NSS, HIGH);
    SPI.endTransaction();

    return waitUntilReady()
        ? SX126X_HAL_STATUS_OK
        : SX126X_HAL_STATUS_ERROR;
}
