#pragma once

#include <Arduino.h>

namespace BoardPins {
constexpr uint8_t POWER_ENABLE = 10;

constexpr uint8_t SPI_SCK  = 40;
constexpr uint8_t SPI_MISO = 38;
constexpr uint8_t SPI_MOSI = 41;

constexpr uint8_t DISPLAY_CS = 12;
constexpr uint8_t DISPLAY_BACKLIGHT = 42;
constexpr uint8_t SD_CS  = 39;

constexpr uint8_t RADIO_NSS   = 9;
constexpr uint8_t RADIO_DIO1  = 45;
constexpr uint8_t RADIO_RESET = 17;
constexpr uint8_t RADIO_BUSY  = 13;

// T-Deck Plus battery monitor ADC
constexpr uint8_t BATTERY_ADC = 4;

// T-Deck keyboard I2C bus
constexpr uint8_t I2C_SDA = 18;
constexpr uint8_t I2C_SCL = 8;
constexpr uint8_t KEYBOARD_ADDR = 0x55;

// T-Deck keyboard MCU commands
constexpr uint8_t KEYBOARD_BRIGHTNESS_CMD = 0x01;
constexpr uint8_t KEYBOARD_DEFAULT_BRIGHTNESS_CMD = 0x02;

// T-Deck trackball. LilyGO names these G01-G04.
constexpr uint8_t TRACKBALL_G01 = 3;   // X+
constexpr uint8_t TRACKBALL_G02 = 2;   // X-
constexpr uint8_t TRACKBALL_G03 = 15;  // Y+
constexpr uint8_t TRACKBALL_G04 = 1;   // Y-
constexpr uint8_t TRACKBALL_PRESS = 0; // BOOT / centre press

// T-Deck Plus GNSS UART.
// These names follow the GPS-module pins. Arduino Serial1.begin()
// takes rxPin first, so local_gps.cpp passes GPS_TX then GPS_RX.
constexpr uint8_t GPS_TX = 43;
constexpr uint8_t GPS_RX = 44;
}
