#pragma once

#include <Arduino.h>

#include "../models/network_status.h"

class SondeWifi {
public:
    void begin();
    void update();

    bool configured() const;
    bool connected() const;

    NetworkStatus status() const;

private:
    void startConnection();

    bool configured_ = false;
    bool connecting_ = false;
    uint32_t lastAttemptMs_ = 0;
};
