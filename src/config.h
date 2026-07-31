#pragma once

#include <Arduino.h>

namespace SondeConfig {
constexpr uint32_t FREQUENCY_HZ = 405700000UL;

constexpr uint32_t BIT_RATE_BPS = 4800UL;
constexpr uint32_t FDEV_HZ      = 6300UL;

// 39 kHz is deliberately wider than the nominal signal for the
// first raw-capture build and tolerates some frequency error.
constexpr uint32_t RX_BANDWIDTH_HZ = 39000UL;

constexpr size_t FRAME_LENGTH = 312;

// The RS41 sync sequence as transmitted to the SX1262 after
// reversing the bit order in each published sync byte.
constexpr uint8_t SYNC_WORD[8] = {
    0x08, 0x6D, 0x53, 0x88, 0x44, 0x69, 0x48, 0x1F
};

constexpr uint32_t FIRST_DRAIN_DELAY_MS = 300;
constexpr uint32_t CAPTURE_TIMEOUT_MS   = 1100;
}
