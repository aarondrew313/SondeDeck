#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int rs1729_rs41_fec_init(void);
int rs1729_rs41_fec_decode(uint8_t codeword[255]);

#ifdef __cplusplus
}
#endif
