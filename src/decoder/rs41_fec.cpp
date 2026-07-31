#include "rs41_fec.h"

#include <string.h>

extern "C" {
#include <rs1729_fec.h>
}

namespace {
constexpr int CODEWORD_LENGTH = 255;
constexpr int PARITY_LENGTH = 24;
constexpr int DATA_LENGTH = 231;
constexpr int FRAME_DATA_BYTES_PER_LANE = 132;

bool initialised = false;

void buildCodewords(
    const uint8_t frame[320],
    uint8_t first[CODEWORD_LENGTH],
    uint8_t second[CODEWORD_LENGTH]
) {
    memset(first, 0, CODEWORD_LENGTH);
    memset(second, 0, CODEWORD_LENGTH);

    // Proven RS41 layout from rs1729/RS:
    // parity bytes are frame[8..31] and frame[32..55];
    // message bytes from frame[56] onward are interleaved by lane.
    for (int i = 0; i < PARITY_LENGTH; ++i) {
        first[i] = frame[8 + i];
        second[i] = frame[8 + PARITY_LENGTH + i];
    }

    for (int i = 0; i < FRAME_DATA_BYTES_PER_LANE; ++i) {
        first[PARITY_LENGTH + i] = frame[56 + (2 * i)];
        second[PARITY_LENGTH + i] = frame[56 + (2 * i) + 1];
    }

    // Remaining shortened data bytes stay zero-padded.
}

void writeCorrectedCodewords(
    uint8_t frame[320],
    const uint8_t first[CODEWORD_LENGTH],
    const uint8_t second[CODEWORD_LENGTH]
) {
    for (int i = 0; i < PARITY_LENGTH; ++i) {
        frame[8 + i] = first[i];
        frame[8 + PARITY_LENGTH + i] = second[i];
    }

    for (int i = 0; i < FRAME_DATA_BYTES_PER_LANE; ++i) {
        frame[56 + (2 * i)] = first[PARITY_LENGTH + i];
        frame[56 + (2 * i) + 1] = second[PARITY_LENGTH + i];
    }
}
}

Rs41FecResult rs41CorrectFrame(uint8_t frame[320]) {
    Rs41FecResult result;

    if (!initialised) {
        if (rs1729_rs41_fec_init() != 0) {
            return result;
        }
        initialised = true;
    }

    uint8_t first[CODEWORD_LENGTH];
    uint8_t second[CODEWORD_LENGTH];

    buildCodewords(frame, first, second);

    const int correctedFirst = rs1729_rs41_fec_decode(first);
    const int correctedSecond = rs1729_rs41_fec_decode(second);

    if (correctedFirst < 0 || correctedSecond < 0) {
        return result;
    }

    writeCorrectedCodewords(frame, first, second);

    result.success = true;
    result.correctedCodeword1 =
        static_cast<uint8_t>(correctedFirst);
    result.correctedCodeword2 =
        static_cast<uint8_t>(correctedSecond);

    return result;
}
