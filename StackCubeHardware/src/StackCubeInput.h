#pragma once

#include <stdint.h>

namespace stackcube {

enum Button : uint32_t {
    Up = 1u << 0, Down = 1u << 1, Left = 1u << 2, Right = 1u << 3,
    Accept = 1u << 4, Back = 1u << 5, Menu = 1u << 6, Select = 1u << 7,
    FaceX = 1u << 8, FaceY = 1u << 9, L1 = 1u << 10, R1 = 1u << 11,
    L3 = 1u << 12, R3 = 1u << 13,
};

struct Gamepad {
    bool connected = false;
    uint32_t buttons = 0;
    int16_t x = 0, y = 0, rightX = 0, rightY = 0;
    uint16_t leftTrigger = 0, rightTrigger = 0;
};

struct Input {
    uint32_t held = 0;
    uint32_t pressed = 0;
    uint8_t key = 0;  // USB HID usage of the latest new key
    char text = 0;    // US keyboard translation of that key
    int16_t mouseX = 0, mouseY = 0;
    int16_t wheel = 0;
    uint8_t mouseButtons = 0;
    bool mouseConnected = false;
    Gamepad pads[4];
    bool keyboardConnected = false;
    uint32_t repeated = 0;
    uint32_t keyboardArrows = 0;
    uint8_t keys[32] = {};
};

}
