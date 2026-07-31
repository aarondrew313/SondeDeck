#include "sx1262_receiver.h"

#include <SPI.h>

#include "../board_pins.h"
#include "../config.h"
#include "../semtech/sx126x_long_rx.h"

extern "C" {
#include <sx126x.h>
#include <sx126x_hal.h>
#include <sx126x_regs.h>
}

namespace {
Sx126xLongRxState longRxState = {};

bool checkStatus(const char* operation, sx126x_status_t status) {
    if (status == SX126X_STATUS_OK) {
        Serial.printf("%-32s OK\n", operation);
        return true;
    }

    Serial.printf("%-32s FAILED (%d)\n", operation, static_cast<int>(status));
    return false;
}

sx126x_status_t enableDio2RfSwitch() {
    // SX126x SetDIO2AsRfSwitchCtrl command.
    const uint8_t command = 0x9D;
    const uint8_t enabled = 0x01;

    return sx126x_hal_write(
        nullptr,
        &command,
        1,
        &enabled,
        1
    ) == SX126X_HAL_STATUS_OK
        ? SX126X_STATUS_OK
        : SX126X_STATUS_ERROR;
}

sx126x_status_t enableBoostedRxGain() {
    uint8_t gain = 0x96;
    return sx126x_write_register(
        nullptr,
        SX126X_REG_RXGAIN,
        &gain,
        1
    );
}
}

bool Sx1262Receiver::begin() {
    currentFrequencyHz_ = SondeConfig::FREQUENCY_HZ;
    pinMode(BoardPins::POWER_ENABLE, OUTPUT);
    digitalWrite(BoardPins::POWER_ENABLE, HIGH);
    delay(100);

    pinMode(BoardPins::DISPLAY_CS, OUTPUT);
    pinMode(BoardPins::SD_CS, OUTPUT);
    pinMode(BoardPins::RADIO_NSS, OUTPUT);
    pinMode(BoardPins::RADIO_RESET, OUTPUT);
    pinMode(BoardPins::RADIO_BUSY, INPUT);
    pinMode(BoardPins::RADIO_DIO1, INPUT);

    digitalWrite(BoardPins::DISPLAY_CS, HIGH);
    digitalWrite(BoardPins::SD_CS, HIGH);
    digitalWrite(BoardPins::RADIO_NSS, HIGH);
    digitalWrite(BoardPins::RADIO_RESET, HIGH);

    SPI.begin(
        BoardPins::SPI_SCK,
        BoardPins::SPI_MISO,
        BoardPins::SPI_MOSI,
        BoardPins::RADIO_NSS
    );

    if (!configureRadio()) {
        return false;
    }

    return startSearching();
}

bool Sx1262Receiver::configureRadio() {
    Serial.println("Configuring SX1262 for RS41...");

    if (!checkStatus("Reset", sx126x_reset(nullptr))) {
        return false;
    }

    if (!checkStatus(
            "TCXO 1.6 V",
            sx126x_set_dio3_as_tcxo_ctrl(
                nullptr,
                SX126X_TCXO_CTRL_1_6V,
                128
            ))) {
        return false;
    }

    delay(3);

    if (!checkStatus(
            "Standby RC",
            sx126x_set_standby(nullptr, SX126X_STANDBY_CFG_RC))) {
        return false;
    }

    if (!checkStatus(
            "DC-DC regulator",
            sx126x_set_reg_mode(nullptr, SX126X_REG_MODE_DCDC))) {
        return false;
    }

    if (!checkStatus("DIO2 RF switch", enableDio2RfSwitch())) {
        return false;
    }

    if (!checkStatus(
            "Packet type GFSK",
            sx126x_set_pkt_type(nullptr, SX126X_PKT_TYPE_GFSK))) {
        return false;
    }

    if (!checkStatus(
            "RF frequency",
            sx126x_set_rf_freq(nullptr, currentFrequencyHz_))) {
        return false;
    }

    sx126x_mod_params_gfsk_t modulation = {};
    modulation.br_in_bps = SondeConfig::BIT_RATE_BPS;
    modulation.fdev_in_hz = SondeConfig::FDEV_HZ;
    modulation.pulse_shape = SX126X_GFSK_PULSE_SHAPE_OFF;
    modulation.bw_dsb_param = SX126X_GFSK_BW_39000;

    if (!checkStatus(
            "GFSK modulation",
            sx126x_set_gfsk_mod_params(nullptr, &modulation))) {
        return false;
    }

    sx126x_pkt_params_gfsk_t packet = {};
    packet.preamble_len_in_bits = 0;
    packet.preamble_detector = SX126X_GFSK_PREAMBLE_DETECTOR_MIN_24BITS;
    packet.sync_word_len_in_bits = 64;
    packet.address_filtering = SX126X_GFSK_ADDRESS_FILTERING_DISABLE;
    packet.header_type = SX126X_GFSK_PKT_FIX_LEN;
    packet.pld_len_in_bytes = 255;
    packet.crc_type = SX126X_GFSK_CRC_OFF;
    packet.dc_free = SX126X_GFSK_DC_FREE_OFF;

    if (!checkStatus(
            "Long packet parameters",
            sx126xLongRxConfigure(nullptr, &packet))) {
        return false;
    }

    if (!checkStatus(
            "RS41 sync word",
            sx126x_set_gfsk_sync_word(
                nullptr,
                SondeConfig::SYNC_WORD,
                sizeof(SondeConfig::SYNC_WORD)))) {
        return false;
    }

    if (!checkStatus(
            "DIO1 sync interrupt",
            sx126x_set_dio_irq_params(
                nullptr,
                SX126X_IRQ_SYNC_WORD_VALID,
                SX126X_IRQ_SYNC_WORD_VALID,
                SX126X_IRQ_NONE,
                SX126X_IRQ_NONE))) {
        return false;
    }

    if (!checkStatus(
            "Image calibration",
            sx126x_cal_img_in_mhz(nullptr, 400, 410))) {
        return false;
    }

    if (!checkStatus("Boosted RX gain", enableBoostedRxGain())) {
        return false;
    }

    if (!checkStatus(
            "Clear device errors",
            sx126x_clear_device_errors(nullptr))) {
        return false;
    }

    return true;
}

bool Sx1262Receiver::startSearching() {
    bytesRead_ = 0;
    syncAtMs_ = 0;
    lastDrainAtMs_ = 0;
    frameRssiDbm_ = -127;
    state_ = State::Searching;

    sx126x_clear_irq_status(nullptr, SX126X_IRQ_ALL);

    const sx126x_status_t status = sx126xLongRxStart(
        nullptr,
        &longRxState
    );

    if (status != SX126X_STATUS_OK) {
        Serial.printf("Unable to restart RX: %d\n", static_cast<int>(status));
        return false;
    }

    return true;
}

void Sx1262Receiver::update() {
    if (state_ == State::Ready) {
        return;
    }

    const uint32_t now = millis();

    if (state_ == State::Searching &&
        digitalRead(BoardPins::RADIO_DIO1) == HIGH) {
        sx126x_clear_irq_status(nullptr, SX126X_IRQ_SYNC_WORD_VALID);

        sx126x_pkt_status_gfsk_t status = {};
        if (sx126x_get_gfsk_pkt_status(nullptr, &status) ==
            SX126X_STATUS_OK) {
            frameRssiDbm_ = status.rssi_sync;
        }

        bytesRead_ = 0;
        syncAtMs_ = now;
        lastDrainAtMs_ = now;
        state_ = State::Capturing;

        Serial.printf(
            "\nSYNC DETECTED  RSSI %d dBm\n",
            static_cast<int>(frameRssiDbm_)
        );
    }

    if (state_ != State::Capturing) {
        return;
    }

    if (now - syncAtMs_ > SondeConfig::CAPTURE_TIMEOUT_MS) {
        abortCapture("capture timeout");
        return;
    }

    if (now - lastDrainAtMs_ < SondeConfig::FIRST_DRAIN_DELAY_MS) {
        return;
    }

    lastDrainAtMs_ = now;

    if (!drainAvailableBytes()) {
        abortCapture("FIFO read failed");
        return;
    }

    const size_t remaining = SondeConfig::FRAME_LENGTH - bytesRead_;

    if (remaining <= 255) {
        const sx126x_status_t status = sx126xLongRxPrepareFinal(
            nullptr,
            &longRxState,
            static_cast<uint8_t>(remaining)
        );

        if (status != SX126X_STATUS_OK) {
            abortCapture("final length setup failed");
            return;
        }
    }

    if (bytesRead_ == SondeConfig::FRAME_LENGTH) {
        sx126xLongRxStop(nullptr);
        ++completedFrames_;
        state_ = State::Ready;

        Serial.printf(
            "FRAME COMPLETE: %u bytes  RSSI %d dBm  TOTAL %lu\n",
            static_cast<unsigned>(bytesRead_),
            static_cast<int>(frameRssiDbm_),
            static_cast<unsigned long>(completedFrames_)
        );
    }
}

bool Sx1262Receiver::drainAvailableBytes() {
    if (bytesRead_ >= SondeConfig::FRAME_LENGTH) {
        return true;
    }

    uint8_t chunkLength = 0;
    const size_t capacity = SondeConfig::FRAME_LENGTH - bytesRead_;

    const sx126x_status_t status = sx126xLongRxReadPartial(
        nullptr,
        &longRxState,
        frame_ + bytesRead_,
        capacity,
        &chunkLength
    );

    if (status != SX126X_STATUS_OK || chunkLength == 0) {
        return false;
    }

    bytesRead_ += chunkLength;

    Serial.printf(
        "FIFO READ: %u bytes  TOTAL: %u/%u\n",
        static_cast<unsigned>(chunkLength),
        static_cast<unsigned>(bytesRead_),
        static_cast<unsigned>(SondeConfig::FRAME_LENGTH)
    );

    return true;
}

void Sx1262Receiver::abortCapture(const char* reason) {
    ++abortedFrames_;

    Serial.printf(
        "FRAME ABORTED: %s  bytes=%u  aborted=%lu\n",
        reason,
        static_cast<unsigned>(bytesRead_),
        static_cast<unsigned long>(abortedFrames_)
    );

    sx126xLongRxStop(nullptr);
    startSearching();
}

bool Sx1262Receiver::setFrequencyHz(uint32_t frequencyHz) {
    if (frequencyHz == currentFrequencyHz_ && state_ == State::Searching) {
        return true;
    }

    if (state_ == State::Ready) {
        return false;
    }

    sx126xLongRxStop(nullptr);

    if (!checkStatus(
            "Standby before retune",
            sx126x_set_standby(nullptr, SX126X_STANDBY_CFG_RC))) {
        startSearching();
        return false;
    }

    if (!checkStatus(
            "Set RF frequency",
            sx126x_set_rf_freq(nullptr, frequencyHz))) {
        startSearching();
        return false;
    }

    currentFrequencyHz_ = frequencyHz;

    Serial.printf(
        "Receiver tuned: %.3f MHz\n",
        currentFrequencyHz_ / 1000000.0
    );

    return startSearching();
}

uint32_t Sx1262Receiver::frequencyHz() const {
    return currentFrequencyHz_;
}

bool Sx1262Receiver::isSearching() const {
    return state_ == State::Searching;
}

bool Sx1262Receiver::frameAvailable() const {
    return state_ == State::Ready;
}

const uint8_t* Sx1262Receiver::frameData() const {
    return frame_;
}

size_t Sx1262Receiver::frameLength() const {
    return bytesRead_;
}

int8_t Sx1262Receiver::frameRssi() const {
    return frameRssiDbm_;
}

uint32_t Sx1262Receiver::completedFrames() const {
    return completedFrames_;
}

uint32_t Sx1262Receiver::abortedFrames() const {
    return abortedFrames_;
}

void Sx1262Receiver::releaseFrame() {
    if (state_ == State::Ready) {
        startSearching();
    }
}
