#pragma once

#include <stdint.h>

struct Rs41FecResult {
    bool success = false;
    uint8_t correctedCodeword1 = 0;
    uint8_t correctedCodeword2 = 0;

    uint8_t totalCorrected() const {
        return static_cast<uint8_t>(
            correctedCodeword1 + correctedCodeword2
        );
    }
};

Rs41FecResult rs41CorrectFrame(uint8_t frame[320]);
