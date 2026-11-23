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
    bool returnToLauncher();
};

}
