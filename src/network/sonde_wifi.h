#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "../models/network_status.h"

class SondeWifi {
public:
    void begin();
    void update();

    bool configured() const;
    bool connected() const;

    const char* ssid() const;
    const char* password() const;

    void setCredentials(const char* ssid, const char* password);

    NetworkStatus status() const;

private:
    void loadCredentials();
    void startConnection();

    Preferences preferences_;

    bool preferencesOpen_ = false;
    bool configured_ = false;
    bool connecting_ = false;
    uint32_t lastAttemptMs_ = 0;

    char ssid_[33] = "";
    char password_[65] = "";
};
