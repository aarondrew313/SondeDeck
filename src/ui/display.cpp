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
DisplayPage currentPage = DisplayPage::Home;
bool pageFullRedraw = true;

struct TextCacheEntry {
    bool used = false;
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
    uint8_t size = 1;
    uint16_t colour = 0;
    char text[96] = {};
};

struct HomeTileCacheEntry {
    bool used = false;
    bool selected = false;
    uint8_t markerState = 0;
    uint16_t outline = 0;
    uint16_t textColour = 0;
};

constexpr uint8_t TEXT_CACHE_SIZE = 96;
TextCacheEntry textCache[TEXT_CACHE_SIZE];
HomeTileCacheEntry homeTileCache[8];

enum class OverviewLayoutState {
    Unknown,
    WaitingForSonde,
    SondeNoGps,
    SondeGps
};

OverviewLayoutState overviewLayoutState = OverviewLayoutState::Unknown;

void clearTextCache() {
    for (uint8_t i = 0; i < TEXT_CACHE_SIZE; ++i) {
        textCache[i].used = false;
        textCache[i].text[0] = '\0';
    }
}

void clearHomeTileCache() {
    for (uint8_t i = 0; i < 8; ++i) {
        homeTileCache[i].used = false;
    }
}

bool cachedFieldUnchanged(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char* text,
    uint16_t colour,
    uint8_t size
) {
    TextCacheEntry* freeEntry = nullptr;

    for (uint8_t i = 0; i < TEXT_CACHE_SIZE; ++i) {
        TextCacheEntry& entry = textCache[i];

        if (!entry.used) {
            if (freeEntry == nullptr) {
                freeEntry = &entry;
            }

            continue;
        }

        if (entry.x == x &&
            entry.y == y &&
            entry.width == width &&
            entry.height == height &&
            entry.size == size) {
            const bool unchanged =
                !pageFullRedraw &&
                entry.colour == colour &&
                strncmp(entry.text, text, sizeof(entry.text)) == 0;

            entry.colour = colour;
            strncpy(entry.text, text, sizeof(entry.text) - 1);
            entry.text[sizeof(entry.text) - 1] = '\0';

            return unchanged;
        }
    }

    TextCacheEntry& entry = freeEntry != nullptr ? *freeEntry : textCache[0];
    entry.used = true;
    entry.x = x;
    entry.y = y;
    entry.width = width;
    entry.height = height;
    entry.size = size;
    entry.colour = colour;
    strncpy(entry.text, text, sizeof(entry.text) - 1);
    entry.text[sizeof(entry.text) - 1] = '\0';

    return false;
}

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

void centreTextInRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1
) {
    setText(size);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(colour, TFT_BLACK);
    tft.drawString(text, x + width / 2, y + height / 2);
    tft.setTextDatum(TL_DATUM);
}

void clearScreen() {
    clearTextCache();
    clearHomeTileCache();
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
    const char* safeText = text != nullptr ? text : "";

    if (cachedFieldUnchanged(x, y, width, height, safeText, colour, size)) {
        return;
    }

    // Always clear the complete assigned field before drawing the new value.
    // This prevents shorter updates leaving old characters behind anywhere
    // dynamic text is used.
    clearField(x, y, width, height);

    setText(size);
    tft.setTextColor(colour, TFT_BLACK);
    tft.setTextPadding(width);
    tft.drawString(safeText, x, y);
    tft.setTextPadding(0);
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
    const int16_t left = x - width;

    if (cachedFieldUnchanged(left, y, width, height, text, colour, size)) {
        return;
    }

    clearField(left, y, width, height);
    rightTextAt(x, y, text, colour, size);
}

void lineText(
    int16_t y,
    const char* text,
    uint16_t colour = TFT_WHITE,
    uint8_t size = 1,
    int16_t x = 10,
    int16_t width = SCREEN_W - 20
) {
    const int16_t height = static_cast<int16_t>(size * 8 + 6);
    fieldText(x, y, width, height, text, colour, size);
}

void blankLine(
    int16_t y,
    uint8_t size = 1,
    int16_t x = 10,
    int16_t width = SCREEN_W - 20
) {
    const int16_t height = static_cast<int16_t>(size * 8 + 6);
    clearField(x, y, width, height);
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
        case DisplayPage::Home:
            return "Home";
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
            return "Settings";
        case DisplayPage::Frequency:
            return "Frequency";
        case DisplayPage::Online:
            return "Online";
        case DisplayPage::Help:
            return "Help";
        case DisplayPage::HelpStatus:
            return "Icons";
        case DisplayPage::About:
            return "About";
        default:
            return VersionInfo::APP_NAME;
    }
}

void drawStatusField(
    int16_t x,
    int16_t y,
    int16_t width,
    const char* text,
    uint16_t colour
) {
    char padded[5] = "   ";

    if (text != nullptr) {
        snprintf(padded, sizeof(padded), "%-3.3s", text);
    }

    if (cachedFieldUnchanged(x, y, width, 13, padded, colour, 1)) {
        return;
    }

    // Status slots are intentionally fixed-width and padded. Clear the whole
    // slot before every changed value so transitions like S-- -> S+ cannot
    // leave the old trailing dash behind.
    clearField(x, y, width, 13);

    setText(1);
    tft.setTextColor(colour, TFT_BLACK);
    tft.setTextPadding(width);
    tft.drawString(padded, x, y);
    tft.setTextPadding(0);
}

bool sondeGpsAvailable(const AppStatus& status) {
    return status.counters.gpsFrames > 0 && !status.signalLost;
}

bool sondeFrameAvailable(const AppStatus& status) {
    return status.counters.validFrames > 0 && !status.signalLost;
}

bool frequencyLockedOrValid(const AppStatus& status) {
    // If we are receiving valid sonde frames on the current tuned frequency,
    // the frequency is effectively valid even if the frequency manager has
    // not explicitly latched its locked flag yet.
    return status.frequency.locked || sondeFrameAvailable(status);
}

bool sdErrorState(const AppStatus& status) {
    return !status.logger.available || status.logger.lastError[0] != '\0';
}

void drawTopStatus(
    const char* gpsStatus,
    const AppStatus& status
) {
    (void)gpsStatus;

    char buffer[12];

    // Keep this bar simple: each Home tile that has a dot gets one matching
    // top-bar slot. Settings has no dot and therefore no top-bar state.

    // Local GPS: G12/G06/G../G--
    if (status.localGpsFix) {
        snprintf(buffer, sizeof(buffer), "G%02u", status.localGpsSats);
        drawStatusField(96, 6, 28, buffer, green());
    } else if (status.localGpsPassed > 0) {
        drawStatusField(96, 6, 28, "G..", amber());
    } else {
        drawStatusField(96, 6, 28, "G--", red());
    }

    // Sonde: S+ full GPS, S? frame without GPS, S-- none.
    const bool sondeGps = sondeGpsAvailable(status);
    const bool sondeSeen = status.counters.validFrames > 0;

    if (sondeGps) {
        drawStatusField(126, 6, 24, "S+", green());
    } else if (sondeSeen) {
        drawStatusField(126, 6, 24, "S?", amber());
    } else {
        drawStatusField(126, 6, 24, "S--", red());
    }

    // Recovery: needs both local GPS and sonde GPS.
    if (status.localGpsFix && sondeGps) {
        drawStatusField(152, 6, 24, "R+", green());
    } else if (status.localGpsFix || sondeGps) {
        drawStatusField(152, 6, 24, "R?", amber());
    } else {
        drawStatusField(152, 6, 24, "R--", red());
    }

    // Logging / SD.
    if (status.logger.enabled) {
        drawStatusField(178, 6, 30, "LOG", green());
    } else if (!sdErrorState(status)) {
        drawStatusField(178, 6, 30, "SD", amber());
    } else {
        drawStatusField(178, 6, 30, "SD!", red());
    }

    // Frequency: locked/valid, scanning, or nothing heard on fixed frequency.
    if (frequencyLockedOrValid(status)) {
        drawStatusField(210, 6, 30, "LCK", green());
    } else if (status.frequency.scanEnabled) {
        drawStatusField(210, 6, 30, "SCN", amber());
    } else {
        drawStatusField(210, 6, 30, "F--", red());
    }

    // Online: connected, configured but offline, or not configured.
    if (status.network.connected) {
        drawStatusField(240, 6, 26, "ONL", green());
    } else if (status.network.configured) {
        drawStatusField(240, 6, 26, "W..", amber());
    } else {
        drawStatusField(240, 6, 26, "W--", red());
    }

    if (status.battery.percent < 0) {
        snprintf(buffer, sizeof(buffer), "--%%");
    } else {
        snprintf(buffer, sizeof(buffer), "%d%%", status.battery.percent);
    }

    fieldRightText(
        SCREEN_W - 6,
        6,
        28,
        13,
        buffer,
        batteryColour(status.battery.percent),
        1
    );

    static int cachedBatteryPercent = -999;
    static bool cachedExternalPower = false;

    if (pageFullRedraw ||
        cachedBatteryPercent != status.battery.percent ||
        cachedExternalPower != status.battery.externalPowerLikely) {
        clearField(262, 4, 30, 16);
        drawBatteryIcon(266, 6, status.battery);
        cachedBatteryPercent = status.battery.percent;
        cachedExternalPower = status.battery.externalPowerLikely;
    }
}

void drawTopBar(
    DisplayPage page,
    const char* gpsStatus,
    const AppStatus& status
) {
    if (pageFullRedraw) {
        clearField(0, 0, SCREEN_W, 25);
        tft.drawFastHLine(0, 24, SCREEN_W, green());
    }

    fieldText(6, 6, 88, 13, pageName(page), green(), 1);
    drawTopStatus(gpsStatus, status);
}

void drawFooter(const AppStatus& status) {
    char buffer[64];

    if (pageFullRedraw) {
        clearField(0, SCREEN_H - 25, SCREEN_W, 25);
        tft.drawFastHLine(0, SCREEN_H - 25, SCREEN_W, grey());

        tft.drawRoundRect(244, SCREEN_H - 22, 44, 18, 4, grey());
        centreTextInRect(244, SCREEN_H - 20, 44, 14, "Home", grey(), 1);

        tft.drawRoundRect(294, SCREEN_H - 22, 20, 18, 4, green());
        centreTextInRect(294, SCREEN_H - 20, 20, 14, "?", green(), 1);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "OK %lu GPS %lu BAD %lu PK %d",
        static_cast<unsigned long>(status.counters.validFrames),
        static_cast<unsigned long>(status.counters.gpsFrames),
        static_cast<unsigned long>(status.counters.rejectedFrames),
        status.counters.peakRssiDbm
    );

    fieldText(6, SCREEN_H - 17, 232, 13, buffer, TFT_WHITE, 1);
}

void preparePage(
    DisplayPage page,
    const char* gpsStatus,
    const AppStatus& status
) {
    pageFullRedraw =
        currentScreen != ScreenMode::Page ||
        currentPage != page;

    if (pageFullRedraw) {
        clearScreen();
        currentScreen = ScreenMode::Page;
        currentPage = page;
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
    fieldText(x, y, 32, 13, label, green(), 1);
    fieldText(x + 34, y, 126, 13, value, TFT_WHITE, 1);
}

void drawNoSondeYet(const NavigationInfo& navigation, const AppStatus& status) {
    char buffer[64];
    char frequency[24];

    lineText(52, "Waiting for RS41 frames", TFT_WHITE, 2);

    formatFrequency(frequency, sizeof(frequency), status.frequency.currentFrequencyHz);

    snprintf(
        buffer,
        sizeof(buffer),
        "%s  %s",
        frequency,
        status.frequency.mode
    );
    lineText(88, buffer, status.frequency.scanEnabled ? amber() : grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Preset %u/%u",
        static_cast<unsigned>(status.frequency.presetIndex + 1),
        static_cast<unsigned>(status.frequency.presetCount)
    );
    lineText(108, buffer, grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Local GPS: %s  sats %u",
        navigation.localFixValid ? "OK" : "WAIT",
        navigation.localSatellites
    );
    lineText(136, buffer, navigation.localFixValid ? green() : amber(), 1);

    if (status.frequency.scanEnabled) {
        lineText(164, "Scanning: S stops, Z/X steps", amber(), 1);
    } else {
        lineText(164, "F Frequency page, S scan", grey(), 1);
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

    lineText(38, telemetry->serial, green(), 2, 10, 160);

    snprintf(buffer, sizeof(buffer), "%d dBm", telemetry->rssiDbm);
    fieldRightText(SCREEN_W - 10, 38, 120, 22, buffer, TFT_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "Frame %u", telemetry->frameNumber);
    lineText(66, buffer, TFT_WHITE, 1, 10, 120);

    snprintf(
        buffer,
        sizeof(buffer),
        "%u sat  pDOP %.1f",
        telemetry->satellites,
        telemetry->positionDop
    );
    fieldRightText(SCREEN_W - 10, 66, 130, 13, buffer, TFT_WHITE, 1);

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
        lineText(98, "Valid RS41 frame", TFT_WHITE, 1);
        lineText(118, "Waiting for sonde GPS fix", amber(), 1);
    }

    if (status.signalLost) {
        snprintf(
            buffer,
            sizeof(buffer),
            "TARGET: LAST SEEN  %.0fs ago",
            status.msSinceLastValidFrame / 1000.0
        );
        lineText(162, buffer, amber(), 1);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "TARGET: LIVE  GPS1 %s GPS3 %s FEC %u",
            telemetry->gps1CrcValid ? "OK" : "NO",
            telemetry->gps3CrcValid ? "OK" : "NO",
            telemetry->correctedErrors
        );
        lineText(162, buffer, green(), 1);
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
        lineText(184, buffer, green(), 1);
    } else if (!navigation.localFixValid) {
        snprintf(
            buffer,
            sizeof(buffer),
            "LOCAL GPS WAIT  sats %u",
            navigation.localSatellites
        );
        lineText(184, buffer, amber(), 1);
    } else {
        lineText(184, "NAV WAIT: sonde GPS", amber(), 1);
    }
}

void drawSondePage(
    const SondeTelemetry* telemetry,
    bool gpsPositionUsable,
    const AppStatus& status
) {
    char buffer[80];

    if (telemetry == nullptr) {
        lineText(56, "No sonde frame yet", amber(), 2);
        blankLine(92);
        blankLine(110);
        blankLine(128);
        blankLine(146);
        blankLine(164);
        blankLine(182);
        return;
    }

    snprintf(buffer, sizeof(buffer), "Serial: %s", telemetry->serial);
    lineText(38, buffer, green(), 1);

    snprintf(buffer, sizeof(buffer), "Frame:  %u", telemetry->frameNumber);
    lineText(56, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "RSSI:   %d dBm   peak %d",
        telemetry->rssiDbm,
        status.counters.peakRssiDbm
    );
    lineText(74, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "FEC:    %u byte(s)",
        telemetry->correctedErrors
    );
    lineText(92, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "GPS1:   %s   GPS3: %s",
        telemetry->gps1CrcValid ? "OK" : "NO",
        telemetry->gps3CrcValid ? "OK" : "NO"
    );
    lineText(110, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sats:   %u   pDOP %.1f   sAcc %.1f",
        telemetry->satellites,
        telemetry->positionDop,
        telemetry->speedAccuracyMps
    );
    lineText(128, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "GPS:    week %u tow %lu",
        telemetry->gpsWeek,
        static_cast<unsigned long>(telemetry->gpsTowMs)
    );
    lineText(146, buffer, TFT_WHITE, 1);

    if (gpsPositionUsable) {
        snprintf(
            buffer,
            sizeof(buffer),
            "LLA: %.5f %.5f %.1fm",
            telemetry->latitude,
            telemetry->longitude,
            telemetry->altitudeMetres
        );
        lineText(164, buffer, green(), 1);
    } else {
        lineText(164, "Position: no current GPS fix", amber(), 1);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "V/H/HDG: %+.1f  %.1f  %.0f",
        telemetry->verticalSpeedMps,
        telemetry->horizontalSpeedMps,
        telemetry->headingDegrees
    );
    lineText(182, buffer, TFT_WHITE, 1);
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
        lineText(42, buffer, amber(), 2);

        snprintf(
            buffer,
            sizeof(buffer),
            "Sats %u  hDOP %.1f",
            navigation.localSatellites,
            navigation.localHdop
        );
        lineText(76, buffer, TFT_WHITE, 1);

        lineText(104, "Go outside or wait for GNSS lock.", grey(), 1);
        blankLine(126, 2);
        blankLine(160);
        blankLine(180);
        return;
    }

    if (telemetry == nullptr || !navigation.sondeFixValid) {
        lineText(42, "Waiting for sonde GPS", amber(), 2);
        lineText(76, "No recovery target yet.", grey(), 1);
        blankLine(96, 2);
        blankLine(126, 2);
        blankLine(160);
        blankLine(180);
        return;
    }

    if (!navigation.navValid) {
        lineText(42, "Recovery not valid yet", amber(), 2);
        blankLine(76);
        blankLine(96, 2);
        blankLine(126, 2);
        blankLine(160);
        blankLine(180);
        return;
    }

    if (status.signalLost) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Target: LAST SEEN  %.0fs ago",
            status.msSinceLastValidFrame / 1000.0
        );
        lineText(36, buffer, amber(), 1);
    } else {
        lineText(36, "Target: LIVE", green(), 1);
    }

    formatDistance(range, sizeof(range), navigation.distanceMetres);
    snprintf(buffer, sizeof(buffer), "RNG %s", range);
    lineText(58, buffer, green(), 3);

    snprintf(buffer, sizeof(buffer), "BRG %.0f true", navigation.bearingDegrees);
    lineText(96, buffer, TFT_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "ELE %+.1f deg", navigation.elevationDegrees);
    lineText(126, buffer, TFT_WHITE, 2);

    formatDistance(range, sizeof(range), navigation.straightLineMetres);
    snprintf(buffer, sizeof(buffer), "Line %s   Rel alt %+.1f m", range, navigation.relativeAltitudeMetres);
    lineText(160, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Local GPS OK  sats %u  hDOP %.1f",
        navigation.localSatellites,
        navigation.localHdop
    );
    lineText(180, buffer, grey(), 1);
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
    lineText(38, buffer, navigation.localFixValid ? green() : amber(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sats: %u   hDOP %.1f",
        navigation.localSatellites,
        navigation.localHdop
    );
    lineText(72, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Age:  %lu ms",
        static_cast<unsigned long>(navigation.localFixAgeMs)
    );
    lineText(92, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Lat:  %.6f",
        navigation.localLatitude
    );
    lineText(118, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Lon:  %.6f",
        navigation.localLongitude
    );
    lineText(138, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Alt:  %.1f m",
        navigation.localAltitudeMetres
    );
    lineText(158, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "NMEA chars %lu  OK %lu  BAD %lu",
        static_cast<unsigned long>(status.localGpsChars),
        static_cast<unsigned long>(status.localGpsPassed),
        static_cast<unsigned long>(status.localGpsFailed)
    );
    lineText(184, buffer, grey(), 1);
}

void drawLoggingPage(const AppStatus& status) {
    char buffer[96];

    snprintf(
        buffer,
        sizeof(buffer),
        "SD: %s",
        status.logger.available ? "available" : "not found"
    );
    lineText(38, buffer, status.logger.available ? green() : amber(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Logging: %s",
        status.logger.enabled ? "ON" : "OFF"
    );
    lineText(70, buffer, status.logger.enabled ? green() : amber(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Frames this boot: %lu",
        static_cast<unsigned long>(status.logger.framesLogged)
    );
    lineText(88, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Sonde: %s",
        status.logger.activeSerial[0] ? status.logger.activeSerial : "--"
    );
    lineText(106, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Track: %s",
        status.logger.activePath[0] ? status.logger.activePath : "--"
    );
    lineText(124, buffer, TFT_WHITE, 1);

    lineText(146, "History root: /logs/sondes", grey(), 1);
    lineText(164, "L toggles logging", grey(), 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Last write: %s",
        status.logger.lastWriteOk ? "OK" : "idle/failed"
    );
    lineText(182, buffer, status.logger.lastWriteOk ? green() : amber(), 1);

    if (status.logger.lastError[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "Error: %s", status.logger.lastError);
        lineText(200, buffer, amber(), 1);
    } else {
        blankLine(200);
    }
}

void drawPowerPage(const AppStatus& status) {
    char buffer[96];

    fieldText(10, 36, 300, 18, "Display", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Brightness: %-3u%%  B cycle",
        status.screenBrightnessPercent
    );
    fieldText(10, 66, 300, 13, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Auto dim:   %s  P toggle",
        status.dimmingEnabled ? "ON " : "OFF"
    );
    fieldText(10, 86, 300, 13, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Dim timer:  %lus  next %lus",
        static_cast<unsigned long>(status.dimTimeoutSeconds),
        static_cast<unsigned long>(status.secondsUntilDim)
    );
    fieldText(10, 106, 300, 13, buffer, grey(), 1);

    fieldText(10, 130, 300, 18, "Input", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Keys:       %-3u%%  K cycle",
        status.keyboardBrightness
    );
    fieldText(10, 160, 300, 13, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Touch:      %s  D toggle",
        status.touchAvailable && status.touchEnabled ? "ON " : "OFF"
    );
    fieldText(
        10,
        180,
        300,
        13,
        buffer,
        status.touchAvailable && status.touchEnabled ? green() : amber(),
        1
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "Battery: %.2fV %-3d%%",
        status.battery.voltage,
        status.battery.percent
    );
    fieldText(10, 200, 300, 13, buffer, grey(), 1);
}

void drawFrequencyPage(const AppStatus& status) {
    char buffer[96];
    char frequency[24];

    lineText(36, "RS41 Frequency", green(), 2);

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
    lineText(68, buffer, TFT_WHITE, 2);

    const char* modeText =
        frequencyLockedOrValid(status)
            ? "Locked"
            : (status.frequency.scanEnabled ? "Scanning" : "Fixed");

    snprintf(
        buffer,
        sizeof(buffer),
        "Mode:    %-10s",
        modeText
    );
    fieldText(
        10,
        100,
        300,
        13,
        buffer,
        frequencyLockedOrValid(status)
            ? green()
            : (status.frequency.scanEnabled ? amber() : grey()),
        1
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "Preset:  %u/%u",
        static_cast<unsigned>(status.frequency.presetIndex + 1),
        static_cast<unsigned>(status.frequency.presetCount)
    );
    lineText(120, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Dwell:   %.1fs",
        status.frequency.scanDwellMs / 1000.0
    );
    lineText(140, buffer, TFT_WHITE, 1);

    if (frequencyLockedOrValid(status)) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Locked:  %s",
            status.frequency.lockedSerial[0]
                ? status.frequency.lockedSerial
                : (sondeFrameAvailable(status) ? "RS41 frame" : "RS41")
        );
        lineText(160, buffer, green(), 1);
    } else if (status.frequency.scanEnabled) {
        snprintf(
            buffer,
            sizeof(buffer),
            "On freq: %.1fs",
            status.frequency.msOnFrequency / 1000.0
        );
        lineText(160, buffer, amber(), 1);
    } else {
        blankLine(160);
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
        lineText(182, buffer, grey(), 1);
    } else {
        blankLine(182);
    }

    lineText(194, "Z/X freq  S scan", grey(), 1);
    clearField(10, 207, SCREEN_W - 20, 6);
}

void drawOnlinePage(const AppStatus& status) {
    char buffer[96];

    lineText(36, "Wi-Fi", green(), 2);

    if (!status.network.configured) {
        lineText(70, "Status: not configured", amber(), 1);
        lineText(92, "Edit src/config/wifi_config.h", grey(), 1);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "Status: %s",
            status.network.status
        );
        lineText(70, buffer, status.network.connected ? green() : amber(), 1);

        snprintf(
            buffer,
            sizeof(buffer),
            "IP: %s   RSSI: %ld",
            status.network.ipAddress[0] ? status.network.ipAddress : "--",
            static_cast<long>(status.network.rssiDbm)
        );
        lineText(90, buffer, TFT_WHITE, 1);
    }

    lineText(120, "SondeHub prediction", green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "Status: %s",
        status.prediction.status
    );
    lineText(152, buffer, status.prediction.available ? green() : amber(), 1);

    if (!status.prediction.available) {
        lineText(174, "Reads /predictions?vehicles=<serial>", grey(), 1);
        lineText(192, "No SondeDeck upload is performed.", grey(), 1);

        // Keep clearing inside the page body only. The footer divider starts
        // at y=215, so do not use a normal blankLine() here.
        clearField(10, 207, SCREEN_W - 20, 6);
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
    lineText(174, buffer, TFT_WHITE, 1);

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
        lineText(192, buffer, green(), 1);
    } else {
        lineText(192, "Prediction range waits for local GPS.", grey(), 1);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "Time: %s",
        status.prediction.predictionTime[0] ? status.prediction.predictionTime : "--"
    );

    // Compact field kept above the footer bar.
    fieldText(10, 204, SCREEN_W - 20, 9, buffer, grey(), 1);
}


struct HomeTileSpec {
    DisplayPage page;
    const char* label;
    const char* key;
};

const HomeTileSpec homeTiles[] = {
    {DisplayPage::Overview, "Overview", "Q"},
    {DisplayPage::Sonde, "Sonde", "W"},
    {DisplayPage::Navigation, "Recovery", "E"},
    {DisplayPage::LocalGps, "Local GPS", "R"},
    {DisplayPage::Logging, "Logging", "T"},
    {DisplayPage::Power, "Settings", "Y"},
    {DisplayPage::Frequency, "Frequency", "F"},
    {DisplayPage::Online, "Online", "O"},
};

void drawHomeTile(
    uint8_t index,
    uint8_t selectedIndex,
    const AppStatus& status
) {
    constexpr int16_t tileW = 74;
    constexpr int16_t tileH = 70;
    constexpr int16_t startX = 6;
    constexpr int16_t startY = 38;
    constexpr int16_t gapX = 5;
    constexpr int16_t gapY = 9;

    const uint8_t col = index % 4;
    const uint8_t row = index / 4;
    const int16_t x = startX + col * (tileW + gapX);
    const int16_t y = startY + row * (tileH + gapY);

    uint16_t outline = index == selectedIndex ? green() : darkGrey();
    uint16_t text = index == selectedIndex ? green() : TFT_WHITE;

    if (homeTiles[index].page == DisplayPage::Online &&
        status.network.configured &&
        !status.network.connected) {
        outline = index == selectedIndex ? amber() : darkGrey();
    }

    const bool sondeGpsGreen = sondeGpsAvailable(status);
    const bool sondeHeard = status.counters.validFrames > 0;
    const bool recoveryGreen = status.localGpsFix && sondeGpsGreen;
    const bool loggingGreen = status.logger.enabled;
    const bool frequencyGreen = frequencyLockedOrValid(status);
    const bool overviewGreen =
        sondeGpsGreen &&
        recoveryGreen &&
        status.localGpsFix &&
        frequencyGreen;

    uint8_t markerState = 0;
    uint16_t markerColour = green();

    switch (homeTiles[index].page) {
        case DisplayPage::Overview:
            markerState = overviewGreen ? 1 : 2;
            markerColour = overviewGreen ? green() : amber();
            break;

        case DisplayPage::Sonde:
            if (sondeGpsGreen) {
                markerState = 1;
                markerColour = green();
            } else if (sondeHeard) {
                markerState = 2;
                markerColour = amber();
            } else {
                markerState = 3;
                markerColour = red();
            }
            break;

        case DisplayPage::Navigation:
            if (recoveryGreen) {
                markerState = 1;
                markerColour = green();
            } else if (status.localGpsFix || sondeGpsGreen) {
                markerState = 2;
                markerColour = amber();
            } else {
                markerState = 3;
                markerColour = red();
            }
            break;

        case DisplayPage::LocalGps:
            markerState = status.localGpsFix ? 1 : 3;
            markerColour = status.localGpsFix ? green() : red();
            break;

        case DisplayPage::Logging:
            if (status.logger.enabled) {
                markerState = 1;
                markerColour = green();
            } else if (status.logger.available && status.logger.lastError[0] == '\0') {
                markerState = 2;
                markerColour = amber();
            } else {
                markerState = 3;
                markerColour = red();
            }
            break;

        case DisplayPage::Power:
            markerState = 0;
            break;

        case DisplayPage::Frequency:
            if (frequencyLockedOrValid(status)) {
                markerState = 1;
                markerColour = green();
            } else if (status.frequency.scanEnabled) {
                markerState = 2;
                markerColour = amber();
            } else {
                markerState = 3;
                markerColour = red();
            }
            break;

        case DisplayPage::Online:
            if (status.network.connected) {
                markerState = 1;
                markerColour = green();
            } else if (status.network.configured) {
                markerState = 2;
                markerColour = amber();
            } else {
                markerState = 3;
                markerColour = red();
            }
            break;

        default:
            markerState = 0;
            break;
    }

    HomeTileCacheEntry& cache = homeTileCache[index];
    const bool selected = index == selectedIndex;

    if (!pageFullRedraw &&
        cache.used &&
        cache.selected == selected &&
        cache.markerState == markerState &&
        cache.outline == outline &&
        cache.textColour == text) {
        return;
    }

    cache.used = true;
    cache.selected = selected;
    cache.markerState = markerState;
    cache.outline = outline;
    cache.textColour = text;

    tft.fillRect(x, y, tileW, tileH, TFT_BLACK);
    tft.drawRoundRect(x, y, tileW, tileH, 6, outline);

    centreTextInRect(x, y + 13, tileW, 16, homeTiles[index].key, outline, 2);
    centreTextInRect(x, y + 39, tileW, 12, homeTiles[index].label, text, 1);

    if (markerState != 0) {
        tft.fillCircle(x + tileW - 10, y + 10, 3, markerColour);
    }
}

void drawHomePage(
    const AppStatus& status,
    uint8_t selectedIndex
) {
    if (selectedIndex >= 8) {
        selectedIndex = 0;
    }

    for (uint8_t i = 0; i < 8; ++i) {
        drawHomeTile(i, selectedIndex, status);
    }

    fieldText(8, 198, 220, 13, "Tap tile | U/I | Enter", grey(), 1);
}

void drawHelpPage() {
    lineText(42, "Q Overview    W Sonde", TFT_WHITE, 1);
    lineText(60, "E Recovery    R Local GPS", TFT_WHITE, 1);
    lineText(78, "T Logging     Y Settings", TFT_WHITE, 1);
    lineText(96, "F Frequency   O Online", TFT_WHITE, 1);

    lineText(120, "U/I Page      Z/X Frequency", TFT_WHITE, 1);
    lineText(138, "S Scan on/off L Log", TFT_WHITE, 1);
    lineText(156, "B Screen      K Keyboard", TFT_WHITE, 1);
    lineText(174, "P Auto dim    A Reset", TFT_WHITE, 1);
    lineText(192, "D Touch on/off  H Home", TFT_WHITE, 1);
}

void drawHelpStatusPage() {
    lineText(38, "Top bar icons", green(), 2);

    lineText(66, "G12/G06 local GPS sats", TFT_WHITE, 1);
    lineText(84, "G.. GPS data/no fix  G-- none", TFT_WHITE, 1);
    lineText(104, "S+ sonde GPS  S? no GPS", TFT_WHITE, 1);
    lineText(122, "S-- no sonde", TFT_WHITE, 1);
    lineText(140, "R+ recoverable  R? missing one", TFT_WHITE, 1);
    lineText(158, "LOG logging  SD idle  SD! error", TFT_WHITE, 1);
    lineText(176, "LCK valid freq  SCN scan  F-- none", TFT_WHITE, 1);
    lineText(194, "ONL online  W.. waiting  W-- off", TFT_WHITE, 1);
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
    lineText(38, buffer, green(), 2);

    snprintf(
        buffer,
        sizeof(buffer),
        "%s firmware by %s",
        VersionInfo::APP_NAME,
        VersionInfo::AUTHOR
    );
    lineText(66, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Hardware: %s",
        VersionInfo::HARDWARE
    );
    lineText(90, buffer, TFT_WHITE, 1);

    snprintf(
        buffer,
        sizeof(buffer),
        "Radio: %s   Sonde: %s",
        VersionInfo::RADIO,
        VersionInfo::SONDE_SUPPORT
    );
    lineText(108, buffer, TFT_WHITE, 1);

    // Keep About short. Documentation covers the feature list.
    clearField(10, 132, SCREEN_W - 20, 76);
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
    writeBacklight(75);

    tft.init();
    tft.setRotation(1);
    clearScreen();

    ready_ = true;
    currentScreen = ScreenMode::None;
    return true;
}

void SondeDisplay::setBrightnessPercent(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }

    // Re-assert the PWM duty so the visible backlight always matches the
    // setting shown on the Settings page after a B-key step.
    writeBacklight(percent);
    delay(1);
    writeBacklight(percent);
}

void SondeDisplay::resetScreen() {
    if (!ready_) {
        return;
    }

    currentScreen = ScreenMode::None;
    currentPage = DisplayPage::Home;
    overviewLayoutState = OverviewLayoutState::Unknown;
    pageFullRedraw = true;
    clearTextCache();
    clearHomeTileCache();

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

    lineText(70, title, green(), 2);
    lineText(102, "RS41-SG frequency manager", TFT_WHITE, 1);
    lineText(122, status, grey(), 1);

    currentScreen = ScreenMode::Boot;
}

void SondeDisplay::showPage(
    DisplayPage page,
    const SondeTelemetry* telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const AppStatus& status,
    uint8_t homeSelection
) {
    if (!ready_) {
        return;
    }

    const char* gpsStatus =
        gpsPositionUsable ? "GPS OK" : "NO GPS";

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
        case DisplayPage::Home:
            drawHomePage(status, homeSelection);
            break;

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

        case DisplayPage::HelpStatus:
            drawHelpStatusPage();
            break;

        case DisplayPage::About:
            drawAboutPage();
            break;
    }

    pageFullRedraw = false;
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
    }

    drawTopBar(DisplayPage::Overview, "REJECT", status);

    lineText(58, "Decode rejected", amber(), 2);
    lineText(92, reason, TFT_WHITE, 1);

    snprintf(buffer, sizeof(buffer), "RSSI %d dBm", rssiDbm);
    lineText(116, buffer, grey(), 1);

    drawFooter(status);
}
