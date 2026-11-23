#pragma once

#include <stdint.h>

namespace stackcube::abi {

constexpr uint32_t Version1 = 1;

// Keep the layout, signatures and behavior of V1 available to shipped modules.
struct GamepadV1 {
    uint32_t buttons;
    int16_t x, y, rightX, rightY;
    uint16_t leftTrigger, rightTrigger;
    uint8_t connected;
    uint8_t reserved[3];
};

struct InputV1 {
    uint32_t held, pressed;
    int16_t mouseX, mouseY, wheel;
    uint8_t key;
    char text;
    uint8_t mouseButtons, mouseConnected, keyboardConnected, reserved;
    GamepadV1 pads[4];
};

static_assert(sizeof(GamepadV1) == 20 && sizeof(InputV1) == 100);

struct ServicesV1 {
    uint32_t version;
    uint32_t size;
    uint32_t (*millis)();
    uint32_t (*random)();
    void (*sleep)(uint32_t ms);
    void (*beginDraw)();
    void (*endDraw)();
    void (*fillRect)(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void (*drawRect)(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void (*fillCircle)(int32_t x, int32_t y, int32_t radius, uint16_t color);
    void (*drawText)(const char* text, int32_t x, int32_t y, uint16_t color,
                     uint8_t scale, uint8_t centered);
    uint8_t (*setRumble)(uint8_t slot, uint8_t left, uint8_t right);
    uint8_t (*preloadSound)(const char* name);
    uint8_t (*playSound)(const char* name);
    uint8_t (*playMusic)(const char* name);
    void (*stopMusic)();
};

struct GameV1 {
    void (*start)();
    uint8_t (*tick)(const InputV1* input); // Zero requests a return to the launcher.
};

// Passed as argv[0] to the ELF entry. The game writes its callbacks into game.
struct LaunchV1 {
    const ServicesV1* services;
    GameV1 game;
};

}
