#include "sonde_wifi.h"

#include <WiFi.h>
#include <string.h>

#include "../config/wifi_config.h"

namespace {
constexpr uint32_t RETRY_INTERVAL_MS = 30000;

void safeCopy(char* destination, size_t destinationLength, const char* source) {
    if (destinationLength == 0) {
        return;
    }

    strncpy(destination, source, destinationLength - 1);
    destination[destinationLength - 1] = '\0';
}

const char* wifiStatusText(wl_status_t status) {
    switch (status) {
        case WL_CONNECTED:
            return "connected";
        case WL_NO_SSID_AVAIL:
            return "SSID not found";
        case WL_CONNECT_FAILED:
            return "connect failed";
        case WL_CONNECTION_LOST:
            return "connection lost";
        case WL_DISCONNECTED:
            return "disconnected";
        case WL_IDLE_STATUS:
            return "idle";
        default:
            return "connecting";
    }
}
}

void SondeWifi::begin() {
    configured_ =
        SONDEDECK_WIFI_SSID != nullptr &&
        SONDEDECK_WIFI_SSID[0] != '\0';

    if (!configured_) {
        WiFi.mode(WIFI_OFF);
        connecting_ = false;
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);

    startConnection();
}

void SondeWifi::update() {
    if (!configured_) {
        return;
    }

    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        connecting_ = false;
        return;
    }

    const uint32_t now = millis();

    if (now - lastAttemptMs_ >= RETRY_INTERVAL_MS) {
        startConnection();
    }
}

bool SondeWifi::configured() const {
    return configured_;
}

bool SondeWifi::connected() const {
    return configured_ && WiFi.status() == WL_CONNECTED;
}

NetworkStatus SondeWifi::status() const {
    NetworkStatus value;

    value.configured = configured_;
    value.connected = connected();
    value.connecting = configured_ && !value.connected && connecting_;
    value.lastAttemptMs = lastAttemptMs_;

    if (!configured_) {
        safeCopy(value.status, sizeof(value.status), "not configured");
        return value;
    }

    const wl_status_t currentStatus = WiFi.status();
    safeCopy(value.status, sizeof(value.status), wifiStatusText(currentStatus));

    if (currentStatus == WL_CONNECTED) {
        value.rssiDbm = WiFi.RSSI();

        IPAddress ip = WiFi.localIP();
        snprintf(
            value.ipAddress,
            sizeof(value.ipAddress),
            "%u.%u.%u.%u",
            ip[0],
            ip[1],
            ip[2],
            ip[3]
        );
    }

    return value;
}

void SondeWifi::startConnection() {
    lastAttemptMs_ = millis();
    connecting_ = true;

    WiFi.disconnect(false, true);
    delay(20);
    WiFi.begin(SONDEDECK_WIFI_SSID, SONDEDECK_WIFI_PASSWORD);
}
