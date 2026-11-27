#pragma once

#include <stddef.h>
#include <stdint.h>

struct _lv_obj_t;

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

struct DinServices {
    uint8_t (*gpioMode)(uint8_t port, uint8_t pin, uint8_t mode);
    int32_t (*gpioRead)(uint8_t port, uint8_t pin);
    uint8_t (*gpioWrite)(uint8_t port, uint8_t pin, uint8_t high);
    uint8_t (*uartBegin)(uint8_t port, uint32_t baud);
    int32_t (*uartRead)(uint8_t port, uint8_t* data, uint32_t size);
    int32_t (*uartWrite)(uint8_t port, const uint8_t* data, uint32_t size);
    uint8_t (*i2cBegin)(uint8_t port, uint32_t clockHz);
    uint8_t (*i2cTransfer)(uint8_t port, uint8_t address, const uint8_t* tx, uint32_t txSize,
                           uint8_t* rx, uint32_t rxSize);
    void (*end)(uint8_t port);
};

// Advertised by ServicesV1::size. The V1 entry point and callback signatures stay intact.
struct ServicesV2 {
    ServicesV1 v1;
    DinServices din;
};

struct ServicesV3 {
    ServicesV2 v2;
    int32_t (*readUsbGpio)(uint8_t channel);
    uint8_t (*writeUsbGpio)(uint8_t channel, uint8_t high);
};

struct ServicesV4 {
    ServicesV3 v3;
    uint8_t (*i2cProbe)(uint8_t port, uint8_t address);
};

struct ServicesV5 {
    ServicesV4 v4;
    uint8_t (*uartConfigure)(uint8_t port, uint32_t baud, uint8_t dataBits, uint8_t parity, uint8_t stopBits);
    uint8_t (*uartTxIdle)(uint8_t port);
};

struct ServicesV6 {
    ServicesV5 v5;
    uint8_t (*uartConfigureWithPinSwap)(uint8_t port, uint32_t baud, uint8_t dataBits, uint8_t parity,
                                        uint8_t stopBits, uint8_t swapPins);
};

struct InputV2 {
    InputV1 v1;
    uint32_t repeated;
};

struct InputV3 {
    InputV2 v2;
    uint8_t keys[32];
};

struct ServicesV7 {
    ServicesV6 v6;
    int32_t (*fileSize)(const char* path);
    int32_t (*readFile)(const char* path, uint32_t offset, void* output, uint32_t size);
    bool (*drawImage)(const char* path, int x, int y, int maxWidth, int maxHeight, float scale);
    void (*drawLine)(int x0, int y0, int x1, int y1, uint16_t color);
    void (*drawCircle)(int x, int y, int radius, uint16_t color);
    void (*fillTriangle)(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void (*pushImage)(int x, int y, int width, int height, const uint16_t* pixels, int32_t transparent);
    void (*drawRoundRect)(int x, int y, int width, int height, int radius, uint16_t color, bool filled);
    void (*setClip)(int x, int y, int width, int height);
    void (*clearClip)();
    _lv_obj_t* (*openUi)();
    void (*closeUi)();
    bool (*removeUiWidget)(_lv_obj_t* object);
};

static_assert(offsetof(ServicesV2, v1) == 0 && offsetof(ServicesV3, v2) == 0 &&
              offsetof(ServicesV4, v3) == 0 && offsetof(ServicesV5, v4) == 0 &&
              offsetof(ServicesV6, v5) == 0 && offsetof(InputV2, v1) == 0);

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
