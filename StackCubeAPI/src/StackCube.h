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
    uint8_t keys[32];
    bool keyDown(uint8_t usage) const { return keys[usage / 8] & (1u << (usage % 8)); }
};

enum class Gesture : uint8_t { Down, Up, Single, Double, Hold, Move };
enum class Stick : uint8_t { Left, Right };
enum class MouseButton : uint8_t { Left, Right, Middle, Back, Forward };

struct InputEvent {
    Gesture gesture;
    uint32_t control; // HID usage, MouseButton, Button, or Stick
    uint8_t slot;
    int16_t x, y, wheel;
};

class InputService {
public:
    using Action = void (*)(const InputEvent&, void*);
    using BindingId = uint32_t;
    const Input& snapshot() const;
    bool setRumble(uint8_t slot, uint8_t left, uint8_t right) const;
    BindingId bindKey(uint8_t key, Gesture gesture, Action action, void* context = nullptr);
    BindingId bindKeyPressSingle(uint8_t key, Action action, void* context = nullptr) {
        return bindKey(key, Gesture::Single, action, context);
    }
    BindingId bindKeyPressDouble(uint8_t key, Action action, void* context = nullptr) {
        return bindKey(key, Gesture::Double, action, context);
    }
    BindingId bindKeyHold(uint8_t key, Action action, void* context = nullptr) {
        return bindKey(key, Gesture::Hold, action, context);
    }
    BindingId bindMouseButton(MouseButton button, Gesture gesture, Action action, void* context = nullptr);
    BindingId bindMouseMove(Action action, void* context = nullptr);
    BindingId bindMouseWheel(Action action, void* context = nullptr);
    BindingId bindGamepadButton(uint8_t slot, Button button, Gesture gesture, Action action, void* context = nullptr);
    BindingId bindGamepadStick(Stick stick, Action action, void* context = nullptr, uint8_t slot = 0);
    bool unbind(BindingId id);
    void clear();
    void dispatch();

private:
    enum class Source : uint8_t { Key, MouseButton, PadButton, MouseMove, Wheel, Stick };
    struct Binding {
        BindingId id;
        Action action;
        void* context;
        InputEvent event;
        Source source;
        bool down, pending, held;
        uint32_t downAt, clickAt;
    };
    BindingId bind(Source source, uint32_t control, uint8_t slot, Gesture gesture, Action action, void* context);
    static bool updateButton(Binding& binding, bool down, uint32_t now);
    Binding bindings[32]{};
    BindingId lastId = 0;
    bool dispatching = false;
};

enum class DinPortId : uint8_t { B = 0, C = 1 };
enum class DinPin : uint8_t { Yellow = 0, White = 1 };
enum class PinMode : uint8_t { Input = 0, InputPullUp = 1, InputPullDown = 2, Output = 3, OpenDrain = 4 };
enum class I2cProbeResult : uint8_t { NoResponse, Acknowledged, Unavailable };
enum class UartParity : uint8_t { None, Even, Odd };
enum class UartStopBits : uint8_t { One, OneAndHalf, Two };

struct UartSettings {
    uint32_t baud = 115200;
    uint8_t dataBits = 8;
    UartParity parity = UartParity::None;
    UartStopBits stopBits = UartStopBits::One;
    bool swapPins = false;
};

class DinPort {
public:
    bool pinMode(DinPin pin, PinMode mode) const;
    int32_t digitalRead(DinPin pin) const;
    bool digitalWrite(DinPin pin, bool high) const;
    bool beginUart(uint32_t baud = 115200) const;
    bool beginUart(const UartSettings& settings) const;
    int32_t uartRead(uint8_t* data, uint32_t size) const;
    int32_t uartWrite(const uint8_t* data, uint32_t size) const;
    bool uartTxIdle() const;
    bool beginI2c(uint32_t clockHz = 100000) const;
    I2cProbeResult i2cProbe(uint8_t address) const;
    bool i2cTransfer(uint8_t address, const uint8_t* tx, uint32_t txSize, uint8_t* rx, uint32_t rxSize) const;
    void end() const;

private:
    friend class PortService;
    constexpr explicit DinPort(DinPortId port) : id(static_cast<uint8_t>(port)) {}
    uint8_t id;
};

constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Navy = 0x000f;
constexpr uint16_t Yellow = 0xffe0;
constexpr uint16_t Green = 0x07e0;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t(red & 0xf8) << 8) | (uint16_t(green & 0xfc) << 3) | (blue >> 3);
}

enum class TextAlign : uint8_t { Left, Center };

class RawScreenService {
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
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    void drawCircle(int x, int y, int radius, uint16_t color);
    void drawRoundRect(int x, int y, int width, int height, int radius, uint16_t color);
    void fillRoundRect(int x, int y, int width, int height, int radius, uint16_t color);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void pushImage(int x, int y, int width, int height, const uint16_t* pixels);
    void pushImage(int x, int y, int width, int height, const uint16_t* pixels, uint16_t transparent);
    void setClip(int x, int y, int width, int height);
    void clearClip();
    bool drawImage(const char* path, int x = 0, int y = 0, int maxWidth = 0, int maxHeight = 0, float scale = 1);
    void setTextColor(uint16_t color) { textColor = color; }
    void setTextSize(uint8_t scale) { textScale = scale; }
    void setTextAlign(TextAlign alignment) { textAlign = alignment; }

private:
    uint16_t textColor = White;
    uint8_t textScale = 1;
    TextAlign textAlign = TextAlign::Left;
};

class UIScreenService {
public:
    _lv_obj_t* open();
    void close();
    bool removeWidget(_lv_obj_t* object);
};

class ScreenService {
public:
    RawScreenService& raw() { return rawScreen; }
    UIScreenService& ui() { return uiScreen; }

private:
    RawScreenService rawScreen;
    UIScreenService uiScreen;
};

class PortService {
public:
    constexpr DinPort din(DinPortId id) const { return DinPort(id); }
    int32_t readUsbGpio(uint8_t channel) const;
    bool writeUsbGpio(uint8_t channel, bool high) const;
};

class SoundService {
public:
    bool preloadEffect(const char* filename) const;
    bool playEffect(const char* filename) const;
    bool playMusic(const char* filename) const;
    void stopMusic() const;
};

class DataService {
public:
    int32_t size(const char* path) const;
    int32_t read(const char* path, void* output, uint32_t size, uint32_t offset = 0) const;
};

using Screen = RawScreenService;

class Console {
public:
    bool begin();
    const Input& poll();
    ScreenService& screens() { return display; }
    PortService& ports() { return portService; }
    SoundService& sound() { return soundService; }
    DataService& data() { return dataService; }
    InputService& input();
    Screen& screen() { return screens().raw(); }
    constexpr DinPort port(DinPortId id) const { return portService.din(id); }
    uint32_t millis();
    uint32_t random();
    void sleep(uint32_t ms);
    bool setRumble(uint8_t slot, uint8_t left, uint8_t right) { return input().setRumble(slot, left, right); }
    int32_t readUsbGpio(uint8_t channel) { return ports().readUsbGpio(channel); }
    bool writeUsbGpio(uint8_t channel, bool high) { return ports().writeUsbGpio(channel, high); }
    bool preload_sfx(const char* filename) { return sound().preloadEffect(filename); }
    bool play_sfx(const char* filename) { return sound().playEffect(filename); }
    bool play_bg_track(const char* filename) { return sound().playMusic(filename); }
    void stop_bg_track() { sound().stopMusic(); }
    bool returnToLauncher();

private:
    ScreenService display;
    PortService portService;
    SoundService soundService;
    DataService dataService;
};

}
