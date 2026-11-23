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
using Input = abi::InputV1;

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
    uint32_t millis();
    uint32_t random();
    void sleep(uint32_t ms);
    bool setRumble(uint8_t slot, uint8_t left, uint8_t right);
    bool preload_sfx(const char* filename);
    bool play_sfx(const char* filename);
    bool play_bg_track(const char* filename);
    void stop_bg_track();
    bool returnToLauncher();

private:
    Screen display;
};

}
