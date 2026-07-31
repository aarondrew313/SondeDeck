#pragma once

#include <Arduino.h>

struct BatteryState {
    float voltage = NAN;
    int percent = -1;
    bool externalPowerLikely = false;
};

namespace BatteryMonitor {
void begin();
BatteryState read();
}
