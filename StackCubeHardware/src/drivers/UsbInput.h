#pragma once

#include "../StackCubeInput.h"

namespace stackcube::usb {
bool begin();
const Input& poll();
}
