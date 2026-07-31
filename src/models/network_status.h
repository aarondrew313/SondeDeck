#pragma once

#include <Arduino.h>

struct NetworkStatus {
    bool configured = false;
    bool connected = false;
    bool connecting = false;

    int32_t rssiDbm = 0;
    uint32_t lastAttemptMs = 0;

    char ipAddress[20] = "";
    char status[32] = "";
};
