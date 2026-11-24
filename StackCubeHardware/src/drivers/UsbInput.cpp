#include "UsbInput.h"
#include "ButtonRepeat.h"
#include "HidGamepad.h"

#include <XBOXONE.h>
#include <XBOXUSB.h>
#include <hidcomposite.h>
#include <usbhub.h>
#include <algorithm>

namespace stackcube::usb {
namespace {
constexpr uint32_t RetryIntervalMs = 1000;
constexpr uint32_t EnumerationTimeoutMs = 2000;
USB host;
USBHub hub(&host);
USBHub secondHub(&host);
Input input;
ButtonRepeat buttonRepeat;
bool ready;
uint32_t retryAt;
uint32_t stateSince;
uint8_t lastHostState;

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

    uint32_t collectInput() const {
        uint32_t buttons = 0;
        for (const auto& iface : interfaces) {
            if (iface.protocol == USB_HID_PROTOCOL_KEYBOARD) {
                input.keyboardConnected = true;
                buttons |= iface.held;
                for (const auto key : iface.keys) {
                    if (key >= 0x4f && key <= 0x52) input.keyboardArrows |= keyboardButton(key);
                }
            } else if (iface.protocol == USB_HID_PROTOCOL_MOUSE) {
                input.mouseConnected = true;
                input.mouseButtons |= iface.mouseButtons;
            }
        }
        return buttons;
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

class XboxOnePad : public XBOXONE {
public:
    XboxOnePad() : XBOXONE(&host) {}

    uint8_t Init(uint8_t parent, uint8_t port, bool lowspeed) override {
        const uint8_t result = XBOXONE::Init(parent, port, lowspeed);
        if (!result) Serial.printf("Xbox One gamepad attached (USB address %u)\n", GetAddress());
        return result;
    }

    uint8_t Poll() override {
        const uint32_t previousPoll = qNextPollTime;
        const uint8_t result = XBOXONE::Poll();
        if (qNextPollTime != previousPoll) {
            // NAK is normal when an idle controller has no report ready.
            if (!result || result == hrNAK) errors = 0;
            else if (errors < ErrorLimit) ++errors;
        }
        const uint32_t now = millis();
        if (recovering && now - restartedAt < SessionTimeoutMs && int32_t(now - nextStartup) >= 0) {
            uint8_t powerOn[] = {0x05, 0x20, 0x00, 0x01, 0x00};
            if (!XboxCommand(powerOn, sizeof(powerOn))) finishStartup();
            nextStartup = millis() + StartupIntervalMs;
        }
        return result;
    }

    uint8_t Release() override {
        errors = 0;
        oneS = sessionReady = recovering = false;
        return XBOXONE::Release();
    }

    bool acceptsInput() const { return XboxOneConnected && (!oneS || sessionReady); }
    bool transferFailed() const { return errors == ErrorLimit; }
    bool sessionTimedOut(uint32_t now) const { return recovering && now - restartedAt >= SessionTimeoutMs; }

protected:
    uint8_t StartInputSession(uint16_t vendor, uint16_t product) override {
        oneS = vendor == XBOX_VID1 && product == XBOX_ONE_PID4;
        sessionReady = recovering = false;
        if (!oneS) return 0;
        const auto result = finishStartup();
        nextStartup = millis() + StartupIntervalMs;
        return result;
    }

    void OnInputPacket(const uint8_t* data, uint16_t length) override {
        if (!oneS || length < 4 || (data[1] & 0x0f)) return;
        if (data[0] == 0x20 && length >= 18) {
            markReady();
        } else if (data[1] & 0x20) {
            if (data[0] == 0x02 && length >= 20) {
                restartSession("controller announced itself again");
            } else if (data[0] == 0x03 && length >= 8) {
                if (data[4] & 0x80) markReady();
                else restartSession("controller reported its session closed");
            }
        }
    }

private:
    static constexpr uint8_t ErrorLimit = 3;
    static constexpr uint32_t StartupIntervalMs = 400;
    static constexpr uint32_t SessionTimeoutMs = 3000;
    uint8_t errors = 0;
    bool oneS = false, sessionReady = false, recovering = false;
    uint32_t restartedAt = 0, nextStartup = 0;

    uint8_t finishStartup() {
        // Xbox One S wired start commands from Linux xpad, excluding rumble.
        uint8_t wiredMode[] = {0x05, 0x20, 0x00, 0x0f, 0x06};
        uint8_t ledOn[] = {0x0a, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14};
        uint8_t authDone[] = {0x06, 0x20, 0x00, 0x02, 0x01, 0x00};
        uint8_t result = XboxCommand(wiredMode, sizeof(wiredMode));
        if (!result) result = XboxCommand(ledOn, sizeof(ledOn));
        if (!result) result = XboxCommand(authDone, sizeof(authDone));
        return result;
    }

    void restartSession(const char* reason) {
        if (recovering) return;
        ResetInputState();
        sessionReady = false;
        recovering = true;
        restartedAt = millis();
        Serial.printf("Xbox One S input restart: %s\n", reason);
    }

    void markReady() {
        if (!sessionReady) Serial.println("Xbox One S input ready");
        sessionReady = true;
        recovering = false;
    }
};

XboxOnePad xboxOne;
XboxOnePad secondXboxOne;
XBOXUSB xbox360(&host);
XBOXUSB secondXbox360(&host);
HidDevice hid[4];
const void* owners[4] = {};
bool rumbling[4] = {};

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

void serviceHost() {
    const char* failure = "initialization failed";
    uint32_t now = millis();
    if (ready) {
        host.Task();
        now = millis();
        const uint8_t state = host.getUsbTaskState();
        if (state != lastHostState) {
            lastHostState = state;
            stateSince = now;
        }
        const bool enumerating = state >= USB_ATTACHED_SUBSTATE_SETTLE && state <= USB_STATE_CONFIGURING;
        if (state == USB_STATE_ERROR || state == USB_DETACHED_SUBSTATE_ILLEGAL ||
            (enumerating && now - stateSince >= EnumerationTimeoutMs)) {
            failure = "enumeration failed";
        } else if (xboxOne.transferFailed() || secondXboxOne.transferFailed()) {
            failure = "Xbox input transfer failed";
        } else if (xboxOne.sessionTimedOut(now) || secondXboxOne.sessionTimedOut(now)) {
            failure = "Xbox One S input session timed out";
        } else {
            return;
        }
    }
    if (int32_t(now - retryAt) < 0) return;

    Serial.printf("USB host retry: %s\n", failure);
    // Reset enumeration state and release driver instances even when the bus
    // never signalled a physical disconnect.
    for (auto& device : hid) device.Release();
    xboxOne.Release();
    secondXboxOne.Release();
    xbox360.Release();
    secondXbox360.Release();
    secondHub.Release();
    hub.Release();
    host.ForEachUsbDevice([](UsbDevice* device) {
        host.GetAddressPool().FreeAddress(device->address.devAddress);
    });
    host.ResetHubPreMask();
    begin();
}
}

bool begin() {
    host.setUsbTaskState(USB_DETACHED_SUBSTATE_INITIALIZE);
    ready = host.Init() == 0;
    lastHostState = host.getUsbTaskState();
    stateSince = millis();
    retryAt = stateSince + RetryIntervalMs;
    return ready;
}

const Input& poll() {
    input.key = 0;
    input.text = 0;
    input.mouseX = input.mouseY = input.wheel = 0;
    input.mouseButtons = 0;
    input.mouseConnected = false;
    input.keyboardConnected = false;
    input.keyboardArrows = 0;
    serviceHost();

    const PadSource sources[] = {
        {&xboxOne, xboxState(xboxOne, xboxOne.acceptsInput(), 1023)},
        {&secondXboxOne, xboxState(secondXboxOne, secondXboxOne.acceptsInput(), 1023)},
        {&xbox360, xboxState(xbox360, xbox360.Xbox360Connected, 255)},
        {&secondXbox360, xboxState(secondXbox360, secondXbox360.Xbox360Connected, 255)},
        {&hid[0], hid[0].pad}, {&hid[1], hid[1].pad}, {&hid[2], hid[2].pad}, {&hid[3], hid[3].pad},
    };
    for (auto& owner : owners) {
        for (const auto& source : sources) {
            if (owner == source.device && !source.state.connected) {
                rumbling[&owner - owners] = false;
                owner = nullptr;
            }
        }
    }
    uint32_t held = 0;
    for (const auto& device : hid) held |= device.collectInput();
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
    input.repeated = buttonRepeat.update(held, input.pressed, millis());
    input.held = held;
    return input;
}

bool setRumble(uint8_t slot, uint8_t left, uint8_t right) {
    if (slot >= std::size(owners) || !owners[slot]) return false;
    const auto* owner = owners[slot];
    if (owner == &xboxOne || owner == &secondXboxOne) {
        auto& controller = owner == &xboxOne ? xboxOne : secondXboxOne;
        if (!left && !right) controller.setRumbleOff();
        else controller.setRumbleOn(0, 0, (left * 100u + 127) / 255, (right * 100u + 127) / 255);
    } else if (owner == &xbox360 || owner == &secondXbox360) {
        auto& controller = owner == &xbox360 ? xbox360 : secondXbox360;
        controller.setRumbleOn(left, right);
    } else {
        return false;
    }
    rumbling[slot] = left || right;
    return true;
}

void stopRumble() {
    for (uint8_t slot = 0; slot < std::size(owners); ++slot) {
        if (rumbling[slot]) setRumble(slot, 0, 0);
    }
}
}
