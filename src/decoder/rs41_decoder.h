#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../models/sonde_telemetry.h"

enum class Rs41DecodeStatus {
    Valid,
    ValidNoGpsFix,
    WrongLength,
    BadFrameType,
    FecFailed,
    StatusMissing,
    StatusCrcFailed,
    PositionMissingOrInvalid
};

struct Rs41DecodeResult {
    Rs41DecodeStatus status = Rs41DecodeStatus::WrongLength;
    SondeTelemetry telemetry;
};

class Rs41Decoder {
public:
    Rs41DecodeResult decode(
        const uint8_t* capturedPayload,
        size_t capturedLength,
        int8_t rssiDbm
    );

    static const char* statusText(Rs41DecodeStatus status);

private:
    static uint8_t reverseBits(uint8_t value);
    static void reconstructAndDewhiten(
        const uint8_t* capturedPayload,
        uint8_t frame[320]
    );
};
