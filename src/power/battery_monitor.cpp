#include "battery_monitor.h"

#include <math.h>

#include "../board_pins.h"

namespace {
bool hasSmoothedValue = false;
float smoothedVoltage = 0.0f;

float readBatteryVoltage() {
    uint32_t totalMv = 0;
    constexpr uint8_t samples = 8;

    for (uint8_t i = 0; i < samples; ++i) {
        totalMv += analogReadMilliVolts(BoardPins::BATTERY_ADC);
        delayMicroseconds(250);
    }

    const float adcMv =
        static_cast<float>(totalMv) / static_cast<float>(samples);

    return (adcMv * 2.0f) / 1000.0f;
}

int percentFromVoltage(float voltage) {
    struct Point {
        float voltage;
        int percent;
    };

    constexpr Point curve[] = {
        {4.20f, 100},
        {4.10f, 90},
        {4.00f, 80},
        {3.92f, 70},
        {3.85f, 60},
        {3.79f, 50},
        {3.73f, 40},
        {3.68f, 30},
        {3.61f, 20},
        {3.50f, 10},
        {3.30f, 0},
    };

    if (!isfinite(voltage) || voltage < 2.5f) {
        return -1;
    }

    if (voltage >= curve[0].voltage) {
        return 100;
    }

    for (size_t i = 0; i < (sizeof(curve) / sizeof(curve[0])) - 1; ++i) {
        const Point high = curve[i];
        const Point low = curve[i + 1];

        if (voltage <= high.voltage && voltage >= low.voltage) {
            const float span = high.voltage - low.voltage;
            const float position = (voltage - low.voltage) / span;

            return low.percent +
                   static_cast<int>(
                       (high.percent - low.percent) * position + 0.5f
                   );
        }
    }

    return 0;
}
}

void BatteryMonitor::begin() {
    pinMode(BoardPins::BATTERY_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(BoardPins::BATTERY_ADC, ADC_11db);
}

BatteryState BatteryMonitor::read() {
    const float voltage = readBatteryVoltage();

    if (!hasSmoothedValue) {
        smoothedVoltage = voltage;
        hasSmoothedValue = true;
    } else {
        smoothedVoltage = (smoothedVoltage * 0.85f) + (voltage * 0.15f);
    }

    BatteryState state;
    state.voltage = smoothedVoltage;
    state.percent = percentFromVoltage(smoothedVoltage);

    // T-Deck Plus exposes battery ADC but not a separate VBUS/charger status
    // pin in the public pin list. This is a practical USB/charging hint.
    state.externalPowerLikely =
        isfinite(smoothedVoltage) && smoothedVoltage >= 4.18f;

    return state;
}
