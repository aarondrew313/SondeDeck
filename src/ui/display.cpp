#include "display.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <TFT_eSPI.h>

#include "../board_pins.h"
#include "../config/version.h"

namespace {
TFT_eSPI tft = TFT_eSPI();

constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;
constexpr int16_t BODY_TOP = 28;
constexpr int16_t BODY_BOTTOM = 213;
constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;
constexpr uint32_t BACKLIGHT_PWM_FREQ = 5000;
constexpr uint8_t BACKLIGHT_PWM_BITS = 8;

enum class ScreenMode {
    None,
    Splash,
    Boot,
    Page,
    Reject
};

ScreenMode currentScreen = ScreenMode::None;
DisplayPage currentPage = DisplayPage::Overview;

enum class OverviewLayoutState {
    Unknown,
    WaitingForSonde,
    SondeNoGps,
    SondeGps
};

OverviewLayoutState overviewLayoutState = OverviewLayoutState::Unknown;

uint16_t green() {
    return tft.color565(0, 255, 102);
}

uint16_t grey() {
    return tft.color565(130, 130, 130);
}

uint16_t darkGrey() {
    return tft.color565(48, 48, 48);
}

uint16_t amber() {
    return tft.color565(255, 180, 0);
}

uint16_t red() {
    return tft.color565(255, 60, 60);
}

uint8_t brightnessToDuty(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }

    return static_cast<uint8_t>((255U * percent) / 100U);
}

void writeBacklight(uint8_t percent) {
    ledcWrite(BACKLIGHT_PWM_CHANNEL, brightnessToDuty(percent));
}

uint16_t batteryColour(int percent) {
    if (percent < 0) {
        return grey();
    }

    if (percent <= 15) {
        return red();
    }

    if (percent <= 30) {
        return amber();
    }

    return green();
}

void setText(uint8_t size = 1) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(1);
    tft.setTextSize(size);
    tft.setTextPadding(0);
    tft.setTextWrap(false, false);
}

void textAt(
    int16_t x,
    int16_t y,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    setText(size);
    tft.setTextColor(colour, TFT_BLACK);
    tft.drawString(text, x, y);
}

void centreTextAt(
    int16_t y,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    setText(size);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(colour, TFT_BLACK);
    tft.drawString(text, SCREEN_W / 2, y);
    tft.setTextDatum(TL_DATUM);
}

void rightTextAt(
    int16_t x,
    int16_t y,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    setText(size);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(colour, TFT_BLACK);
    tft.drawString(text, x, y);
    tft.setTextDatum(TL_DATUM);
}

void clearScreen() {
    tft.resetViewport();
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, TFT_BLACK);
    setText(1);
}

void clearBody() {
    tft.fillRect(0, BODY_TOP, SCREEN_W, BODY_BOTTOM - BODY_TOP, TFT_BLACK);
}

void clearField(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height
) {
    tft.fillRect(x, y, width, height, TFT_BLACK);
}

void fieldText(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    clearField(x, y, width, height);
    textAt(x, y, text, colour, size);
}

void fieldRightText(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    clearField(x - width, y, width, height);
    rightTextAt(x, y, text, colour, size);
}

void fillSlantedBar(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t slant,
    uint16_t fill,
    uint16_t outline
) {
    const int16_t x1 = x + slant;
    const int16_t y1 = y;
    const int16_t x2 = x + slant + width;
    const int16_t y2 = y;
    const int16_t x3 = x + width;
    const int16_t y3 = y + height;
    const int16_t x4 = x;
    const int16_t y4 = y + height;

    tft.fillTriangle(x1, y1, x2, y2, x3, y3, fill);
    tft.fillTriangle(x1, y1, x3, y3, x4, y4, fill);

    tft.drawLine(x1, y1, x2, y2, outline);
    tft.drawLine(x2, y2, x3, y3, outline);
    tft.drawLine(x3, y3, x4, y4, outline);
    tft.drawLine(x4, y4, x1, y1, outline);
}

void drawAnetLogo() {
    fillSlantedBar(117, 34, 28, 58, 14, green(), green());
    fillSlantedBar(157, 34, 28, 58, 14, darkGrey(), grey());
}

void drawBatteryIcon(
    int16_t x,
    int16_t y,
    const BatteryState& battery
) {
    constexpr int16_t w = 22;
    constexpr int16_t h = 10;

    const uint16_t colour = batteryColour(battery.percent);

    tft.drawRect(x, y, w, h, colour);
    tft.drawRect(x + w, y + 3, 2, 4, colour);

    if (battery.percent > 0) {
        const int16_t fillWidth =
            static_cast<int16_t>((w - 4) * battery.percent / 100);

        tft.fillRect(x + 2, y + 2, fillWidth, h - 4, colour);
    }

    if (battery.externalPowerLikely) {
        tft.drawLine(x + 11, y + 1, x + 8, y + 6, TFT_WHITE);
        tft.drawLine(x + 8, y + 6, x + 13, y + 6, TFT_WHITE);
        tft.drawLine(x + 13, y + 6, x + 10, y + h - 1, TFT_WHITE);
    }
}

const char* pageName(DisplayPage page) {
    switch (page) {
        case DisplayPage::Overview:
            return "Overview";
        case DisplayPage::Sonde:
            return "Sonde";
        case DisplayPage::Navigation:
            return "Recovery";
        case DisplayPage::LocalGps:
            return "Local GPS";
        case DisplayPage::Logging:
            return "Logging";
        case DisplayPage::Power:
            return "Power";
        case DisplayPage::Frequency:
            return "Frequency";
        case DisplayPage::Online:
            return "Online";
        case DisplayPage::Help:
            return "Help";
        case DisplayPage::About:
            return "About";
        default:
            return VersionInfo::APP_NAME;
    }
}

void drawTopStatus(
    const char* gpsStatus,
    const AppStatus& status
) {
    char buffer[12];

    clearField(132, 3, 184, 18);

    rightTextAt(180, 6, gpsStatus, TFT_WHITE, 1);

    if (status.logger.enabled) {
        textAt(185, 6, "LOG", green(), 1);
    } else if (status.logger.available) {
        textAt(185, 6, "SD", grey(), 1);
    }

    if (status.network.connected) {
        textAt(214, 6, "NET", green(), 1);
    }

    if (status.displayDimmed) {
        textAt(222, 6, "DIM", amber(), 1);
    } else if (status.battery.externalPowerLikely) {
        textAt(222, 6, "USB", green(), 1);
    }

    drawBatteryIcon(248, 6, status.battery);

    if (status.battery.percent < 0) {
        snprintf(buffer, sizeof(buffer), "--%%");
    } else {
        snprintf(buffer, sizeof(buffer), "%d%%", status.battery.percent);
    }

    rightTextAt(
        SCREEN_W - 6,
        6,
        buffer,
        batteryColour(status.battery.percent),
        1
    );
}

void drawTopBar(
    DisplayPage page,
    const char* gpsStatus,
    const AppStatus& status
) {
    clearField(0, 0, SCREEN_W, 25);

    textAt(6, 6, pageName(page), green(), 1);
    drawTopStatus(gpsStatus, status);
    tft.drawFastHLine(0, 24, SCREEN_W, green());
}

void drawFooter(const AppStatus& status) {
    char buffer[80];

    clearField(0, SCREEN_H - 25, SCREEN_W, 25);
    tft.drawFastHLine(0, SCREEN_H - 25, SCREEN_W, grey());

    snprintf(
        buffer,
        sizeof(buffer),
        "OK %lu GPS %lu BAD %lu ABORT %lu PK %d",
        static_cast<unsigned long>(status.counters.validFrames),
        static_cast<unsigned long>(status.counters.gpsFrames),
        static_cast<unsigned long>(status.counters.rejectedFrames),
        static_cast<unsigned long>(status.counters.radioAbortedFrames),
        status.counters.peakRssiDbm
    );

    textAt(6, SCREEN_H - 17, buffer, TFT_WHITE, 1);
}

void preparePage(
    DisplayPage page,
    const char* gpsStatus,
    const AppStatus& status
) {
    if (currentScreen != ScreenMode::Page || currentPage != page) {
        clearScreen();
        currentScreen = ScreenMode::Page;
        currentPage = page;
    } else {
        clearBody();
    }

    drawTopBar(page, gpsStatus, status);
    drawFooter(status);
}

void formatDouble(
    char* buffer,
    size_t length,
    double value,
    uint8_t decimals,
    const char* suffix = ""
) {
    if (!isfinite(value)) {
        snprintf(buffer, length, "--%s", suffix);
        return;
    }

    char format[16];
    snprintf(
        format,
        sizeof(format),
        "%%.%uf%%s",
        static_cast<unsigned>(decimals)
    );

    snprintf(buffer, length, format, value, suffix);
}

void formatSignedDouble(
    char* buffer,
    size_t length,
    double value,
    uint8_t decimals,
    const char* suffix = ""
) {
    if (!isfinite(value)) {
        snprintf(buffer, length, "--%s", suffix);
        return;
    }

    char format[16];
    snprintf(
        format,
        sizeof(format),
        "%%+.%uf%%s",
        static_cast<unsigned>(decimals)
    );

    snprintf(buffer, length, format, value, suffix);
}

void formatDistance(
    char* buffer,
    size_t length,
    double metres
) {
    if (!isfinite(metres)) {
        snprintf(buffer, length, "--");
        return;
    }

    if (metres < 1000.0) {
        snprintf(buffer, length, "%.0fm", metres);
    } else {
        snprintf(buffer, length, "%.2fkm", metres / 1000.0);
    }
}

void formatFrequency(
    char* buffer,
    size_t length,
    uint32_t frequencyHz
) {
    snprintf(buffer, length, "%.3f MHz", frequencyHz / 1000000.0);
}

void labelValue(
    int16_t x,
    int16_t y,
    const char* label,
    const char* value
) {
    textAt(x, y, label, green(), 1);
    textAt(x + 34, y, value, TFT_WHITE, 1);
}

void drawNoSondeYet(const NavigationInfo& navigation, const AppStatus& status) {
    char buffer[64];
    char frequency[24];

    textAt(10, 52, "Waiting for RS41 frames", TFT_WHITE, 2);

    formatFrequency(frequency, sizeof(frequency), status.frequency.currentFrequencyHz);

    snprintf(
        buffer,
        sizeof(buffer),
        "%s  %s",
        frequency,
        status.frequency.mode
    );
    textAt(10, 88, buffer, status.frequency.scanEnabled ? amber() : grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Preset %u/%u",
        static_cast<unsigned>(status.frequency.presetIndex + 1),
        static_cast<unsigned>(status.frequency.presetCount)
    );
    textAt(10, 108, buffer, grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Local GPS: %s  sats %u",
        navigation.localFixValid ? "OK" : "WAIT",
        navigation.localSatellites
    );
    textAt(10, 136, buffer, navigation.localFixValid ? green() : amber(), 1);

    if (status.frequency.scanEnabled) {
        textAt(10, 164, "Scanning: S stops, Z/X steps", amber(), 1);
    } else {
        textAt(10, 164, "F Frequency page, S scan", grey(), 1);
    }
}

void drawOverview(
    const SondeTelemetry* telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const AppStatus& status
) {
    char buffer[80];

    if (telemetry == nullptr) {
        drawNoSondeYet(navigation, status);
        return;
    }

    textAt(10, 38, telemetry->serial, green(), 2);

    snprintf(buffer, sizeof(buffer), "%d dBm", telemetry->rssiDbm);
    rightTextAt(SCREEN_W - 10, 38, buffer, TFT_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "Frame %u", telemetry->frameNumber);
    textAt(10, 66, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "%u sat  pDOP %.1f",
        telemetry->satellites,
        telemetry->positionDop
    );
    rightTextAt(SCREEN_W - 10, 66, buffer, TFT_WHITE, 1);

    tft.drawFastHLine(0, 82, SCREEN_W, grey());

    if (gpsPositionUsable) {
        formatDouble(buffer, sizeof(buffer), telemetry->latitude, 6);
        labelValue(10, 96, "LAT", buffer);

        formatDouble(buffer, sizeof(buffer), telemetry->longitude, 6);
        labelValue(10, 116, "LON", buffer);

        formatDouble(buffer, sizeof(buffer), telemetry->altitudeMetres, 1, "m");
        labelValue(10, 136, "ALT", buffer);

        formatSignedDouble(buffer, sizeof(buffer), telemetry->verticalSpeedMps, 1, "m/s");
        labelValue(174, 96, "V/S", buffer);

        formatDouble(buffer, sizeof(buffer), telemetry->horizontalSpeedMps, 1, "m/s");
        labelValue(174, 116, "H/S", buffer);

        formatDouble(buffer, sizeof(buffer), telemetry->headingDegrees, 0, "deg");
        labelValue(174, 136, "HDG", buffer);
    } else {
        textAt(10, 98, "Valid RS41 frame", TFT_WHITE, 1);
        textAt(10, 118, "Waiting for sonde GPS fix", amber(), 1);
    }

    if (status.signalLost) {
        snprintf(
            buffer,
            sizeof(buffer),
            "TARGET: LAST SEEN  %.0fs ago",
            status.msSinceLastValidFrame / 1000.0
        );
        textAt(10, 162, buffer, amber(), 1);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "TARGET: LIVE  GPS1 %s GPS3 %s FEC %u",
            telemetry->gps1CrcValid ? "OK" : "NO",
            telemetry->gps3CrcValid ? "OK" : "NO",
            telemetry->correctedErrors
        );
        textAt(10, 162, buffer, green(), 1);
    }

    if (navigation.navValid) {
        char range[16];
        formatDistance(range, sizeof(range), navigation.distanceMetres);

        snprintf(
            buffer,
            sizeof(buffer),
            "RNG %s  BRG %.0f true  ELE %+.0f",
            range,
            navigation.bearingDegrees,
            navigation.elevationDegrees
        );
        textAt(10, 184, buffer, green(), 1);
    } else if (!navigation.localFixValid) {
        snprintf(
            buffer,
            sizeof(buffer),
            "LOCAL GPS WAIT  sats %u",
            navigation.localSatellites
        );
        textAt(10, 184, buffer, amber(), 1);
    } else {
        textAt(10, 184, "NAV WAIT: sonde GPS", amber(), 1);
    }
}

void drawSondePage(
    const SondeTelemetry* telemetry,
    bool gpsPositionUsable,
    const AppStatus& status
) {
    char buffer[80];

    if (telemetry == nullptr) {
        textAt(10, 56, "No sonde frame yet", amber(), 2);
        return;
    }

    snprintf(buffer, sizeof(buffer), "Serial: %s", telemetry->serial);
    textAt(10, 38, buffer, green(), 1);

    snprintf(buffer, sizeof(buffer), "Frame:  %u", telemetry->frameNumber);
    textAt(10, 56, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "RSSI:   %d dBm   peak %d",
        telemetry->rssiDbm,
        status.counters.peakRssiDbm
    );
    textAt(10, 74, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "FEC:    %u byte(s)",
        telemetry->correctedErrors
    );
    textAt(10, 92, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "GPS1:   %s   GPS3: %s",
        telemetry->gps1CrcValid ? "OK" : "NO",
        telemetry->gps3CrcValid ? "OK" : "NO"
    );
    textAt(10, 110, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sats:   %u   pDOP %.1f   sAcc %.1f",
        telemetry->satellites,
        telemetry->positionDop,
        telemetry->speedAccuracyMps
    );
    textAt(10, 128, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "GPS:    week %u tow %lu",
        telemetry->gpsWeek,
        static_cast<unsigned long>(telemetry->gpsTowMs)
    );
    textAt(10, 146, buffer, TFT_WHITE, 1);

    if (gpsPositionUsable) {
        snprintf(
            buffer,
            sizeof(buffer),
            "LLA: %.5f %.5f %.1fm",
            telemetry->latitude,
            telemetry->longitude,
            telemetry->altitudeMetres
        );
        textAt(10, 164, buffer, green(), 1);
    } else {
        textAt(10, 164, "Position: no current GPS fix", amber(), 1);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "V/H/HDG: %+.1f  %.1f  %.0f",
        telemetry->verticalSpeedMps,
        telemetry->horizontalSpeedMps,
        telemetry->headingDegrees
    );
    textAt(10, 182, buffer, TFT_WHITE, 1);
}

void drawNavigationPage(
    const SondeTelemetry* telemetry,
    const NavigationInfo& navigation,
    const AppStatus& status
) {
    char buffer[80];
    char range[16];

    if (!navigation.localFixValid) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Local GPS waiting"
        );
        textAt(10, 42, buffer, amber(), 2);

        snprintf(
            buffer,
            sizeof(buffer),
            "Sats %u  hDOP %.1f",
            navigation.localSatellites,
            navigation.localHdop
        );
        textAt(10, 76, buffer, TFT_WHITE, 1);

        textAt(10, 104, "Go outside or wait for GNSS lock.", grey(), 1);
        return;
    }

    if (telemetry == nullptr || !navigation.sondeFixValid) {
        textAt(10, 42, "Waiting for sonde GPS", amber(), 2);
        textAt(10, 76, "No recovery target yet.", grey(), 1);
        return;
    }

    if (!navigation.navValid) {
        textAt(10, 42, "Recovery not valid yet", amber(), 2);
        return;
    }

    if (status.signalLost) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Target: LAST SEEN  %.0fs ago",
            status.msSinceLastValidFrame / 1000.0
        );
        textAt(10, 36, buffer, amber(), 1);
    } else {
        textAt(10, 36, "Target: LIVE", green(), 1);
    }

    formatDistance(range, sizeof(range), navigation.distanceMetres);
    snprintf(buffer, sizeof(buffer), "RNG %s", range);
    textAt(10, 58, buffer, green(), 3);

    snprintf(buffer, sizeof(buffer), "BRG %.0f true", navigation.bearingDegrees);
    textAt(10, 96, buffer, TFT_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "ELE %+.1f deg", navigation.elevationDegrees);
    textAt(10, 126, buffer, TFT_WHITE, 2);

    formatDistance(range, sizeof(range), navigation.straightLineMetres);
    snprintf(buffer, sizeof(buffer), "Line %s   Rel alt %+.1f m", range, navigation.relativeAltitudeMetres);
    textAt(10, 160, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Local GPS OK  sats %u  hDOP %.1f",
        navigation.localSatellites,
        navigation.localHdop
    );
    textAt(10, 180, buffer, grey(), 1);
}

void drawLocalGpsPage(
    const NavigationInfo& navigation,
    const AppStatus& status
) {
    char buffer[80];

    snprintf(
        buffer,
        sizeof(buffer),
        "Fix: %s",
        navigation.localFixValid ? "VALID" : "WAITING"
    );
    textAt(10, 38, buffer, navigation.localFixValid ? green() : amber(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sats: %u   hDOP %.1f",
        navigation.localSatellites,
        navigation.localHdop
    );
    textAt(10, 72, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Age:  %lu ms",
        static_cast<unsigned long>(navigation.localFixAgeMs)
    );
    textAt(10, 92, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Lat:  %.6f",
        navigation.localLatitude
    );
    textAt(10, 118, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Lon:  %.6f",
        navigation.localLongitude
    );
    textAt(10, 138, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Alt:  %.1f m",
        navigation.localAltitudeMetres
    );
    textAt(10, 158, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "NMEA chars %lu  OK %lu  BAD %lu",
        static_cast<unsigned long>(status.localGpsChars),
        static_cast<unsigned long>(status.localGpsPassed),
        static_cast<unsigned long>(status.localGpsFailed)
    );
    textAt(10, 184, buffer, grey(), 1);
}

void drawLoggingPage(const AppStatus& status) {
    char buffer[96];

    snprintf(
        buffer,
        sizeof(buffer),
        "SD: %s",
        status.logger.available ? "available" : "not found"
    );
    textAt(10, 38, buffer, status.logger.available ? green() : amber(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Logging: %s",
        status.logger.enabled ? "ON" : "OFF"
    );
    textAt(10, 70, buffer, status.logger.enabled ? green() : amber(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Frames this boot: %lu",
        static_cast<unsigned long>(status.logger.framesLogged)
    );
    textAt(10, 88, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sonde: %s",
        status.logger.activeSerial[0] ? status.logger.activeSerial : "--"
    );
    textAt(10, 106, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Track: %s",
        status.logger.activePath[0] ? status.logger.activePath : "--"
    );
    textAt(10, 124, buffer, TFT_WHITE, 1);

    textAt(10, 146, "History root: /logs/sondes", grey(), 1);
    textAt(10, 164, "Index: /logs/index.csv", grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Last write: %s",
        status.logger.lastWriteOk ? "OK" : "idle/failed"
    );
    textAt(10, 182, buffer, status.logger.lastWriteOk ? green() : amber(), 1);

    if (status.logger.lastError[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "Error: %s", status.logger.lastError);
        textAt(10, 200, buffer, amber(), 1);
    }
}

void drawPowerPage(const AppStatus& status) {
    char buffer[96];

    fieldText(10, 36, 300, 22, "Display", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Brightness: %-3u%%",
        status.screenBrightnessPercent
    );
    fieldText(10, 70, 250, 14, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Current:    %-3u%% %s",
        status.effectiveScreenBrightnessPercent,
        status.displayDimmed ? "(dimmed)" : ""
    );
    fieldText(10, 88, 300, 14, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Auto dim:   %s  %lus",
        status.dimmingEnabled ? "ON " : "OFF",
        static_cast<unsigned long>(status.dimTimeoutSeconds)
    );
    fieldText(10, 106, 260, 14, buffer, TFT_WHITE, 1);

    if (status.dimmingEnabled && !status.displayDimmed) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Dim in:     %-3lus",
            static_cast<unsigned long>(status.secondsUntilDim)
        );
        fieldText(10, 124, 220, 14, buffer, grey(), 1);
    } else {
        fieldText(10, 124, 220, 14, "Dim in:     --", grey(), 1);
    }

    fieldText(10, 150, 300, 22, "Keyboard", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Brightness: %-3u",
        status.keyboardBrightness
    );
    fieldText(10, 184, 220, 14, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Battery:    %.2fV  %-3d%%",
        status.battery.voltage,
        status.battery.percent
    );
    fieldText(10, 202, 260, 14, buffer, grey(), 1);
}

void drawFrequencyPage(const AppStatus& status) {
    char buffer[96];
    char frequency[24];

    textAt(10, 36, "RS41 Frequency", green(), 2);

    formatFrequency(
        frequency,
        sizeof(frequency),
        status.frequency.currentFrequencyHz
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "Current: %s",
        frequency
    );
    textAt(10, 68, buffer, TFT_WHITE, 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Mode:    %s",
        status.frequency.mode
    );
    textAt(
        10,
        100,
        buffer,
        status.frequency.scanEnabled ? amber() : green(),
        1
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "Preset:  %u/%u",
        static_cast<unsigned>(status.frequency.presetIndex + 1),
        static_cast<unsigned>(status.frequency.presetCount)
    );
    textAt(10, 120, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Dwell:   %.1fs",
        status.frequency.scanDwellMs / 1000.0
    );
    textAt(10, 140, buffer, TFT_WHITE, 1);

    if (status.frequency.locked) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Locked:  %s",
            status.frequency.lockedSerial[0]
                ? status.frequency.lockedSerial
                : "RS41"
        );
        textAt(10, 160, buffer, green(), 1);
    } else if (status.frequency.scanEnabled) {
        snprintf(
            buffer,
            sizeof(buffer),
            "On freq: %.1fs",
            status.frequency.msOnFrequency / 1000.0
        );
        textAt(10, 160, buffer, amber(), 1);
    }

    if (status.frequency.bestFrequencyHz != 0 &&
        status.frequency.bestRssiDbm > -127) {
        char best[24];
        formatFrequency(best, sizeof(best), status.frequency.bestFrequencyHz);

        snprintf(
            buffer,
            sizeof(buffer),
            "Best: %s  %d dBm",
            best,
            status.frequency.bestRssiDbm
        );
        textAt(10, 182, buffer, grey(), 1);
    }

    textAt(10, 206, "Z/X freq  S scan  F page", grey(), 1);
}

void drawOnlinePage(const AppStatus& status) {
    char buffer[96];

    textAt(10, 36, "Wi-Fi", green(), 2);

    if (!status.network.configured) {
        textAt(10, 70, "Status: not configured", amber(), 1);
        textAt(10, 92, "Edit src/config/wifi_config.h", grey(), 1);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "Status: %s",
            status.network.status
        );
        textAt(
            10,
            70,
            buffer,
            status.network.connected ? green() : amber(),
            1
        );

        snprintf(
            buffer,
            sizeof(buffer),
            "IP: %s   RSSI: %ld",
            status.network.ipAddress[0] ? status.network.ipAddress : "--",
            static_cast<long>(status.network.rssiDbm)
        );
        textAt(10, 90, buffer, TFT_WHITE, 1);
    }

    textAt(10, 120, "SondeHub prediction", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Status: %s",
        status.prediction.status
    );
    textAt(
        10,
        152,
        buffer,
        status.prediction.available ? green() : amber(),
        1
    );

    if (!status.prediction.available) {
        textAt(10, 174, "Reads /predictions?vehicles=<serial>", grey(), 1);
        textAt(10, 192, "No SondeDeck upload is performed.", grey(), 1);
        return;
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "%s %.6f, %.6f",
        status.prediction.landed ? "Landed:" : "Pred:",
        status.prediction.latitude,
        status.prediction.longitude
    );
    textAt(10, 174, buffer, TFT_WHITE, 1);

    if (status.prediction.targetNavValid) {
        char range[16];

        if (status.prediction.targetRangeMetres < 1000.0) {
            snprintf(range, sizeof(range), "%.0fm", status.prediction.targetRangeMetres);
        } else {
            snprintf(range, sizeof(range), "%.2fkm", status.prediction.targetRangeMetres / 1000.0);
        }

        snprintf(
            buffer,
            sizeof(buffer),
            "RNG %s  BRG %.0f true",
            range,
            status.prediction.targetBearingDegrees
        );
        textAt(10, 192, buffer, green(), 1);
    } else {
        textAt(10, 192, "Prediction range waits for local GPS.", grey(), 1);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "Time: %s",
        status.prediction.predictionTime[0] ? status.prediction.predictionTime : "--"
    );
    textAt(10, 210, buffer, grey(), 1);
}

void drawHelpPage() {
    textAt(10, 34, "Help", green(), 2);

    textAt(10, 58, "Q Overview    W Sonde", TFT_WHITE, 1);
    textAt(10, 76, "E Recovery    R Local GPS", TFT_WHITE, 1);
    textAt(10, 94, "T Logging     Y Power", TFT_WHITE, 1);
    textAt(10, 112, "F Frequency   O Online", TFT_WHITE, 1);

    textAt(10, 136, "U/I Page      Z/X Frequency", TFT_WHITE, 1);
    textAt(10, 154, "S Scan on/off L Log", TFT_WHITE, 1);
    textAt(10, 172, "B Screen      K Keyboard", TFT_WHITE, 1);
    textAt(10, 190, "P Auto dim    A Reset", TFT_WHITE, 1);

    textAt(10, 214, "Space: About", grey(), 1);
}

void drawAboutPage() {
    char buffer[96];

    snprintf(
        buffer,
        sizeof(buffer),
        "%s %s",
        VersionInfo::SPLASH_NAME,
        VersionInfo::VERSION
    );
    textAt(10, 34, buffer, green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "%s firmware by %s",
        VersionInfo::APP_NAME,
        VersionInfo::AUTHOR
    );
    textAt(10, 66, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Hardware: %s",
        VersionInfo::HARDWARE
    );
    textAt(10, 90, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Radio: %s   Sonde: %s",
        VersionInfo::RADIO,
        VersionInfo::SONDE_SUPPORT
    );
    textAt(10, 108, buffer, TFT_WHITE, 1);

    textAt(10, 132, "Features:", green(), 1);
    textAt(10, 150, "Recovery navigation + SD logging", TFT_WHITE, 1);
    textAt(10, 168, "Frequency presets + RS41 scan", TFT_WHITE, 1);
    textAt(10, 186, "SondeHub predictions: read-only", TFT_WHITE, 1);
    textAt(10, 204, "No handheld telemetry uploads", grey(), 1);
    textAt(226, 222, "Space closes", grey(), 1);
}
}

bool SondeDisplay::begin() {
    pinMode(BoardPins::POWER_ENABLE, OUTPUT);
    digitalWrite(BoardPins::POWER_ENABLE, HIGH);
    delay(150);

    pinMode(BoardPins::DISPLAY_CS, OUTPUT);
    pinMode(BoardPins::SD_CS, OUTPUT);
    pinMode(BoardPins::RADIO_NSS, OUTPUT);

    digitalWrite(BoardPins::DISPLAY_CS, HIGH);
    digitalWrite(BoardPins::SD_CS, HIGH);
    digitalWrite(BoardPins::RADIO_NSS, HIGH);

    pinMode(BoardPins::DISPLAY_BACKLIGHT, OUTPUT);
    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_BITS);
    ledcAttachPin(BoardPins::DISPLAY_BACKLIGHT, BACKLIGHT_PWM_CHANNEL);
    writeBacklight(80);

    tft.init();
    tft.setRotation(1);
    clearScreen();

    ready_ = true;
    currentScreen = ScreenMode::None;
    return true;
}

void SondeDisplay::setBrightnessPercent(uint8_t percent) {
    writeBacklight(percent);
}

void SondeDisplay::resetScreen() {
    if (!ready_) {
        return;
    }

    currentScreen = ScreenMode::None;
    currentPage = DisplayPage::Overview;
    overviewLayoutState = OverviewLayoutState::Unknown;

    tft.resetViewport();
    tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
    tft.fillScreen(TFT_BLACK);
    delay(20);
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, TFT_BLACK);
    setText(1);
}

void SondeDisplay::showSplash(bool promptVisible) {
    if (!ready_) {
        return;
    }

    if (currentScreen != ScreenMode::Splash) {
        clearScreen();

        drawAnetLogo();

        // Main splash title. The app name itself is centred on the line;
        // the version is tucked close to the right of it without shifting
        // the name off centre.
        centreTextAt(108, VersionInfo::SPLASH_NAME, green(), 2);
        textAt(218, 116, VersionInfo::VERSION, grey(), 1);

        centreTextAt(140, "Created by A-NET", TFT_WHITE, 1);
        centreTextAt(160, "RS41 recovery receiver", grey(), 1);

        tft.drawFastHLine(58, 184, SCREEN_W - 116, green());

        currentScreen = ScreenMode::Splash;
    }

    clearField(0, 196, SCREEN_W, 16);

    if (promptVisible) {
        centreTextAt(198, "Press SPACE to continue", TFT_WHITE, 1);
    }
}

void SondeDisplay::showBoot(
    const char* title,
    const char* status
) {
    if (!ready_) {
        return;
    }

    clearScreen();

    textAt(10, 70, title, green(), 2);
    textAt(10, 102, "RS41-SG frequency manager", TFT_WHITE, 1);
    textAt(10, 122, status, grey(), 1);

    currentScreen = ScreenMode::Boot;
}

void SondeDisplay::showPage(
    DisplayPage page,
    const SondeTelemetry* telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const AppStatus& status
) {
    if (!ready_) {
        return;
    }

    const char* gpsStatus =
        gpsPositionUsable ? "GPS OK" : "NO GPS";

    if (page == DisplayPage::Help) {
        gpsStatus = "HELP";
    } else if (page == DisplayPage::About) {
        gpsStatus = "ABOUT";
    }

    if (page == DisplayPage::Overview) {
        OverviewLayoutState newOverviewState =
            OverviewLayoutState::WaitingForSonde;

        if (telemetry != nullptr) {
            newOverviewState =
                gpsPositionUsable
                    ? OverviewLayoutState::SondeGps
                    : OverviewLayoutState::SondeNoGps;
        }

        // Overview has different layouts depending on which fields exist:
        // no sonde yet, sonde heard but no sonde GPS, and sonde GPS valid.
        // Force a full redraw whenever the layout changes so old lines
        // cannot remain underneath newer fields.
        if (overviewLayoutState != newOverviewState) {
            currentScreen = ScreenMode::None;
            overviewLayoutState = newOverviewState;
        }
    }

    preparePage(page, gpsStatus, status);

    switch (page) {
        case DisplayPage::Overview:
            drawOverview(telemetry, gpsPositionUsable, navigation, status);
            break;

        case DisplayPage::Sonde:
            drawSondePage(telemetry, gpsPositionUsable, status);
            break;

        case DisplayPage::Navigation:
            drawNavigationPage(telemetry, navigation, status);
            break;

        case DisplayPage::LocalGps:
            drawLocalGpsPage(navigation, status);
            break;

        case DisplayPage::Logging:
            drawLoggingPage(status);
            break;

        case DisplayPage::Power:
            drawPowerPage(status);
            break;

        case DisplayPage::Frequency:
            drawFrequencyPage(status);
            break;

        case DisplayPage::Online:
            drawOnlinePage(status);
            break;

        case DisplayPage::Help:
            drawHelpPage();
            break;

        case DisplayPage::About:
            drawAboutPage();
            break;
    }
}

void SondeDisplay::showDecodeFailure(
    const char* reason,
    int8_t rssiDbm,
    const AppStatus& status
) {
    if (!ready_) {
        return;
    }

    char buffer[64];

    if (currentScreen != ScreenMode::Reject) {
        clearScreen();
        currentScreen = ScreenMode::Reject;
    } else {
        clearBody();
    }

    drawTopBar(DisplayPage::Overview, "REJECT", status);

    textAt(10, 58, "Decode rejected", amber(), 2);
    textAt(10, 92, reason, TFT_WHITE, 1);

    snprintf(buffer, sizeof(buffer), "RSSI %d dBm", rssiDbm);
    textAt(10, 116, buffer, grey(), 1);

    drawFooter(status);
}
