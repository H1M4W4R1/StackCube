#pragma once

#include "../StackCubeInput.h"

namespace stackcube::usb {
bool begin();
const Input& poll();
int32_t readGpio(uint8_t channel);
bool writeGpio(uint8_t channel, bool high);
void resetGpioOutputs();
bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
void stopRumble();
}
