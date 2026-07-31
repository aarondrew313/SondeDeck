#include "sd_logger.h"

#include <SD.h>
#include <SPI.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "../board_pins.h"

namespace {
constexpr const char* LOG_DIR = "/logs";
constexpr const char* SONDES_DIR = "/logs/sondes";
constexpr const char* INDEX_PATH = "/logs/index.csv";
constexpr const char* LATEST_PATH = "/logs/latest.csv";
constexpr const char* LAST_SEEN_PATH = "/logs/last_seen.txt";

void safeCopy(char* destination, size_t destinationLength, const char* source) {
    if (destinationLength == 0) {
        return;
    }

    strncpy(destination, source, destinationLength - 1);
    destination[destinationLength - 1] = '\0';
}

void printCsvString(File& file, const char* value) {
    file.print('"');

    for (const char* p = value; *p != '\0'; ++p) {
        if (*p == '"') {
            file.print("\"\"");
        } else {
            file.print(*p);
        }
    }

    file.print('"');
}

void printDoubleOrBlank(File& file, double value, uint8_t decimals) {
    if (!isfinite(value)) {
        return;
    }

    file.print(value, decimals);
}

void writeTrackHeader(File& file) {
    file.println(
        "timestamp_ms,"
        "serial,"
        "frame,"
        "rssi_dbm,"
        "sonde_gps_valid,"
        "sonde_lat,"
        "sonde_lon,"
        "sonde_alt_m,"
        "sonde_hspeed_mps,"
        "sonde_vspeed_mps,"
        "sonde_heading_deg,"
        "sonde_sats,"
        "pdop,"
        "gps_week,"
        "gps_tow_ms,"
        "local_fix,"
        "local_lat,"
        "local_lon,"
        "local_alt_m,"
        "local_sats,"
        "range_m,"
        "bearing_deg,"
        "elevation_deg,"
        "straight_line_m,"
        "relative_alt_m,"
        "battery_percent,"
        "battery_voltage"
    );
}

void writeIndexHeader(File& file) {
    file.println(
        "timestamp_ms,"
        "event,"
        "serial,"
        "track_path,"
        "summary_path"
    );
}
}

bool SdLogger::begin() {
    pinMode(BoardPins::SD_CS, OUTPUT);
    pinMode(BoardPins::DISPLAY_CS, OUTPUT);
    pinMode(BoardPins::RADIO_NSS, OUTPUT);

    digitalWrite(BoardPins::SD_CS, HIGH);
    digitalWrite(BoardPins::DISPLAY_CS, HIGH);
    digitalWrite(BoardPins::RADIO_NSS, HIGH);

    SPI.begin(
        BoardPins::SPI_SCK,
        BoardPins::SPI_MISO,
        BoardPins::SPI_MOSI
    );

    available_ = SD.begin(BoardPins::SD_CS, SPI, 25000000);

    if (!available_) {
        enabled_ = false;
        directoriesReady_ = false;
        setError("No SD card");
        return false;
    }

    directoriesReady_ = ensureBaseDirectories();

    if (!directoriesReady_) {
        enabled_ = false;
        setError("Could not create log dirs");
        return false;
    }

    enabled_ = true;
    lastWriteOk_ = true;
    clearError();
    return true;
}

bool SdLogger::available() const {
    return available_;
}

bool SdLogger::enabled() const {
    return enabled_;
}

void SdLogger::setEnabled(bool enabled) {
    if (!available_) {
        enabled_ = false;
        setError("No SD card");
        return;
    }

    if (!directoriesReady_ && !ensureBaseDirectories()) {
        enabled_ = false;
        setError("Could not create log dirs");
        return;
    }

    enabled_ = enabled;

    if (enabled_) {
        clearError();
    }
}

bool SdLogger::toggleEnabled() {
    setEnabled(!enabled_);
    return enabled_;
}

void SdLogger::resetSessionCounter() {
    framesLogged_ = 0;
    activeSondeFrames_ = 0;
}

LoggerStatus SdLogger::status() const {
    LoggerStatus value;
    value.available = available_;
    value.enabled = enabled_;
    value.lastWriteOk = lastWriteOk_;
    value.framesLogged = framesLogged_;

    safeCopy(value.activeSerial, sizeof(value.activeSerial), activeSerial_);
    safeCopy(value.activePath, sizeof(value.activePath), activePath_);
    safeCopy(value.historyRoot, sizeof(value.historyRoot), SONDES_DIR);
    safeCopy(value.lastError, sizeof(value.lastError), lastError_);

    return value;
}

bool SdLogger::logFrame(
    const SondeTelemetry& telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const BatteryState& battery,
    uint32_t timestampMs
) {
    if (!enabled_) {
        return false;
    }

    if (!ensureReady()) {
        return false;
    }

    char serial[12];
    normaliseSerial(telemetry.serial, serial, sizeof(serial));

    char sondeDir[40];
    if (!ensureSondeDirectory(serial, sondeDir, sizeof(sondeDir))) {
        return false;
    }

    char trackPath[64];
    char summaryPath[64];

    snprintf(
        trackPath,
        sizeof(trackPath),
        "%s/track.csv",
        sondeDir
    );

    snprintf(
        summaryPath,
        sizeof(summaryPath),
        "%s/summary.txt",
        sondeDir
    );

    const bool newActiveSerial =
        strncmp(serial, activeSerial_, sizeof(activeSerial_)) != 0;

    if (newActiveSerial) {
        safeCopy(activeSerial_, sizeof(activeSerial_), serial);
        safeCopy(activePath_, sizeof(activePath_), trackPath);
        activeSondeFirstSeenMs_ = timestampMs;
        activeSondeFrames_ = 0;

        noteSondeSeen(serial, trackPath, summaryPath, timestampMs);
    }

    bool ok = appendTrackFrame(
        trackPath,
        telemetry,
        gpsPositionUsable,
        navigation,
        battery,
        timestampMs
    );

    ok = appendTrackFrame(
        LATEST_PATH,
        telemetry,
        gpsPositionUsable,
        navigation,
        battery,
        timestampMs
    ) && ok;

    ok = writeSummary(
        summaryPath,
        telemetry,
        gpsPositionUsable,
        navigation,
        battery,
        timestampMs
    ) && ok;

    ok = writeLastSeen(
        telemetry,
        gpsPositionUsable,
        navigation,
        battery,
        timestampMs
    ) && ok;

    lastWriteOk_ = ok;

    if (ok) {
        ++framesLogged_;
        ++activeSondeFrames_;
        safeCopy(activePath_, sizeof(activePath_), trackPath);
        clearError();
    }

    return ok;
}

bool SdLogger::ensureReady() {
    if (!available_) {
        setError("No SD card");
        return false;
    }

    if (!directoriesReady_) {
        directoriesReady_ = ensureBaseDirectories();
    }

    if (!directoriesReady_) {
        setError("Could not create log dirs");
        return false;
    }

    return true;
}

bool SdLogger::ensureBaseDirectories() {
    if (!SD.exists(LOG_DIR) && !SD.mkdir(LOG_DIR)) {
        return false;
    }

    if (!SD.exists(SONDES_DIR) && !SD.mkdir(SONDES_DIR)) {
        return false;
    }

    if (!SD.exists(INDEX_PATH)) {
        File file = SD.open(INDEX_PATH, FILE_APPEND);

        if (!file) {
            return false;
        }

        writeIndexHeader(file);
        file.close();
    }

    return true;
}

bool SdLogger::ensureSondeDirectory(
    const char* serial,
    char* sondeDir,
    size_t sondeDirLength
) {
    snprintf(
        sondeDir,
        sondeDirLength,
        "%s/%s",
        SONDES_DIR,
        serial
    );

    if (SD.exists(sondeDir)) {
        return true;
    }

    if (!SD.mkdir(sondeDir)) {
        setError("Could not create sonde dir");
        return false;
    }

    return true;
}

bool SdLogger::writeHeaderIfNeeded(const char* path) {
    bool needsHeader = !SD.exists(path);

    if (!needsHeader) {
        File existing = SD.open(path, FILE_READ);

        if (existing) {
            needsHeader = existing.size() == 0;
            existing.close();
        }
    }

    if (!needsHeader) {
        return true;
    }

    File file = SD.open(path, FILE_APPEND);

    if (!file) {
        setError("Could not write CSV header");
        return false;
    }

    writeTrackHeader(file);
    file.close();

    return true;
}

bool SdLogger::appendTrackFrame(
    const char* path,
    const SondeTelemetry& telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const BatteryState& battery,
    uint32_t timestampMs
) {
    if (!writeHeaderIfNeeded(path)) {
        return false;
    }

    File file = SD.open(path, FILE_APPEND);

    if (!file) {
        setError("Could not append track");
        return false;
    }

    char serial[12];
    normaliseSerial(telemetry.serial, serial, sizeof(serial));

    file.print(timestampMs);
    file.print(',');
    printCsvString(file, serial);
    file.print(',');
    file.print(telemetry.frameNumber);
    file.print(',');
    file.print(telemetry.rssiDbm);
    file.print(',');
    file.print(gpsPositionUsable ? 1 : 0);
    file.print(',');
    printDoubleOrBlank(file, telemetry.latitude, 6);
    file.print(',');
    printDoubleOrBlank(file, telemetry.longitude, 6);
    file.print(',');
    printDoubleOrBlank(file, telemetry.altitudeMetres, 2);
    file.print(',');
    printDoubleOrBlank(file, telemetry.horizontalSpeedMps, 2);
    file.print(',');
    printDoubleOrBlank(file, telemetry.verticalSpeedMps, 2);
    file.print(',');
    printDoubleOrBlank(file, telemetry.headingDegrees, 1);
    file.print(',');
    file.print(telemetry.satellites);
    file.print(',');
    printDoubleOrBlank(file, telemetry.positionDop, 1);
    file.print(',');
    file.print(telemetry.gpsWeek);
    file.print(',');
    file.print(telemetry.gpsTowMs);
    file.print(',');
    file.print(navigation.localFixValid ? 1 : 0);
    file.print(',');
    printDoubleOrBlank(file, navigation.localLatitude, 6);
    file.print(',');
    printDoubleOrBlank(file, navigation.localLongitude, 6);
    file.print(',');
    printDoubleOrBlank(file, navigation.localAltitudeMetres, 2);
    file.print(',');
    file.print(navigation.localSatellites);
    file.print(',');
    printDoubleOrBlank(file, navigation.distanceMetres, 1);
    file.print(',');
    printDoubleOrBlank(file, navigation.bearingDegrees, 1);
    file.print(',');
    printDoubleOrBlank(file, navigation.elevationDegrees, 1);
    file.print(',');
    printDoubleOrBlank(file, navigation.straightLineMetres, 1);
    file.print(',');
    printDoubleOrBlank(file, navigation.relativeAltitudeMetres, 1);
    file.print(',');
    file.print(battery.percent);
    file.print(',');
    printDoubleOrBlank(file, battery.voltage, 3);
    file.println();

    file.close();
    return true;
}

bool SdLogger::writeSummary(
    const char* path,
    const SondeTelemetry& telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const BatteryState& battery,
    uint32_t timestampMs
) {
    SD.remove(path);

    File file = SD.open(path, FILE_WRITE);

    if (!file) {
        setError("Could not write summary");
        return false;
    }

    char serial[12];
    normaliseSerial(telemetry.serial, serial, sizeof(serial));

    file.println("SondeDeck sonde summary");
    file.println();

    file.print("serial=");
    file.println(serial);

    file.print("first_seen_ms=");
    file.println(activeSondeFirstSeenMs_);

    file.print("last_seen_ms=");
    file.println(timestampMs);

    file.print("frames_this_session=");
    file.println(activeSondeFrames_ + 1);

    file.print("last_frame=");
    file.println(telemetry.frameNumber);

    file.print("last_rssi_dbm=");
    file.println(telemetry.rssiDbm);

    file.print("sonde_gps_valid=");
    file.println(gpsPositionUsable ? 1 : 0);

    file.print("sonde_lat=");
    file.println(telemetry.latitude, 6);

    file.print("sonde_lon=");
    file.println(telemetry.longitude, 6);

    file.print("sonde_alt_m=");
    file.println(telemetry.altitudeMetres, 2);

    file.print("sonde_hspeed_mps=");
    file.println(telemetry.horizontalSpeedMps, 2);

    file.print("sonde_vspeed_mps=");
    file.println(telemetry.verticalSpeedMps, 2);

    file.print("sonde_heading_deg=");
    file.println(telemetry.headingDegrees, 1);

    file.print("sonde_sats=");
    file.println(telemetry.satellites);

    file.print("local_gps_valid=");
    file.println(navigation.localFixValid ? 1 : 0);

    file.print("local_lat=");
    file.println(navigation.localLatitude, 6);

    file.print("local_lon=");
    file.println(navigation.localLongitude, 6);

    file.print("range_m=");
    file.println(navigation.distanceMetres, 1);

    file.print("bearing_deg=");
    file.println(navigation.bearingDegrees, 1);

    file.print("elevation_deg=");
    file.println(navigation.elevationDegrees, 1);

    file.print("battery_percent=");
    file.println(battery.percent);

    file.print("battery_voltage=");
    file.println(battery.voltage, 3);

    file.close();
    return true;
}

bool SdLogger::writeLastSeen(
    const SondeTelemetry& telemetry,
    bool gpsPositionUsable,
    const NavigationInfo& navigation,
    const BatteryState& battery,
    uint32_t timestampMs
) {
    SD.remove(LAST_SEEN_PATH);

    File file = SD.open(LAST_SEEN_PATH, FILE_WRITE);

    if (!file) {
        setError("Could not write last seen");
        return false;
    }

    char serial[12];
    normaliseSerial(telemetry.serial, serial, sizeof(serial));

    file.println("SondeDeck last seen");
    file.println();

    file.print("timestamp_ms=");
    file.println(timestampMs);

    file.print("serial=");
    file.println(serial);

    file.print("frame=");
    file.println(telemetry.frameNumber);

    file.print("rssi_dbm=");
    file.println(telemetry.rssiDbm);

    file.print("sonde_gps_valid=");
    file.println(gpsPositionUsable ? 1 : 0);

    file.print("sonde_lat=");
    file.println(telemetry.latitude, 6);

    file.print("sonde_lon=");
    file.println(telemetry.longitude, 6);

    file.print("sonde_alt_m=");
    file.println(telemetry.altitudeMetres, 2);

    file.print("local_gps_valid=");
    file.println(navigation.localFixValid ? 1 : 0);

    file.print("local_lat=");
    file.println(navigation.localLatitude, 6);

    file.print("local_lon=");
    file.println(navigation.localLongitude, 6);

    file.print("range_m=");
    file.println(navigation.distanceMetres, 1);

    file.print("bearing_deg=");
    file.println(navigation.bearingDegrees, 1);

    file.print("elevation_deg=");
    file.println(navigation.elevationDegrees, 1);

    file.print("battery_percent=");
    file.println(battery.percent);

    file.close();
    return true;
}

bool SdLogger::noteSondeSeen(
    const char* serial,
    const char* trackPath,
    const char* summaryPath,
    uint32_t timestampMs
) {
    File file = SD.open(INDEX_PATH, FILE_APPEND);

    if (!file) {
        setError("Could not append index");
        return false;
    }

    file.print(timestampMs);
    file.print(',');
    printCsvString(file, "seen");
    file.print(',');
    printCsvString(file, serial);
    file.print(',');
    printCsvString(file, trackPath);
    file.print(',');
    printCsvString(file, summaryPath);
    file.println();

    file.close();
    return true;
}

void SdLogger::normaliseSerial(
    const char* input,
    char* output,
    size_t outputLength
) {
    if (outputLength == 0) {
        return;
    }

    size_t written = 0;

    if (input != nullptr) {
        for (const char* p = input; *p != '\0' && written + 1 < outputLength; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);

            if (isalnum(c) || c == '-' || c == '_') {
                output[written++] = static_cast<char>(c);
            }
        }
    }

    if (written == 0) {
        safeCopy(output, outputLength, "UNKNOWN");
        return;
    }

    output[written] = '\0';
}

void SdLogger::setError(const char* message) {
    safeCopy(lastError_, sizeof(lastError_), message);
    lastWriteOk_ = false;
}

void SdLogger::clearError() {
    lastError_[0] = '\0';
}
