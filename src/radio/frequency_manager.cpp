#include "frequency_manager.h"

#include <string.h>

#include "../config/frequency_config.h"

namespace {
void safeCopy(char* destination, size_t destinationLength, const char* source) {
    if (destinationLength == 0) {
        return;
    }

    if (source == nullptr) {
        source = "";
    }

    strncpy(destination, source, destinationLength - 1);
    destination[destinationLength - 1] = '\0';
}
}

void FrequencyManager::begin() {
    findDefaultPreset();

    scanEnabled_ = false;
    locked_ = false;
    lastTuneMs_ = millis();
    lastRssiDbm_ = -127;
    bestRssiDbm_ = -127;
    bestFrequencyHz_ = currentFrequencyHz();
    lockedSerial_[0] = '\0';
}

uint32_t FrequencyManager::currentFrequencyHz() const {
    return FrequencyConfig::PRESETS_HZ[presetIndex_];
}

uint32_t FrequencyManager::selectNextPreset() {
    presetIndex_ =
        static_cast<uint8_t>((presetIndex_ + 1) % FrequencyConfig::PRESET_COUNT);

    scanEnabled_ = false;
    locked_ = false;
    lockedSerial_[0] = '\0';

    return currentFrequencyHz();
}

uint32_t FrequencyManager::selectPreviousPreset() {
    if (presetIndex_ == 0) {
        presetIndex_ = FrequencyConfig::PRESET_COUNT - 1;
    } else {
        --presetIndex_;
    }

    scanEnabled_ = false;
    locked_ = false;
    lockedSerial_[0] = '\0';

    return currentFrequencyHz();
}

bool FrequencyManager::toggleScan() {
    scanEnabled_ = !scanEnabled_;
    locked_ = false;
    lockedSerial_[0] = '\0';

    if (scanEnabled_) {
        bestRssiDbm_ = -127;
        bestFrequencyHz_ = currentFrequencyHz();
        lastTuneMs_ = 0;
    }

    return scanEnabled_;
}

void FrequencyManager::stopScan() {
    scanEnabled_ = false;
    locked_ = false;
    lockedSerial_[0] = '\0';
}

bool FrequencyManager::scanEnabled() const {
    return scanEnabled_;
}

bool FrequencyManager::locked() const {
    return locked_;
}

bool FrequencyManager::shouldAdvanceScan(bool receiverSearching, uint32_t nowMs) const {
    if (!scanEnabled_ || locked_ || !receiverSearching) {
        return false;
    }

    if (lastTuneMs_ == 0) {
        return true;
    }

    return nowMs - lastTuneMs_ >= FrequencyConfig::SCAN_DWELL_MS;
}

uint32_t FrequencyManager::advanceScan(uint32_t nowMs) {
    presetIndex_ =
        static_cast<uint8_t>((presetIndex_ + 1) % FrequencyConfig::PRESET_COUNT);

    lastTuneMs_ = nowMs;
    lastRssiDbm_ = -127;

    return currentFrequencyHz();
}

void FrequencyManager::noteTuned(uint32_t nowMs) {
    lastTuneMs_ = nowMs;
    lastRssiDbm_ = -127;
}

void FrequencyManager::noteFrameRssi(int8_t rssiDbm) {
    lastRssiDbm_ = rssiDbm;

    if (rssiDbm > bestRssiDbm_) {
        bestRssiDbm_ = rssiDbm;
        bestFrequencyHz_ = currentFrequencyHz();
    }
}

void FrequencyManager::noteValidFrame(
    const char* serial,
    int8_t rssiDbm,
    uint32_t nowMs
) {
    noteFrameRssi(rssiDbm);
    lastTuneMs_ = nowMs;

    if (scanEnabled_) {
        scanEnabled_ = false;
        locked_ = true;
        copySerial(serial);
    }
}

FrequencyStatus FrequencyManager::status(uint32_t nowMs) const {
    FrequencyStatus value;

    value.scanEnabled = scanEnabled_;
    value.locked = locked_;
    value.presetIndex = presetIndex_;
    value.presetCount = FrequencyConfig::PRESET_COUNT;
    value.currentFrequencyHz = currentFrequencyHz();
    value.bestFrequencyHz = bestFrequencyHz_;
    value.scanDwellMs = FrequencyConfig::SCAN_DWELL_MS;
    value.msOnFrequency =
        nowMs >= lastTuneMs_
            ? nowMs - lastTuneMs_
            : 0;

    value.lastRssiDbm = lastRssiDbm_;
    value.bestRssiDbm = bestRssiDbm_;

    if (locked_) {
        safeCopy(value.mode, sizeof(value.mode), "Locked");
    } else if (scanEnabled_) {
        safeCopy(value.mode, sizeof(value.mode), "Scanning");
    } else {
        safeCopy(value.mode, sizeof(value.mode), "Fixed");
    }

    safeCopy(value.lockedSerial, sizeof(value.lockedSerial), lockedSerial_);

    return value;
}

void FrequencyManager::findDefaultPreset() {
    presetIndex_ = 0;

    for (uint8_t i = 0; i < FrequencyConfig::PRESET_COUNT; ++i) {
        if (FrequencyConfig::PRESETS_HZ[i] ==
            FrequencyConfig::DEFAULT_FREQUENCY_HZ) {
            presetIndex_ = i;
            return;
        }
    }
}

void FrequencyManager::copySerial(const char* serial) {
    safeCopy(lockedSerial_, sizeof(lockedSerial_), serial);
}
