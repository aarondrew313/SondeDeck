#include "rs41_decoder.h"

#include <math.h>
#include <string.h>

#include "rs41_crc.h"
#include "rs41_fec.h"

namespace {
constexpr double PI_D = 3.14159265358979323846;

constexpr uint8_t WHITENING_MASK[64] = {
    0x96, 0x83, 0x3E, 0x51, 0xB1, 0x49, 0x08, 0x98,
    0x32, 0x05, 0x59, 0x0E, 0xF9, 0x44, 0xC6, 0x26,
    0x21, 0x60, 0xC2, 0xEA, 0x79, 0x5D, 0x6D, 0xA1,
    0x54, 0x69, 0x47, 0x0C, 0xDC, 0xE8, 0x5C, 0xF1,
    0xF7, 0x76, 0x82, 0x7F, 0x07, 0x99, 0xA2, 0x2C,
    0x93, 0x7C, 0x30, 0x63, 0xF5, 0x10, 0x2E, 0x61,
    0xD0, 0xBC, 0xB4, 0xB6, 0x06, 0xAA, 0xF4, 0x23,
    0x78, 0x6E, 0x3B, 0xAE, 0xBF, 0x7B, 0x4C, 0xC1
};

constexpr uint8_t DEWHITENED_HEADER[8] = {
    0x86, 0x35, 0xF4, 0x40, 0x93, 0xDF, 0x1A, 0x60
};

constexpr size_t FRAME_LENGTH = 320;
constexpr size_t CAPTURED_LENGTH = 312;

constexpr size_t POS_FRAME_TYPE   = 0x038;
constexpr size_t POS_FRAME_BLOCK  = 0x039;
constexpr size_t POS_FRAME_NUMBER = 0x03B;
constexpr size_t POS_SERIAL       = 0x03D;

constexpr size_t POS_GPS1         = 0x093;
constexpr size_t POS_GPS_WEEK     = 0x095;
constexpr size_t POS_GPS_TOW      = 0x097;
constexpr size_t POS_SATS_LIST    = 0x09B;

constexpr size_t POS_GPS3         = 0x112;
constexpr size_t POS_ECEF_X       = 0x114;
constexpr size_t POS_ECEF_Y       = 0x118;
constexpr size_t POS_ECEF_Z       = 0x11C;
constexpr size_t POS_ECEF_V       = 0x120;
constexpr size_t POS_NUM_SATS     = 0x126;
constexpr size_t POS_SACC         = 0x127;
constexpr size_t POS_PDOP         = 0x128;

constexpr uint8_t EXPECTED_FRAME_TYPE = 0x0F;
constexpr uint8_t EXPECTED_FRAME_ID   = 0x79;
constexpr uint8_t EXPECTED_GPS1_ID    = 0x7C;
constexpr uint8_t EXPECTED_GPS3_ID    = 0x7B;
constexpr uint8_t EXPECTED_GPS3_LEN   = 0x15;

uint16_t readU16(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           (static_cast<uint16_t>(value[1]) << 8);
}

uint32_t readU32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8) |
           (static_cast<uint32_t>(value[2]) << 16) |
           (static_cast<uint32_t>(value[3]) << 24);
}

int16_t readI16(const uint8_t* value) {
    return static_cast<int16_t>(readU16(value));
}

int32_t readI32(const uint8_t* value) {
    return static_cast<int32_t>(readU32(value));
}

void ecefToGeodetic(
    double x,
    double y,
    double z,
    double& latitudeDegrees,
    double& longitudeDegrees,
    double& altitudeMetres
) {
    constexpr double a = 6378137.0;
    constexpr double b = 6356752.31424518;
    constexpr double e2 = (a * a - b * b) / (a * a);
    constexpr double ee2 = (a * a - b * b) / (b * b);

    const double p = sqrt(x * x + y * y);

    if (p < 1e-6 && fabs(z) < 1e-6) {
        latitudeDegrees = NAN;
        longitudeDegrees = NAN;
        altitudeMetres = NAN;
        return;
    }

    const double lambda = atan2(y, x);
    const double theta = atan2(z * a, p * b);

    const double sinTheta = sin(theta);
    const double cosTheta = cos(theta);

    const double phi = atan2(
        z + ee2 * b * sinTheta * sinTheta * sinTheta,
        p - e2 * a * cosTheta * cosTheta * cosTheta
    );

    const double sinPhi = sin(phi);
    const double radius =
        a / sqrt(1.0 - e2 * sinPhi * sinPhi);

    altitudeMetres = p / cos(phi) - radius;
    latitudeDegrees = phi * 180.0 / PI_D;
    longitudeDegrees = lambda * 180.0 / PI_D;
}

void ecefVelocityToLocal(
    double vx,
    double vy,
    double vz,
    double latitudeDegrees,
    double longitudeDegrees,
    double& horizontalSpeed,
    double& heading,
    double& verticalSpeed
) {
    const double latitude = latitudeDegrees * PI_D / 180.0;
    const double longitude = longitudeDegrees * PI_D / 180.0;

    const double sinLat = sin(latitude);
    const double cosLat = cos(latitude);
    const double sinLon = sin(longitude);
    const double cosLon = cos(longitude);

    const double north =
        -vx * sinLat * cosLon
        -vy * sinLat * sinLon
        +vz * cosLat;

    const double east =
        -vx * sinLon
        +vy * cosLon;

    const double up =
        vx * cosLat * cosLon
        +vy * cosLat * sinLon
        +vz * sinLat;

    horizontalSpeed = sqrt(north * north + east * east);
    verticalSpeed = up;

    heading = atan2(east, north) * 180.0 / PI_D;
    if (heading < 0.0) {
        heading += 360.0;
    }
}

bool serialLooksValid(const char* serial) {
    for (int i = 0; i < 8; ++i) {
        const char c = serial[i];

        if (c == '\0') {
            return i > 0;
        }

        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }

    return true;
}

bool positionMathLooksValid(const SondeTelemetry& telemetry) {
    if (!isfinite(telemetry.latitude) ||
        !isfinite(telemetry.longitude) ||
        !isfinite(telemetry.altitudeMetres)) {
        return false;
    }

    if (telemetry.rawEcefXcm == 0 &&
        telemetry.rawEcefYcm == 0 &&
        telemetry.rawEcefZcm == 0) {
        return false;
    }

    return telemetry.latitude >= -90.0 &&
           telemetry.latitude <= 90.0 &&
           telemetry.longitude >= -180.0 &&
           telemetry.longitude <= 180.0 &&
           telemetry.altitudeMetres > -1000.0 &&
           telemetry.altitudeMetres < 80000.0;
}

void decodeFrameIdentity(
    const uint8_t frame[FRAME_LENGTH],
    SondeTelemetry& telemetry
) {
    telemetry.frameNumber = readU16(frame + POS_FRAME_NUMBER);

    memcpy(telemetry.serial, frame + POS_SERIAL, 8);
    telemetry.serial[8] = '\0';

    for (int i = 7; i >= 0; --i) {
        if (telemetry.serial[i] == ' ') {
            telemetry.serial[i] = '\0';
        } else {
            break;
        }
    }

    telemetry.statusValid =
        frame[POS_FRAME_BLOCK] == EXPECTED_FRAME_ID &&
        serialLooksValid(telemetry.serial);
}

void decodeGps1(
    const uint8_t frame[FRAME_LENGTH],
    SondeTelemetry& telemetry
) {
    telemetry.gps1BlockSeen =
        frame[POS_GPS1] == EXPECTED_GPS1_ID;

    telemetry.gps1CrcValid =
        telemetry.gps1BlockSeen &&
        rs41CheckBlockCrc(frame + POS_GPS1,
                          FRAME_LENGTH - POS_GPS1);

    telemetry.gpsInfoCrcValid = telemetry.gps1CrcValid;

    telemetry.gpsWeek = readU16(frame + POS_GPS_WEEK);
    telemetry.gpsTowMs = readU32(frame + POS_GPS_TOW);

    telemetry.gpsInfoValid =
        telemetry.gps1CrcValid &&
        telemetry.gpsWeek > 1000 &&
        telemetry.gpsTowMs < 604800000UL;

    // This block contains the tracked satellite list, but not all tracked
    // satellites are necessarily part of a solved navigation fix.
    uint8_t tracked = 0;
    for (uint8_t i = 0; i < 12; ++i) {
        const uint8_t sv = frame[POS_SATS_LIST + (2 * i)];
        if (sv == 0xFF || sv == 0x00) {
            continue;
        }
        ++tracked;
    }

    if (telemetry.satellites == 0) {
        telemetry.satellites = tracked;
    }
}

void decodeGps3(
    const uint8_t frame[FRAME_LENGTH],
    SondeTelemetry& telemetry
) {
    telemetry.gps3BlockSeen =
        frame[POS_GPS3] == EXPECTED_GPS3_ID &&
        frame[POS_GPS3 + 1] == EXPECTED_GPS3_LEN;

    telemetry.gps3CrcValid =
        telemetry.gps3BlockSeen &&
        rs41CheckBlockCrc(frame + POS_GPS3,
                          FRAME_LENGTH - POS_GPS3);

    telemetry.gpsPositionCrcValid = telemetry.gps3CrcValid;

    telemetry.gps3RawLength = 25;
    memcpy(telemetry.gps3Raw,
           frame + POS_GPS3,
           telemetry.gps3RawLength);

    telemetry.rawEcefXcm = readI32(frame + POS_ECEF_X);
    telemetry.rawEcefYcm = readI32(frame + POS_ECEF_Y);
    telemetry.rawEcefZcm = readI32(frame + POS_ECEF_Z);

    telemetry.rawEcefVxCms = readI16(frame + POS_ECEF_V + 0);
    telemetry.rawEcefVyCms = readI16(frame + POS_ECEF_V + 2);
    telemetry.rawEcefVzCms = readI16(frame + POS_ECEF_V + 4);

    const uint8_t solvedSatellites = frame[POS_NUM_SATS];
    if (solvedSatellites != 0xFF) {
        telemetry.satellites = solvedSatellites;
    }

    telemetry.speedAccuracyMps =
        static_cast<float>(frame[POS_SACC]) / 10.0f;

    telemetry.positionDop =
        static_cast<float>(frame[POS_PDOP]) / 10.0f;

    const double x = telemetry.rawEcefXcm / 100.0;
    const double y = telemetry.rawEcefYcm / 100.0;
    const double z = telemetry.rawEcefZcm / 100.0;

    ecefToGeodetic(
        x,
        y,
        z,
        telemetry.latitude,
        telemetry.longitude,
        telemetry.altitudeMetres
    );

    const double vx = telemetry.rawEcefVxCms / 100.0;
    const double vy = telemetry.rawEcefVyCms / 100.0;
    const double vz = telemetry.rawEcefVzCms / 100.0;

    if (positionMathLooksValid(telemetry)) {
        ecefVelocityToLocal(
            vx,
            vy,
            vz,
            telemetry.latitude,
            telemetry.longitude,
            telemetry.horizontalSpeedMps,
            telemetry.headingDegrees,
            telemetry.verticalSpeedMps
        );
    }

    // A cached/stale NAV-SOL position can be mathematically plausible even
    // when the receiver has no solved fix. Do not accept it unless GPS3 CRC is
    // good and enough satellites are reported in the solved navigation block.
    telemetry.positionValid =
        telemetry.gps3CrcValid &&
        telemetry.satellites >= 4 &&
        positionMathLooksValid(telemetry);
}
}

Rs41DecodeResult Rs41Decoder::decode(
    const uint8_t* capturedPayload,
    size_t capturedLength,
    int8_t rssiDbm
) {
    Rs41DecodeResult result;
    result.telemetry.rssiDbm = rssiDbm;

    if (capturedPayload == nullptr ||
        capturedLength != CAPTURED_LENGTH) {
        result.status = Rs41DecodeStatus::WrongLength;
        return result;
    }

    uint8_t frame[FRAME_LENGTH];
    reconstructAndDewhiten(capturedPayload, frame);

    if (frame[POS_FRAME_TYPE] != EXPECTED_FRAME_TYPE) {
        result.status = Rs41DecodeStatus::BadFrameType;
        return result;
    }

    result.telemetry.headerValid = true;

    const Rs41FecResult fec = rs41CorrectFrame(frame);

    if (!fec.success) {
        result.status = Rs41DecodeStatus::FecFailed;
        return result;
    }

    result.telemetry.fecValid = true;
    result.telemetry.correctedErrors = fec.totalCorrected();

    decodeFrameIdentity(frame, result.telemetry);
    decodeGps1(frame, result.telemetry);
    decodeGps3(frame, result.telemetry);

    if (!result.telemetry.statusValid) {
        result.status = Rs41DecodeStatus::StatusCrcFailed;
        return result;
    }

    if (!result.telemetry.positionValid) {
        result.status = Rs41DecodeStatus::ValidNoGpsFix;
        result.telemetry.valid = true;
        return result;
    }

    result.telemetry.valid = true;
    result.status = Rs41DecodeStatus::Valid;
    return result;
}

const char* Rs41Decoder::statusText(Rs41DecodeStatus status) {
    switch (status) {
        case Rs41DecodeStatus::Valid:
            return "valid";
        case Rs41DecodeStatus::ValidNoGpsFix:
            return "valid frame, no current GPS fix";
        case Rs41DecodeStatus::WrongLength:
            return "wrong frame length";
        case Rs41DecodeStatus::BadFrameType:
            return "not a standard RS41 frame";
        case Rs41DecodeStatus::FecFailed:
            return "Reed-Solomon correction failed";
        case Rs41DecodeStatus::StatusMissing:
            return "STATUS block missing";
        case Rs41DecodeStatus::StatusCrcFailed:
            return "STATUS/serial decode failed";
        case Rs41DecodeStatus::PositionMissingOrInvalid:
            return "GPS position missing or invalid";
        default:
            return "unknown decoder result";
    }
}

uint8_t Rs41Decoder::reverseBits(uint8_t value) {
    value = static_cast<uint8_t>(
        ((value & 0xF0) >> 4) |
        ((value & 0x0F) << 4)
    );

    value = static_cast<uint8_t>(
        ((value & 0xCC) >> 2) |
        ((value & 0x33) << 2)
    );

    return static_cast<uint8_t>(
        ((value & 0xAA) >> 1) |
        ((value & 0x55) << 1)
    );
}

void Rs41Decoder::reconstructAndDewhiten(
    const uint8_t* capturedPayload,
    uint8_t frame[320]
) {
    memcpy(frame, DEWHITENED_HEADER, sizeof(DEWHITENED_HEADER));

    for (size_t i = 0; i < CAPTURED_LENGTH; ++i) {
        const size_t absoluteOffset = i + 8;

        frame[absoluteOffset] =
            reverseBits(capturedPayload[i]) ^
            WHITENING_MASK[absoluteOffset % 64];
    }
}
