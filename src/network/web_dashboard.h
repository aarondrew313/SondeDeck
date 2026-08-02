#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "../models/app_status.h"
#include "../models/navigation_info.h"
#include "../models/sonde_telemetry.h"

class WebDashboard {
public:
    bool begin();
    void stop();
    void handleClient();

    bool running() const;
    const char* ipAddress() const;
    const char* statusText() const;

    void updateSnapshot(
        const SondeTelemetry* telemetry,
        bool hasTelemetry,
        bool sondeGpsUsable,
        const NavigationInfo& navigation,
        const AppStatus& status
    );

private:
    void handleRoot();
    void handleStatus();
    void handleNotFound();

    void appendJsonString(String& output, const char* value);
    void appendJsonNumber(String& output, double value, uint8_t decimals = 6);

    WebServer server_{80};
    bool running_ = false;
    bool routesConfigured_ = false;

    char ipAddress_[20] = "--";
    char statusText_[48] = "off";
    String cachedJson_ = "{}";
};
