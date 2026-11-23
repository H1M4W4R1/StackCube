#pragma once

#include <M5GFX.h>
#include <StackCubeInput.h>

namespace stackcube {

class Console {
public:
    bool begin();
    const Input& poll();
    M5GFX& screen();
    bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
    bool preload_sfx(const char* filename);
    bool play_sfx(const char* filename);
    bool play_bg_track(const char* filename);
    void stop_bg_track();
    bool returnToLauncher();
};

}
