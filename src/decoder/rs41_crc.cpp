#include "rs41_crc.h"

uint16_t rs41Crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000)
                ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                : static_cast<uint16_t>(crc << 1);
        }
    }

    return crc;
}

bool rs41CheckBlockCrc(const uint8_t* block, size_t availableLength) {
    if (block == nullptr || availableLength < 4) {
        return false;
    }

    const size_t dataLength = block[1];
    const size_t totalLength = dataLength + 4;

    if (totalLength > availableLength) {
        return false;
    }

    const uint16_t expected =
        static_cast<uint16_t>(block[2 + dataLength]) |
        (static_cast<uint16_t>(block[3 + dataLength]) << 8);

    return rs41Crc16(block + 2, dataLength) == expected;
}
