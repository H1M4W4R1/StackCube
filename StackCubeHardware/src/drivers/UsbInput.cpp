#include "UsbInput.h"
#include "HidGamepad.h"

#include <XBOXONE.h>
#include <XBOXUSB.h>
#include <hidcomposite.h>
#include <usbhub.h>
#include <algorithm>

namespace stackcube::usb {
namespace {
USB host;
USBHub hub(&host);
USBHub secondHub(&host);
Input input;
bool ready;

uint32_t keyboardButton(uint8_t key) {
    switch (key) {
        case 0x1a: case 0x52: return Up;
        case 0x16: case 0x51: return Down;
        case 0x04: case 0x50: return Left;
        case 0x07: case 0x4f: return Right;
        case 0x28: case 0x2c: case 0x58: return Accept;
        case 0x29: return Back;
        case 0x3b: return Menu;
        default: return 0;
    }
}

char keyboardText(uint8_t key, bool shift) {
    if (key >= 4 && key <= 29) return (shift ? 'A' : 'a') + key - 4;
    if (key >= 30 && key <= 39) return shift ? "!@#$%^&*()"[key - 30] : "1234567890"[key - 30];
    if (key >= 44 && key <= 56) return (shift ? " _+{}||:\"~<>?" : " -=[]\\\\;'\x60,./")[key - 44];
    return 0;
}

class HidDevice : public HIDComposite {
public:
    HidDevice() : HIDComposite(&host) {}
    bool SelectInterface(uint8_t, uint8_t) override { return true; }

    uint8_t Release() override {
        for (auto& iface : interfaces) iface = {};
        pad = {};
        return HIDComposite::Release();
    }

    uint32_t keys() const {
        uint32_t buttons = 0;
        for (const auto& iface : interfaces) buttons |= iface.held;
        return buttons;
    }

    void mouse() const {
        for (const auto& iface : interfaces) {
            if (iface.protocol != USB_HID_PROTOCOL_MOUSE) continue;
            input.mouseConnected = true;
            input.mouseButtons |= iface.mouseButtons;
        }
    }

    Gamepad pad;

private:
    struct Interface {
        uint8_t endpoint = 0, protocol = 0;
        uint8_t keys[6] = {};
        uint8_t mouseButtons = 0;
        uint32_t held = 0;
        HidGamepad descriptor;
    };
    Interface interfaces[maxHidInterfaces];

    uint8_t OnInitSuccessful() override {
        for (uint8_t index = 0; index < bNumIface; ++index) {
            const auto& hid = hidInterfaces[index];
            const uint8_t endpoint = hid.epIndex[epInterruptInIndex];
            if (!endpoint) continue;
            auto& iface = interfaces[index];
            iface.endpoint = epInfo[endpoint].epAddr;
            iface.protocol = hid.bmProtocol;
            if (iface.protocol == USB_HID_PROTOCOL_KEYBOARD || iface.protocol == USB_HID_PROTOCOL_MOUSE) {
                if (SetProtocol(hid.bmInterface, USB_HID_BOOT_PROTOCOL)) iface.protocol = 0;
            } else {
                if (!GetReportDescr(hid.bmInterface, &iface.descriptor) && iface.descriptor.available()) {
                    pad.connected = true;
                }
            }
        }
        Serial.printf("HID attached: %04x:%04x\n", VID, PID);
        return 0;
    }

    void ParseHIDData(USBHID*, uint8_t endpoint, bool, uint8_t length, uint8_t* data) override {
        for (auto& iface : interfaces) {
            if (iface.endpoint != endpoint) continue;
            if (iface.protocol == USB_HID_PROTOCOL_KEYBOARD && length >= 8) {
                for (uint8_t i = 2; i < 8; ++i) if (data[i] && data[i] < 4) return;
                iface.held = 0;
                for (uint8_t i = 2; i < 8; ++i) {
                    const uint8_t key = data[i];
                    iface.held |= keyboardButton(key);
                    if (key && std::find(std::begin(iface.keys), std::end(iface.keys), key) == std::end(iface.keys)) {
                        input.key = key;
                        input.text = keyboardText(key, data[0] & 0x22);
                    }
                }
                std::copy(data + 2, data + 8, iface.keys);
            } else if (iface.protocol == USB_HID_PROTOCOL_MOUSE && length >= 3) {
                iface.mouseButtons = data[0];
                input.mouseX += int8_t(data[1]);
                input.mouseY += int8_t(data[2]);
                if (length >= 4) input.wheel += int8_t(data[3]);
            } else if (iface.descriptor.available()) {
                iface.descriptor.apply(pad, data, length);
            }
            break;
        }
    }
};

XBOXONE xboxOne(&host);
XBOXONE secondXboxOne(&host);
XBOXUSB xbox360(&host);
XBOXUSB secondXbox360(&host);
HidDevice hid[4];
const void* owners[4] = {};

template <typename Controller>
Gamepad xboxState(Controller& xbox, bool connected, unsigned triggerMax) {
    Gamepad pad;
    if (!connected) return pad;
    constexpr struct { ButtonEnum xbox; Button button; } mapping[] = {
        {UP, Up}, {DOWN, Down}, {LEFT, Left}, {RIGHT, Right}, {A, Accept}, {B, Back},
        {X, FaceX}, {Y, FaceY}, {START, Menu}, {BACK, Select},
        {::L1, L1}, {::R1, R1},
        {::L3, L3}, {::R3, R3},
    };
    for (const auto& button : mapping) {
        if (xbox.getButtonPress(button.xbox)) pad.buttons |= button.button;
    }
    pad.connected = true;
    pad.x = xbox.getAnalogHat(LeftHatX);
    pad.y = -1 - xbox.getAnalogHat(LeftHatY);
    pad.rightX = xbox.getAnalogHat(RightHatX);
    pad.rightY = -1 - xbox.getAnalogHat(RightHatY);
    pad.leftTrigger = std::min<unsigned>(triggerMax, xbox.getButtonPress(L2)) * 65535 / triggerMax;
    pad.rightTrigger = std::min<unsigned>(triggerMax, xbox.getButtonPress(R2)) * 65535 / triggerMax;
    return pad;
}

struct PadSource {
    const void* device;
    Gamepad state;
};
}

bool begin() {
    ready = host.Init() == 0;
    return ready;
}

const Input& poll() {
    input.key = 0;
    input.text = 0;
    input.mouseX = input.mouseY = input.wheel = 0;
    input.mouseButtons = 0;
    input.mouseConnected = false;
    if (ready) host.Task();

    const PadSource sources[] = {
        {&xboxOne, xboxState(xboxOne, xboxOne.XboxOneConnected, 1023)},
        {&secondXboxOne, xboxState(secondXboxOne, secondXboxOne.XboxOneConnected, 1023)},
        {&xbox360, xboxState(xbox360, xbox360.Xbox360Connected, 255)},
        {&secondXbox360, xboxState(secondXbox360, secondXbox360.Xbox360Connected, 255)},
        {&hid[0], hid[0].pad}, {&hid[1], hid[1].pad}, {&hid[2], hid[2].pad}, {&hid[3], hid[3].pad},
    };
    for (auto& owner : owners) {
        for (const auto& source : sources) {
            if (owner == source.device && !source.state.connected) owner = nullptr;
        }
    }
    uint32_t held = 0;
    for (auto& device : hid) {
        held |= device.keys();
        device.mouse();
    }
    for (auto& pad : input.pads) pad = {};
    for (const auto& source : sources) {
        if (!source.state.connected) continue;
        auto* slot = std::find(std::begin(owners), std::end(owners), source.device);
        if (slot == std::end(owners)) slot = std::find(std::begin(owners), std::end(owners), nullptr);
        if (slot == std::end(owners)) continue;
        *slot = source.device;
        input.pads[slot - owners] = source.state;
        held |= source.state.buttons;
        if (source.state.x < -14500) held |= Left;
        if (source.state.x > 14500) held |= Right;
        if (source.state.y < -14500) held |= Up;
        if (source.state.y > 14500) held |= Down;
    }
    input.pressed = held & ~input.held;
    input.held = held;
    return input;
}
}
