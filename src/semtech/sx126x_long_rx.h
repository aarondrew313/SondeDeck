#pragma once

#include <stdint.h>

extern "C" {
#include <sx126x.h>
}

struct Sx126xLongRxState {
    uint8_t index;
};

sx126x_status_t sx126xLongRxConfigure(
    const void* context,
    const sx126x_pkt_params_gfsk_t* params
);

sx126x_status_t sx126xLongRxStart(
    const void* context,
    Sx126xLongRxState* state
);

sx126x_status_t sx126xLongRxReadPartial(
    const void* context,
    Sx126xLongRxState* state,
    uint8_t* destination,
    unsigned int maximumLength,
    uint8_t* bytesRead
);

sx126x_status_t sx126xLongRxPrepareFinal(
    const void* context,
    const Sx126xLongRxState* state,
    uint8_t remainingLength
);

sx126x_status_t sx126xLongRxStop(const void* context);
