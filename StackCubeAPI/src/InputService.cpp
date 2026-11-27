#include "StackCube.h"

namespace stackcube {
namespace {
constexpr uint32_t DoubleClickMs = 250;
constexpr uint32_t HoldMs = 500;
}

InputService::BindingId InputService::bind(Source source, uint32_t control, uint8_t slot,
                                           Gesture gesture, Action action, void* context) {
    if (!action || lastId == UINT32_MAX) return 0;
    for (auto& binding : bindings) {
        if (binding.id) continue;
        binding = {++lastId, action, context, {gesture, control, slot, 0, 0, 0},
                   source, false, false, false, 0, 0};
        return binding.id;
    }
    return 0;
}

InputService::BindingId InputService::bindKey(uint8_t key, Gesture gesture, Action action, void* context) {
    if (key < 4 || gesture >= Gesture::Move) return 0;
    return bind(Source::Key, key, 0, gesture, action, context);
}

InputService::BindingId InputService::bindMouseButton(MouseButton button, Gesture gesture,
                                                      Action action, void* context) {
    if (button > MouseButton::Forward || gesture >= Gesture::Move) return 0;
    return bind(Source::MouseButton, uint8_t(button), 0, gesture, action, context);
}

InputService::BindingId InputService::bindMouseMove(Action action, void* context) {
    return bind(Source::MouseMove, 0, 0, Gesture::Move, action, context);
}

InputService::BindingId InputService::bindMouseWheel(Action action, void* context) {
    return bind(Source::Wheel, 0, 0, Gesture::Move, action, context);
}

InputService::BindingId InputService::bindGamepadButton(uint8_t slot, Button button, Gesture gesture,
                                                       Action action, void* context) {
    const uint32_t mask = button;
    if (slot >= 4 || !mask || mask > R3 || (mask & (mask - 1)) || gesture >= Gesture::Move) return 0;
    return bind(Source::PadButton, mask, slot, gesture, action, context);
}

InputService::BindingId InputService::bindGamepadStick(Stick stick, Action action, void* context, uint8_t slot) {
    if (slot >= 4 || stick > Stick::Right) return 0;
    return bind(Source::Stick, uint8_t(stick), slot, Gesture::Move, action, context);
}

bool InputService::unbind(BindingId id) {
    if (!id) return false;
    for (auto& binding : bindings) {
        if (binding.id != id) continue;
        binding = {};
        return true;
    }
    return false;
}

void InputService::clear() {
    for (auto& binding : bindings) binding = {};
}

bool InputService::updateButton(Binding& binding, bool down, uint32_t now) {
    const auto gesture = binding.event.gesture;
    const bool pressed = down && !binding.down;
    const bool released = !down && binding.down;
    binding.down = down;
    if (pressed) {
        binding.downAt = now;
        binding.held = false;
    }
    if (gesture == Gesture::Down) return pressed;
    if (gesture == Gesture::Up) return released;
    if (gesture == Gesture::Hold) {
        if (!down || binding.held || now - binding.downAt < HoldMs) return false;
        binding.held = true;
        return true;
    }

    const bool expired = binding.pending && now - binding.clickAt > DoubleClickMs;
    if (expired) binding.pending = false;
    if (released && now - binding.downAt < HoldMs) {
        if (binding.pending) {
            binding.pending = false;
            return gesture == Gesture::Double;
        }
        binding.pending = true;
        binding.clickAt = now;
    }
    return expired && gesture == Gesture::Single;
}

void InputService::dispatch() {
    if (dispatching) return;
    dispatching = true;
    const auto& frame = snapshot();
    const auto now = Console{}.millis();
    const auto last = lastId;
    for (auto& binding : bindings) {
        if (!binding.id || binding.id > last) continue;
        auto event = binding.event;
        const auto& pad = frame.pads[event.slot];
        bool fire = false;
        bool connected = true;
        switch (binding.source) {
            case Source::Key:
                connected = frame.keyboardConnected;
                fire = connected && updateButton(binding, frame.keyDown(event.control), now);
                break;
            case Source::MouseButton:
                connected = frame.mouseConnected;
                fire = connected && updateButton(binding, frame.mouseButtons & (1u << event.control), now);
                break;
            case Source::PadButton:
                connected = pad.connected;
                fire = connected && updateButton(binding, pad.buttons & event.control, now);
                break;
            case Source::MouseMove:
                event.x = frame.mouseX;
                event.y = frame.mouseY;
                fire = event.x || event.y;
                break;
            case Source::Wheel:
                event.wheel = frame.wheel;
                fire = event.wheel != 0;
                break;
            case Source::Stick:
                if (pad.connected) {
                    const bool right = event.control == uint8_t(Stick::Right);
                    event.x = right ? pad.rightX : pad.x;
                    event.y = right ? pad.rightY : pad.y;
                }
                fire = event.x != binding.event.x || event.y != binding.event.y;
                binding.event = event;
                break;
        }
        if (!connected) binding.down = binding.pending = binding.held = false;
        if (fire) binding.action(event, binding.context);
    }
    dispatching = false;
}
}
