#pragma once

#include "StackCubeAbi.h"

namespace stackcube {

enum Button : uint32_t {
    Up = 1u << 0, Down = 1u << 1, Left = 1u << 2, Right = 1u << 3,
    Accept = 1u << 4, Back = 1u << 5, Menu = 1u << 6, Select = 1u << 7,
    FaceX = 1u << 8, FaceY = 1u << 9, L1 = 1u << 10, R1 = 1u << 11,
    L3 = 1u << 12, R3 = 1u << 13,
};

using Gamepad = abi::GamepadV1;
struct Input : abi::InputV1 {
    uint32_t repeated;
};

enum class DinPortId : uint8_t { B = 0, C = 1 };
enum class DinPin : uint8_t { Yellow = 0, White = 1 };
enum class PinMode : uint8_t { Input = 0, InputPullUp = 1, InputPullDown = 2, Output = 3, OpenDrain = 4 };

class DinPort {
public:
    bool pinMode(DinPin pin, PinMode mode) const;
    int32_t digitalRead(DinPin pin) const;
    bool digitalWrite(DinPin pin, bool high) const;
    bool beginUart(uint32_t baud = 115200) const;
    int32_t uartRead(uint8_t* data, uint32_t size) const;
    int32_t uartWrite(const uint8_t* data, uint32_t size) const;
    bool beginI2c(uint32_t clockHz = 100000) const;
    bool i2cTransfer(uint8_t address, const uint8_t* tx, uint32_t txSize, uint8_t* rx, uint32_t rxSize) const;
    void end() const;

private:
    friend class Console;
    constexpr explicit DinPort(DinPortId port) : id(static_cast<uint8_t>(port)) {}
    uint8_t id;
};

constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Navy = 0x000f;
constexpr uint16_t Yellow = 0xffe0;
constexpr uint16_t Green = 0x07e0;

enum class TextAlign : uint8_t { Left, Center };

class Screen {
public:
    static constexpr int width() { return 640; }
    static constexpr int height() { return 360; }
    void startWrite();
    void endWrite();
    void fillScreen(uint16_t color) { fillRect(0, 0, width(), height(), color); }
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void fillCircle(int x, int y, int radius, uint16_t color);
    void drawString(const char* text, int x, int y);
    void setTextColor(uint16_t color) { textColor = color; }
    void setTextSize(uint8_t scale) { textScale = scale; }
    void setTextAlign(TextAlign alignment) { textAlign = alignment; }

private:
    uint16_t textColor = White;
    uint8_t textScale = 1;
    TextAlign textAlign = TextAlign::Left;
};

class Console {
public:
    bool begin();
    const Input& poll();
    Screen& screen() { return display; }
    constexpr DinPort port(DinPortId id) const { return DinPort(id); }
    uint32_t millis();
    uint32_t random();
    void sleep(uint32_t ms);
    bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
    int32_t readUsbGpio(uint8_t channel);
    bool writeUsbGpio(uint8_t channel, bool high);
    bool preload_sfx(const char* filename);
    bool play_sfx(const char* filename);
    bool play_bg_track(const char* filename);
    void stop_bg_track();
    bool returnToLauncher();

private:
    Screen display;
};

}
