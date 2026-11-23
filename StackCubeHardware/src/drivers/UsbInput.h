#pragma once

#include "../StackCubeInput.h"

namespace stackcube::usb {
bool begin();
const Input& poll();
bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
void stopRumble();
}
