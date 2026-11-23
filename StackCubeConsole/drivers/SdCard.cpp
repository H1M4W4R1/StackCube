#include "SdCard.h"

#include <StackCubeHal.h>

SdCard::Change SdCard::poll(uint32_t now) {
    constexpr uint32_t DebounceMs = 60;
    if ((!firstProbe || detectError) && now - lastProbe < DebounceMs) return Change::None;
    lastProbe = now;

    bool inserted;
    if (!stackcube::hal::readSdPresence(inserted)) {
        if (!detectError) Serial.println("SD card-detect read failed (AW9523B)");
        detectError = true;
        return Change::None;
    }
    detectError = false;
    if (firstProbe || inserted != candidate) {
        candidate = inserted;
        lastTransition = firstProbe ? now - DebounceMs : now;
        firstProbe = false;
    }
    if (candidate == detected || now - lastTransition < DebounceMs) return Change::None;
    detected = candidate;
    if (detected) return mount();
    if (present) {
        stackcube::hal::endGameAudio();
        SD.end();
    }
    present = false;
    Serial.println("SD removed");
    return Change::Removed;
}

SdCard::Change SdCard::mount() {
    Serial.println("SD card detected");
    present = stackcube::hal::mountSd();
    if (!present) {
        Serial.println("SD mount failed, reinsert the card to retry");
        return Change::MountFailed;
    }
    Serial.println("SD mounted");
    return Change::Inserted;
}
