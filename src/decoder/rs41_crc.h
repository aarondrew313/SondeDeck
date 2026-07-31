#pragma once

#include <stddef.h>
#include <stdint.h>

uint16_t rs41Crc16(const uint8_t* data, size_t length);
bool rs41CheckBlockCrc(const uint8_t* block, size_t availableLength);
