#pragma once

#include <Arduino.h>

namespace FrequencyConfig {
constexpr uint32_t DEFAULT_FREQUENCY_HZ = 405700000UL;
constexpr uint32_t SCAN_DWELL_MS = 2500;

// RS41-only preset list. Keep the strongest known local frequency in here;
// use Z/X on the Frequency page to step manually, or S to scan.
constexpr uint32_t PRESETS_HZ[] = {
    400500000UL,
    401000000UL,
    401100000UL,
    401500000UL,
    402000000UL,
    402700000UL,
    403000000UL,
    403500000UL,
    404000000UL,
    404500000UL,
    405000000UL,
    405700000UL,
    406000000UL
};

constexpr uint8_t PRESET_COUNT =
    static_cast<uint8_t>(sizeof(PRESETS_HZ) / sizeof(PRESETS_HZ[0]));
}
