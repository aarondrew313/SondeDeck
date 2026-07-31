/*
 * Receive-only SX126x long-packet helper.
 *
 * This is a reduced receive-only implementation based on Semtech's
 * sx126x_long_pkt example code. The original implementation is distributed
 * under the Revised BSD licence.
 *
 * Copyright Semtech Corporation 2019.
 */

#include "sx126x_long_rx.h"

extern "C" {
#include <sx126x_regs.h>
}

namespace {
constexpr uint16_t REG_RX_ADDRESS_POINTER = 0x0803;
constexpr uint16_t REG_RXTX_PAYLOAD_LENGTH = 0x06BB;
constexpr uint8_t HARDWARE_PACKET_LIMIT = 255;
}

sx126x_status_t sx126xLongRxConfigure(
    const void* context,
    const sx126x_pkt_params_gfsk_t* params
) {
    if (params == nullptr) {
        return SX126X_STATUS_ERROR;
    }

    if (params->crc_type != SX126X_GFSK_CRC_OFF ||
        params->address_filtering != SX126X_GFSK_ADDRESS_FILTERING_DISABLE) {
        return SX126X_STATUS_UNSUPPORTED_FEATURE;
    }

    sx126x_pkt_params_gfsk_t hardwareParams = *params;
    hardwareParams.pld_len_in_bytes = HARDWARE_PACKET_LIMIT;
    return sx126x_set_gfsk_pkt_params(context, &hardwareParams);
}

sx126x_status_t sx126xLongRxStart(
    const void* context,
    Sx126xLongRxState* state
) {
    if (state == nullptr) {
        return SX126X_STATUS_ERROR;
    }

    state->index = 0;
    uint8_t payloadLength = HARDWARE_PACKET_LIMIT;

    sx126x_status_t status = sx126x_write_register(
        context,
        REG_RXTX_PAYLOAD_LENGTH,
        &payloadLength,
        1
    );

    if (status != SX126X_STATUS_OK) {
        return status;
    }

    return sx126x_set_rx_with_timeout_in_rtc_step(
        context,
        SX126X_RX_CONTINUOUS
    );
}

sx126x_status_t sx126xLongRxReadPartial(
    const void* context,
    Sx126xLongRxState* state,
    uint8_t* destination,
    unsigned int maximumLength,
    uint8_t* bytesRead
) {
    if (state == nullptr || destination == nullptr || bytesRead == nullptr) {
        return SX126X_STATUS_ERROR;
    }

    uint8_t currentIndex = 0;
    sx126x_status_t status = sx126x_read_register(
        context,
        REG_RX_ADDRESS_POINTER,
        &currentIndex,
        1
    );

    if (status != SX126X_STATUS_OK) {
        return status;
    }

    uint8_t available = static_cast<uint8_t>(currentIndex - state->index);

    if (available > maximumLength) {
        available = static_cast<uint8_t>(maximumLength);
        currentIndex = static_cast<uint8_t>(state->index + available);
    }

    // Keep the receiver alive beyond the hardware's normal 255-byte limit.
    uint8_t extendedEnd = static_cast<uint8_t>(currentIndex - 1);
    status = sx126x_write_register(
        context,
        REG_RXTX_PAYLOAD_LENGTH,
        &extendedEnd,
        1
    );

    if (status != SX126X_STATUS_OK) {
        return status;
    }

    if (available > 0) {
        status = sx126x_read_buffer(
            context,
            state->index,
            destination,
            available
        );

        if (status != SX126X_STATUS_OK) {
            return status;
        }

        state->index = currentIndex;
    }

    *bytesRead = available;
    return SX126X_STATUS_OK;
}

sx126x_status_t sx126xLongRxPrepareFinal(
    const void* context,
    const Sx126xLongRxState* state,
    uint8_t remainingLength
) {
    if (state == nullptr) {
        return SX126X_STATUS_ERROR;
    }

    const uint8_t endAddress =
        static_cast<uint8_t>(state->index + remainingLength);

    return sx126x_write_register(
        context,
        REG_RXTX_PAYLOAD_LENGTH,
        &endAddress,
        1
    );
}

sx126x_status_t sx126xLongRxStop(const void* context) {
    return sx126x_set_standby(context, SX126X_STANDBY_CFG_RC);
}
