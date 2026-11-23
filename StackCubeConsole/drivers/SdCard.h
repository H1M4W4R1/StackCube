#pragma once

#include <SD.h>

class SdCard {
public:
    enum class Change { None, Inserted, Removed, MountFailed };
    Change poll(uint32_t now);
    bool mounted() const { return present; }
    bool inserted() const { return detected; }

private:
    Change mount();
    uint32_t lastProbe = 0;
    uint32_t lastTransition = 0;
    bool firstProbe = true;
    bool detectError = false;
    bool candidate = false;
    bool detected = false;
    bool present = false;
};
