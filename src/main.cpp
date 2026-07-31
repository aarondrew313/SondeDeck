#include <Arduino.h>
#include "config/version.h"
#include <ctype.h>

#include "decoder/rs41_decoder.h"
#include "gps/local_gps.h"
#include "input/keyboard.h"
#include "input/trackball.h"
#include "navigation/chase_math.h"
#include "network/sonde_wifi.h"
#include "power/battery_monitor.h"
#include "power/power_settings.h"
#include "prediction/online_prediction.h"
#include "radio/sx1262_receiver.h"
#include "radio/frequency_manager.h"
#include "storage/sd_logger.h"
#include "ui/display.h"
#include "ui/page_id.h"

namespace {
constexpr uint32_t SIGNAL_LOST_MS = 10000;
constexpr uint32_t IDLE_DISPLAY_REFRESH_MS = 1000;

Sx1262Receiver receiver;
Rs41Decoder decoder;
SondeDisplay display;
TDeckKeyboard keyboard;
TrackballInput trackball;
LocalGps localGps;
SdLogger sdLogger;
PowerSettings powerSettings;
SondeWifi sondeWifi;
OnlinePredictionClient onlinePrediction;
FrequencyManager frequencyManager;

DisplayPage activePage = DisplayPage::Overview;
DisplayPage pageBeforeHelp = DisplayPage::Overview;

SondeTelemetry lastTelemetry;
NavigationInfo lastNavigation;
bool hasLastTelemetry = false;
bool lastGpsUsable = false;

uint32_t validFrames = 0;
uint32_t gpsFrames = 0;
uint32_t rejectedFrames = 0;
uint32_t lastValidFrameMs = 0;
uint32_t lastDisplayRefreshMs = 0;
uint32_t lastUserActivityMs = 0;
char lastKeyboardKey = 0;

int8_t peakRssiDbm = -127;

bool helpOpen();
void refreshDisplay(bool force);
void applyPowerOutputs();
void markUserActivity();
void forceDisplayRefresh();
void tuneReceiverTo(uint32_t frequencyHz);
void updateFrequencyScan();

void waitForSplashContinue() {
    display.showSplash(true);

    while (true) {
        localGps.update();

        if (keyboard.continuePressed()) {
            return;
        }

        while (Serial.available() > 0) {
            const char c = static_cast<char>(Serial.read());
            if (c == ' ' || c == '\r' || c == '\n') {
                return;
            }
        }

        delay(20);
    }
}

void waitForKeyboardRelease() {
    // The T-Deck keyboard MCU can still report SPACE for a short moment after
    // the splash has been dismissed. Without this, the first loop pass can
    // interpret the same SPACE press as "open help", causing stale-looking
    // overlay until the screen is refreshed.
    uint32_t quietSince = millis();

    while (millis() - quietSince < 300) {
        localGps.update();

        const char key = keyboard.readKey();

        if (key != 0) {
            quietSince = millis();
        }

        while (Serial.available() > 0) {
            Serial.read();
            quietSince = millis();
        }

        delay(20);
    }
}

void applyPowerOutputs() {
    display.setBrightnessPercent(powerSettings.effectiveScreenBrightnessPercent());

    if (helpOpen()) {
        keyboard.setBacklight(powerSettings.helpKeyboardBrightness());
    } else if (powerSettings.displayDimmed()) {
        keyboard.setBacklight(0);
    } else {
        keyboard.setBacklight(powerSettings.keyboardBrightness());
    }
}

void markUserActivity() {
    lastUserActivityMs = millis();

    if (powerSettings.setDisplayDimmed(false)) {
        applyPowerOutputs();
    }
}

void forceDisplayRefresh() {
    lastDisplayRefreshMs = 0;
    refreshDisplay(true);
}

void tuneReceiverTo(uint32_t frequencyHz) {
    if (receiver.setFrequencyHz(frequencyHz)) {
        frequencyManager.noteTuned(millis());

        Serial.printf(
            "Frequency selected: %.3f MHz\n",
            frequencyHz / 1000000.0
        );
    } else {
        Serial.printf(
            "Frequency change failed: %.3f MHz\n",
            frequencyHz / 1000000.0
        );
    }
}

void updateFrequencyScan() {
    const uint32_t now = millis();

    if (!frequencyManager.shouldAdvanceScan(receiver.isSearching(), now)) {
        return;
    }

    const uint32_t nextFrequency = frequencyManager.advanceScan(now);
    tuneReceiverTo(nextFrequency);
}

bool helpOpen() {
    return activePage == DisplayPage::Help ||
           activePage == DisplayPage::About;
}

bool aboutOpen() {
    return activePage == DisplayPage::About;
}

void openHelp() {
    if (!helpOpen()) {
        pageBeforeHelp = activePage;
    }

    activePage = DisplayPage::Help;
    applyPowerOutputs();
}

void advanceHelpCycle() {
    if (activePage == DisplayPage::Help) {
        activePage = DisplayPage::About;
    } else if (activePage == DisplayPage::About) {
        activePage = pageBeforeHelp;
    } else {
        pageBeforeHelp = activePage;
        activePage = DisplayPage::Help;
    }

    applyPowerOutputs();
}

void closeHelp() {
    if (helpOpen()) {
        activePage = pageBeforeHelp;
    }

    applyPowerOutputs();
}

void resetCounters() {
    validFrames = 0;
    gpsFrames = 0;
    rejectedFrames = 0;
    peakRssiDbm = -127;
    sdLogger.resetSessionCounter();
}

void goToPage(DisplayPage page) {
    activePage = page;

    if (!helpOpen()) {
        applyPowerOutputs();
    }
}

void handleKey(char key) {
    if (key == 0) {
        return;
    }

    markUserActivity();

    if (key == ' ') {
        advanceHelpCycle();
        forceDisplayRefresh();
        return;
    }

    if (key == 0x1B) {
        closeHelp();
        goToPage(DisplayPage::Overview);
        forceDisplayRefresh();
        return;
    }

    const char upper = static_cast<char>(toupper(static_cast<unsigned char>(key)));

    switch (upper) {
        // Direct page keys across the physical QWERTY top row.
        case 'Q':
            closeHelp();
            goToPage(DisplayPage::Overview);
            break;

        case 'W':
            closeHelp();
            goToPage(DisplayPage::Sonde);
            break;

        case 'E':
            closeHelp();
            goToPage(DisplayPage::Navigation);
            break;

        case 'R':
            closeHelp();
            goToPage(DisplayPage::LocalGps);
            break;

        case 'T':
            closeHelp();
            goToPage(DisplayPage::Logging);
            break;

        case 'Y':
            closeHelp();
            goToPage(DisplayPage::Power);
            break;

        case 'F':
            closeHelp();
            goToPage(DisplayPage::Frequency);
            break;

        case 'O':
            closeHelp();
            goToPage(DisplayPage::Online);
            break;

        // Actions.
        case 'B':
            powerSettings.cycleScreenBrightness();
            applyPowerOutputs();
            break;

        case 'K':
            powerSettings.cycleKeyboardBrightness();
            applyPowerOutputs();
            break;

        case 'P':
            powerSettings.toggleDimmingEnabled();
            applyPowerOutputs();
            break;

        case 'L':
            sdLogger.toggleEnabled();
            break;

        case 'A':
            resetCounters();
            break;

        case 'S':
            frequencyManager.toggleScan();
            tuneReceiverTo(frequencyManager.currentFrequencyHz());
            break;

        case 'Z':
            tuneReceiverTo(frequencyManager.selectPreviousPreset());
            break;

        case 'X':
            tuneReceiverTo(frequencyManager.selectNextPreset());
            break;

        // Simple page left/right on nearby top-row keys.
        case 'I':
        case '.':
        case '>':
        case ']':
            closeHelp();
            goToPage(nextDisplayPage(activePage));
            break;

        case 'U':
        case ',':
        case '<':
        case '[':
            closeHelp();
            goToPage(previousDisplayPage(activePage));
            break;

        default:
            break;
    }

    forceDisplayRefresh();
}

void handleTrackballEvent(TrackballEvent event) {
    if (event == TrackballEvent::None) {
        return;
    }

    markUserActivity();

    if (event == TrackballEvent::Press) {
        advanceHelpCycle();
        forceDisplayRefresh();
    }
}

void processInput() {
    const char key = keyboard.readKey();

    if (key == 0) {
        lastKeyboardKey = 0;
    } else if (key != lastKeyboardKey) {
        lastKeyboardKey = key;
        handleKey(key);
    }

    handleTrackballEvent(trackball.poll());

    while (Serial.available() > 0) {
        handleKey(static_cast<char>(Serial.read()));
    }
}

AppStatus buildStatus() {
    AppStatus status;

    status.activePage = activePage;
    status.counters.validFrames = validFrames;
    status.counters.gpsFrames = gpsFrames;
    status.counters.rejectedFrames = rejectedFrames;
    status.counters.radioAbortedFrames = receiver.abortedFrames();
    status.counters.peakRssiDbm = peakRssiDbm;

    status.logger = sdLogger.status();
    status.battery = BatteryMonitor::read();

    if (hasLastTelemetry) {
        status.msSinceLastValidFrame = millis() - lastValidFrameMs;
        status.signalLost =
            status.msSinceLastValidFrame >= SIGNAL_LOST_MS;
    }

    status.localGpsChars = localGps.charsProcessed();
    status.localGpsPassed = localGps.passedChecksumCount();
    status.localGpsFailed = localGps.failedChecksumCount();

    status.screenBrightnessPercent = powerSettings.screenBrightnessPercent();
    status.effectiveScreenBrightnessPercent =
        powerSettings.effectiveScreenBrightnessPercent();
    status.keyboardBrightness = powerSettings.keyboardBrightness();
    status.displayDimmed = powerSettings.displayDimmed();
    status.dimmingEnabled = powerSettings.dimmingEnabled();
    status.dimTimeoutSeconds = powerSettings.dimTimeoutSeconds();

    const uint32_t now = millis();
    const uint32_t elapsed =
        now >= lastUserActivityMs ? now - lastUserActivityMs : 0;

    if (status.dimmingEnabled &&
        !status.displayDimmed &&
        elapsed < powerSettings.dimTimeoutMs()) {
        status.secondsUntilDim =
            (powerSettings.dimTimeoutMs() - elapsed + 999) / 1000;
    } else {
        status.secondsUntilDim = 0;
    }

    status.network = sondeWifi.status();
    status.prediction = onlinePrediction.info();
    status.frequency = frequencyManager.status(millis());

    return status;
}

void handlePowerDimming() {
    if (helpOpen()) {
        if (powerSettings.setDisplayDimmed(false)) {
            applyPowerOutputs();
            forceDisplayRefresh();
        }

        return;
    }

    const uint32_t now = millis();
    const uint32_t inactiveMs =
        now >= lastUserActivityMs ? now - lastUserActivityMs : 0;

    const bool shouldDim =
        powerSettings.dimmingEnabled() &&
        inactiveMs >= powerSettings.dimTimeoutMs();

    if (powerSettings.setDisplayDimmed(shouldDim)) {
        applyPowerOutputs();
        forceDisplayRefresh();
    }
}

void refreshDisplay(bool force = false) {
    const uint32_t now = millis();

    if (!force &&
        now - lastDisplayRefreshMs < IDLE_DISPLAY_REFRESH_MS) {
        return;
    }

    lastDisplayRefreshMs = now;

    if (hasLastTelemetry) {
        lastNavigation =
            calculateNavigation(localGps, lastTelemetry, lastGpsUsable);
    } else {
        SondeTelemetry blank;
        lastNavigation =
            calculateNavigation(localGps, blank, false);
    }

    const AppStatus status = buildStatus();

    display.showPage(
        activePage,
        hasLastTelemetry ? &lastTelemetry : nullptr,
        lastGpsUsable,
        lastNavigation,
        status
    );
}

void printGps3Raw(const SondeTelemetry& value) {
    Serial.print("GPS3 raw:       ");

    for (uint8_t i = 0; i < value.gps3RawLength; ++i) {
        Serial.printf("%02X", value.gps3Raw[i]);

        if (i + 1 < value.gps3RawLength) {
            Serial.print(' ');
        }
    }

    Serial.println();
}

void printNavigation(const NavigationInfo& navigation) {
    if (!navigation.localFixValid) {
        Serial.printf(
            "Local GPS:      waiting  sats=%u  age=%lu ms\n",
            navigation.localSatellites,
            static_cast<unsigned long>(navigation.localFixAgeMs)
        );
        return;
    }

    Serial.printf(
        "Local GPS:      %.6f, %.6f  alt=%.1f m  sats=%u  hdop=%.1f\n",
        navigation.localLatitude,
        navigation.localLongitude,
        navigation.localAltitudeMetres,
        navigation.localSatellites,
        navigation.localHdop
    );

    if (navigation.navValid) {
        Serial.printf(
            "Navigation:     range=%.1f m  bearing=%.1f deg  elev=%+.1f deg  line=%.1f m\n",
            navigation.distanceMetres,
            navigation.bearingDegrees,
            navigation.elevationDegrees,
            navigation.straightLineMetres
        );
    } else {
        Serial.println("Navigation:     waiting for local and sonde GPS fixes");
    }
}

void printTelemetry(
    const SondeTelemetry& value,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const LoggerStatus& loggerStatus
) {
    Serial.println();

    if (gpsPositionUsable) {
        Serial.println("========== RS41 FRAME VALID ==========");
    } else {
        Serial.println("====== RS41 VALID - GPS NOT FIXED ====");
    }

    Serial.printf("Serial:         %s\n", value.serial);
    Serial.printf("Frame:          %u\n", value.frameNumber);
    Serial.printf("RSSI:           %d dBm\n", value.rssiDbm);
    Serial.printf("Peak RSSI:      %d dBm\n", peakRssiDbm);
    Serial.printf(
        "FEC corrected:  %u byte(s)\n",
        value.correctedErrors
    );

    Serial.printf(
        "GPS1:           seen=%s crc=%s\n",
        value.gps1BlockSeen ? "yes" : "no",
        value.gps1CrcValid ? "OK" : "NO"
    );

    Serial.printf(
        "GPS3:           seen=%s crc=%s sats=%u sAcc=%.1f pDOP=%.1f\n",
        value.gps3BlockSeen ? "yes" : "no",
        value.gps3CrcValid ? "OK" : "NO",
        value.satellites,
        value.speedAccuracyMps,
        value.positionDop
    );

    if (value.gpsInfoValid) {
        Serial.printf("GPS week:       %u\n", value.gpsWeek);
        Serial.printf(
            "GPS TOW:        %lu ms\n",
            static_cast<unsigned long>(value.gpsTowMs)
        );
    } else {
        Serial.printf(
            "GPS time:       not valid yet  week=%u tow=%lu\n",
            value.gpsWeek,
            static_cast<unsigned long>(value.gpsTowMs)
        );
    }

    if (gpsPositionUsable) {
        Serial.printf("Latitude:       %.6f\n", value.latitude);
        Serial.printf("Longitude:      %.6f\n", value.longitude);
        Serial.printf("Altitude:       %.2f m\n", value.altitudeMetres);
        Serial.printf("Horizontal:     %.2f m/s\n", value.horizontalSpeedMps);
        Serial.printf("Vertical:       %+.2f m/s\n", value.verticalSpeedMps);
        Serial.printf("Heading:        %.1f deg\n", value.headingDegrees);
    } else {
        Serial.println("Position:       ignored until GPS3 CRC OK and sats >= 4");
        printGps3Raw(value);
    }

    printNavigation(navigation);

    Serial.printf(
        "SD logging:     available=%u enabled=%u frames=%lu last=%s file=%s\n",
        loggerStatus.available ? 1 : 0,
        loggerStatus.enabled ? 1 : 0,
        static_cast<unsigned long>(loggerStatus.framesLogged),
        loggerStatus.lastWriteOk ? "OK" : "not written",
        loggerStatus.activePath[0] ? loggerStatus.activePath : "--"
    );

    if (loggerStatus.lastError[0] != '\0') {
        Serial.printf("SD error:       %s\n", loggerStatus.lastError);
    }

    Serial.printf(
        "Frequency:      %.3f MHz\n",
        receiver.frequencyHz() / 1000000.0
    );

    Serial.printf(
        "Counters:       valid=%lu gps=%lu rejected=%lu radio-aborted=%lu\n",
        static_cast<unsigned long>(validFrames),
        static_cast<unsigned long>(gpsFrames),
        static_cast<unsigned long>(rejectedFrames),
        static_cast<unsigned long>(receiver.abortedFrames())
    );
    Serial.println("======================================");
    Serial.println();
}

void printDecoderFailure(const Rs41DecodeResult& result) {
    Serial.printf(
        "DECODE REJECTED: %s  RSSI=%d dBm  valid=%lu gps=%lu rejected=%lu\n",
        Rs41Decoder::statusText(result.status),
        result.telemetry.rssiDbm,
        static_cast<unsigned long>(validFrames),
        static_cast<unsigned long>(gpsFrames),
        static_cast<unsigned long>(rejectedFrames)
    );
}
}

void setup() {
    Serial.begin(115200);

    const uint32_t waitStarted = millis();
    while (!Serial && millis() - waitStarted < 5000) {
        delay(10);
    }

    display.begin();
    keyboard.begin();
    trackball.begin();
    powerSettings.begin();
    applyPowerOutputs();
    BatteryMonitor::begin();
    localGps.begin(38400);
    frequencyManager.begin();
    lastUserActivityMs = millis();

    Serial.println();
    Serial.println("========================================");
    Serial.printf(" %s %s - Stage 9 Release Metadata\n", VersionInfo::APP_NAME, VersionInfo::VERSION);
    Serial.println("========================================");
    Serial.println("Press SPACE on the T-Deck keyboard to start.");
    Serial.println("After startup, SPACE cycles Help -> About -> close.");
    Serial.println("Pages: Q/W/E/R/T/Y/F/O. Frequency: Z/X step, S scan.");
    Serial.println("Trackball press cycles Help -> About -> close. Predictions use SondeHub by serial only.");
    Serial.println();

    waitForSplashContinue();
    waitForKeyboardRelease();

    activePage = DisplayPage::Overview;
    pageBeforeHelp = DisplayPage::Overview;
    hasLastTelemetry = false;
    lastGpsUsable = false;
    applyPowerOutputs();

    // Fully blank the display after the splash and do not draw any boot pages
    // on top of it. This avoids retained ST7789 RAM / stale page overlays after
    // soft reset or re-upload.
    display.resetScreen();

    const bool sdOk = sdLogger.begin();

    Serial.printf(
        "SD logger: %s\n",
        sdOk ? "ready" : "not available"
    );

    sondeWifi.begin();
    onlinePrediction.begin();

    const NetworkStatus networkStatus = sondeWifi.status();
    const PredictionInfo predictionInfo = onlinePrediction.info();

    Serial.printf(
        "Wi-Fi: %s\n",
        networkStatus.status
    );

    Serial.printf(
        "Online prediction: %s\n",
        predictionInfo.status
    );

    Serial.println("Starting SX1262 receiver...");

    if (!receiver.begin()) {
        Serial.println("FATAL: radio initialisation failed.");

        while (true) {
            localGps.update();
            delay(1000);
        }
    }

    tuneReceiverTo(frequencyManager.currentFrequencyHz());

    Serial.println();
    Serial.println("RADIO READY");
    Serial.println("Waiting for RS41 frames...");

    activePage = DisplayPage::Overview;
    pageBeforeHelp = DisplayPage::Overview;
    display.resetScreen();
    refreshDisplay(true);
}

void loop() {
    sondeWifi.update();
    localGps.update();
    processInput();
    localGps.update();

    receiver.update();
    updateFrequencyScan();
    localGps.update();

    if (!receiver.frameAvailable()) {
        handlePowerDimming();
        refreshDisplay(false);
        delay(1);
        return;
    }

    const Rs41DecodeResult result = decoder.decode(
        receiver.frameData(),
        receiver.frameLength(),
        receiver.frameRssi()
    );

    if (result.status == Rs41DecodeStatus::Valid ||
        result.status == Rs41DecodeStatus::ValidNoGpsFix) {
        ++validFrames;

        if (result.status == Rs41DecodeStatus::Valid) {
            ++gpsFrames;
        }

        const bool gpsUsable =
            result.status == Rs41DecodeStatus::Valid;

        lastTelemetry = result.telemetry;
        lastGpsUsable = gpsUsable;
        hasLastTelemetry = true;
        lastValidFrameMs = millis();

        if (lastTelemetry.rssiDbm > peakRssiDbm) {
            peakRssiDbm = lastTelemetry.rssiDbm;
        }

        frequencyManager.noteValidFrame(
            lastTelemetry.serial,
            lastTelemetry.rssiDbm,
            millis()
        );

        lastNavigation =
            calculateNavigation(localGps, lastTelemetry, lastGpsUsable);

        const BatteryState battery = BatteryMonitor::read();

        sdLogger.logFrame(
            lastTelemetry,
            lastGpsUsable,
            lastNavigation,
            battery,
            millis()
        );

        onlinePrediction.update(
            lastTelemetry,
            lastGpsUsable,
            sondeWifi.connected(),
            lastNavigation
        );

        const LoggerStatus loggerStatus = sdLogger.status();

        printTelemetry(
            lastTelemetry,
            lastGpsUsable,
            lastNavigation,
            loggerStatus
        );

        const PredictionInfo predictionInfo = onlinePrediction.info();

        if (predictionInfo.configured) {
            Serial.printf(
                "SondeHub prediction: %s",
                predictionInfo.status
            );

            if (predictionInfo.available) {
                Serial.printf(
                    "  target=%.6f,%.6f source=%s time=%s",
                    predictionInfo.latitude,
                    predictionInfo.longitude,
                    predictionInfo.source,
                    predictionInfo.predictionTime
                );

                if (predictionInfo.targetNavValid) {
                    Serial.printf(
                        " range=%.1f m bearing=%.1f",
                        predictionInfo.targetRangeMetres,
                        predictionInfo.targetBearingDegrees
                    );
                }
            }

            Serial.println();
        }

        refreshDisplay(true);
    } else {
        ++rejectedFrames;
        frequencyManager.noteFrameRssi(result.telemetry.rssiDbm);
        printDecoderFailure(result);

        const AppStatus status = buildStatus();

        display.showDecodeFailure(
            Rs41Decoder::statusText(result.status),
            result.telemetry.rssiDbm,
            status
        );
    }

    receiver.releaseFrame();
}
