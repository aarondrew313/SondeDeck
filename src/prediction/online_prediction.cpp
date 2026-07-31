#include "online_prediction.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <string.h>

#include "../config/wifi_config.h"

namespace {
constexpr double PI_D = 3.14159265358979323846;
constexpr double EARTH_RADIUS_M = 6371000.0;

String urlEncode(const char* value) {
    String encoded;

    if (value == nullptr) {
        return encoded;
    }

    const char* hex = "0123456789ABCDEF";

    for (const char* p = value; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_' ||
            c == '.' ||
            c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }

    return encoded;
}

int findJsonKey(const String& json, const String& key) {
    const String quotedKey = "\"" + key + "\"";
    return json.indexOf(quotedKey);
}

String jsonStringValue(const String& json, const String& key) {
    const int keyPos = findJsonKey(json, key);

    if (keyPos < 0) {
        return String();
    }

    int colon = json.indexOf(':', keyPos);

    if (colon < 0) {
        return String();
    }

    int pos = colon + 1;

    while (pos < json.length() && isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (pos >= json.length() || json[pos] != '"') {
        return String();
    }

    ++pos;

    String value;
    bool escaped = false;

    while (pos < json.length()) {
        const char c = json[pos++];

        if (escaped) {
            value += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            break;
        }

        value += c;
    }

    return value;
}

String jsonRawValue(const String& json, const String& key) {
    const int keyPos = findJsonKey(json, key);

    if (keyPos < 0) {
        return String();
    }

    int colon = json.indexOf(':', keyPos);

    if (colon < 0) {
        return String();
    }

    int pos = colon + 1;

    while (pos < json.length() && isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    int end = pos;

    while (end < json.length()) {
        const char c = json[end];

        if (c == ',' || c == '}' || c == ']') {
            break;
        }

        ++end;
    }

    String value = json.substring(pos, end);
    value.trim();

    return value;
}

bool jsonBoolValue(const String& json, const String& key, bool fallback) {
    String value = jsonRawValue(json, key);
    value.toLowerCase();

    if (value == "true" || value == "1") {
        return true;
    }

    if (value == "false" || value == "0") {
        return false;
    }

    return fallback;
}

bool isFiniteLatLon(double lat, double lon) {
    return isfinite(lat) &&
           isfinite(lon) &&
           lat >= -90.0 &&
           lat <= 90.0 &&
           lon >= -180.0 &&
           lon <= 180.0;
}

double degToRad(double degrees) {
    return degrees * PI_D / 180.0;
}

double radToDeg(double radians) {
    return radians * 180.0 / PI_D;
}

double normaliseBearing(double degrees) {
    while (degrees < 0.0) {
        degrees += 360.0;
    }

    while (degrees >= 360.0) {
        degrees -= 360.0;
    }

    return degrees;
}

double haversineDistanceMetres(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    const double phi1 = degToRad(lat1);
    const double phi2 = degToRad(lat2);
    const double dPhi = degToRad(lat2 - lat1);
    const double dLambda = degToRad(lon2 - lon1);

    const double sinDPhi = sin(dPhi / 2.0);
    const double sinDLambda = sin(dLambda / 2.0);

    const double a =
        sinDPhi * sinDPhi +
        cos(phi1) * cos(phi2) * sinDLambda * sinDLambda;

    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}

double initialBearingDegrees(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    const double phi1 = degToRad(lat1);
    const double phi2 = degToRad(lat2);
    const double dLambda = degToRad(lon2 - lon1);

    const double y = sin(dLambda) * cos(phi2);
    const double x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(dLambda);

    return normaliseBearing(radToDeg(atan2(y, x)));
}
}

void OnlinePredictionClient::begin() {
    configured_ = true;
    clearResult();
    setStatus("waiting for Wi-Fi");
}

bool OnlinePredictionClient::configured() const {
    return configured_;
}

void OnlinePredictionClient::update(
    const SondeTelemetry& telemetry,
    bool sondeGpsUsable,
    bool wifiConnected,
    const NavigationInfo& navigation
) {
    if (!sondeGpsUsable) {
        setStatus("waiting for sonde GPS");
        return;
    }

    if (!wifiConnected) {
        setStatus("waiting for Wi-Fi");
        return;
    }

    const uint32_t now = millis();
    const uint32_t interval =
        lastRequestOk_
            ? SONDEDECK_SONDEHUB_SUCCESS_INTERVAL_MS
            : SONDEDECK_SONDEHUB_FAILED_INTERVAL_MS;

    if (lastAttemptMs_ != 0 && now - lastAttemptMs_ < interval) {
        calculateTargetNavigation(navigation);
        return;
    }

    requestPrediction(telemetry, navigation);
}

PredictionInfo OnlinePredictionClient::info() const {
    PredictionInfo value;

    value.configured = configured_;
    value.available = available_;
    value.lastRequestOk = lastRequestOk_;
    value.requestInProgress = requestInProgress_;
    value.lastAttemptMs = lastAttemptMs_;
    value.lastSuccessMs = lastSuccessMs_;
    value.latitude = latitude_;
    value.longitude = longitude_;
    value.altitudeMetres = altitudeMetres_;
    value.landed = landed_;
    value.etaSeconds = etaSeconds_;
    value.targetNavValid = targetNavValid_;
    value.targetRangeMetres = targetRangeMetres_;
    value.targetBearingDegrees = targetBearingDegrees_;

    value.vehicle[0] = '\0';
    value.source[0] = '\0';
    value.predictionTime[0] = '\0';
    value.status[0] = '\0';

    strncpy(value.vehicle, vehicle_, sizeof(value.vehicle) - 1);
    strncpy(value.source, source_, sizeof(value.source) - 1);
    strncpy(value.predictionTime, predictionTime_, sizeof(value.predictionTime) - 1);
    strncpy(value.status, status_, sizeof(value.status) - 1);

    return value;
}

bool OnlinePredictionClient::requestPrediction(
    const SondeTelemetry& telemetry,
    const NavigationInfo& navigation
) {
    lastAttemptMs_ = millis();
    requestInProgress_ = true;
    setStatus("requesting SondeHub");

    String url = String(SONDEDECK_SONDEHUB_PREDICTIONS_URL);

    if (url.indexOf('?') < 0) {
        url += '?';
    } else if (!url.endsWith("&") && !url.endsWith("?")) {
        url += '&';
    }

    url += "vehicles=" + urlEncode(telemetry.serial);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(2500);
    http.setTimeout(5000);
    http.setUserAgent("SondeDeck/0.7");

    if (!http.begin(client, url)) {
        requestInProgress_ = false;
        lastRequestOk_ = false;
        setStatus("HTTP begin failed");
        return false;
    }

    const int code = http.GET();

    if (code != 200) {
        http.end();
        requestInProgress_ = false;
        lastRequestOk_ = false;

        char buffer[40];
        snprintf(buffer, sizeof(buffer), "SondeHub HTTP %d", code);
        setStatus(buffer);
        return false;
    }

    const String response = http.getString();
    http.end();

    const bool parsed = parseResponse(response, telemetry, navigation);

    requestInProgress_ = false;
    lastRequestOk_ = parsed;

    if (parsed) {
        lastSuccessMs_ = millis();
    }

    return parsed;
}

bool OnlinePredictionClient::parseResponse(
    const String& response,
    const SondeTelemetry& telemetry,
    const NavigationInfo& navigation
) {
    String trimmed = response;
    trimmed.trim();

    if (trimmed.length() == 0 || trimmed == "[]") {
        available_ = false;
        setStatus("no SondeHub prediction");
        return false;
    }

    const String vehicle = jsonStringValue(trimmed, "vehicle");
    const String time = jsonStringValue(trimmed, "time");
    const String latText = jsonRawValue(trimmed, "latitude");
    const String lonText = jsonRawValue(trimmed, "longitude");
    const String altText = jsonRawValue(trimmed, "altitude");

    const double lat = latText.toDouble();
    const double lon = lonText.toDouble();

    if (!isFiniteLatLon(lat, lon)) {
        available_ = false;
        setStatus("invalid SondeHub lat/lon");
        return false;
    }

    latitude_ = lat;
    longitude_ = lon;
    altitudeMetres_ = altText.length() > 0 ? altText.toDouble() : NAN;
    landed_ = jsonBoolValue(trimmed, "landed", false);
    etaSeconds_ = -1;

    safeCopy(vehicle_, sizeof(vehicle_), vehicle.length() ? vehicle.c_str() : telemetry.serial);
    safeCopy(source_, sizeof(source_), "SondeHub");
    safeCopy(predictionTime_, sizeof(predictionTime_), time.c_str());
    safeCopy(status_, sizeof(status_), landed_ ? "SondeHub landed prediction" : "SondeHub prediction valid");

    available_ = true;

    calculateTargetNavigation(navigation);
    return true;
}

void OnlinePredictionClient::calculateTargetNavigation(
    const NavigationInfo& navigation
) {
    targetNavValid_ = false;
    targetRangeMetres_ = NAN;
    targetBearingDegrees_ = NAN;

    if (!available_ ||
        !navigation.localFixValid ||
        !isfinite(navigation.localLatitude) ||
        !isfinite(navigation.localLongitude) ||
        !isFiniteLatLon(latitude_, longitude_)) {
        return;
    }

    targetRangeMetres_ = haversineDistanceMetres(
        navigation.localLatitude,
        navigation.localLongitude,
        latitude_,
        longitude_
    );

    targetBearingDegrees_ = initialBearingDegrees(
        navigation.localLatitude,
        navigation.localLongitude,
        latitude_,
        longitude_
    );

    targetNavValid_ =
        isfinite(targetRangeMetres_) &&
        isfinite(targetBearingDegrees_);
}

void OnlinePredictionClient::clearResult() {
    available_ = false;
    lastRequestOk_ = false;
    requestInProgress_ = false;

    latitude_ = NAN;
    longitude_ = NAN;
    altitudeMetres_ = NAN;
    landed_ = false;
    etaSeconds_ = -1;

    targetNavValid_ = false;
    targetRangeMetres_ = NAN;
    targetBearingDegrees_ = NAN;

    vehicle_[0] = '\0';
    safeCopy(source_, sizeof(source_), "SondeHub");
    predictionTime_[0] = '\0';
    status_[0] = '\0';
}

void OnlinePredictionClient::setStatus(const char* status) {
    safeCopy(status_, sizeof(status_), status);
}

void OnlinePredictionClient::safeCopy(
    char* destination,
    size_t destinationLength,
    const char* source
) {
    if (destinationLength == 0) {
        return;
    }

    if (source == nullptr) {
        source = "";
    }

    strncpy(destination, source, destinationLength - 1);
    destination[destinationLength - 1] = '\0';
}
