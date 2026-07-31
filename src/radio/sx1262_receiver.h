#pragma once

#include <Arduino.h>

class Sx1262Receiver {
public:
    bool begin();
    void update();

    bool setFrequencyHz(uint32_t frequencyHz);
    uint32_t frequencyHz() const;
    bool isSearching() const;

    bool frameAvailable() const;
    const uint8_t* frameData() const;
    size_t frameLength() const;
    int8_t frameRssi() const;
    uint32_t completedFrames() const;
    uint32_t abortedFrames() const;

    void releaseFrame();

private:
    enum class State {
        Searching,
        Capturing,
        Ready
    };

    bool configureRadio();
    bool startSearching();
    bool drainAvailableBytes();
    void abortCapture(const char* reason);

    State state_ = State::Searching;

    uint32_t currentFrequencyHz_ = 405700000UL;

    uint8_t frame_[312] = {};
    size_t bytesRead_ = 0;

    uint32_t syncAtMs_ = 0;
    uint32_t lastDrainAtMs_ = 0;

    int8_t frameRssiDbm_ = -127;

    uint32_t completedFrames_ = 0;
    uint32_t abortedFrames_ = 0;
};
