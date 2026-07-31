/*
 * Thin ESP32/Arduino wrapper around the proven Reed-Solomon implementation
 * used by rs1729/RS.
 *
 * Important:
 * bch_ecc_upstream.inc is deliberately included here as an .inc file, not
 * compiled separately. The upstream source expects ui8_t/ui32_t to be defined
 * by the including RS41 decoder before bch_ecc.c is included.
 */

#include <stdint.h>
#include <stdio.h>

typedef unsigned char ui8_t;
typedef unsigned int ui32_t;

#include "bch_ecc_upstream.inc"

int rs1729_rs41_fec_init(void) {
    return rs_init_RS255();
}

int rs1729_rs41_fec_decode(uint8_t codeword[255]) {
    ui8_t errorPositions[24];
    ui8_t errorValues[24];

    return rs_decode(codeword, errorPositions, errorValues);
}
