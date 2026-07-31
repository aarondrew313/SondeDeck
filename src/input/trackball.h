#pragma once

#include <Arduino.h>

enum class TrackballEvent {
    None,
    Press
};

class TrackballInput {
public:
    bool begin();
    TrackballEvent poll();

private:
    uint32_t lastPressMs_ = 0;
    bool lastPressState_ = false;
};
