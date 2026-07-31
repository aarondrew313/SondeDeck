#pragma once

// Leave SSID blank to keep Wi-Fi disabled.
// Do not commit real Wi-Fi credentials to a public GitHub repository.

constexpr const char* SONDEDECK_WIFI_SSID = "";
constexpr const char* SONDEDECK_WIFI_PASSWORD = "";

// SondeHub prediction API.
// SondeDeck only reads predictions. It does not upload telemetry, station
// positions, chase-car positions, or recovery reports.
constexpr const char* SONDEDECK_SONDEHUB_PREDICTIONS_URL =
    "https://api.v2.sondehub.org/predictions";

// Query SondeHub prediction data at most once per minute after a successful
// request. Failed requests retry sooner.
constexpr uint32_t SONDEDECK_SONDEHUB_SUCCESS_INTERVAL_MS = 60000;
constexpr uint32_t SONDEDECK_SONDEHUB_FAILED_INTERVAL_MS = 30000;
