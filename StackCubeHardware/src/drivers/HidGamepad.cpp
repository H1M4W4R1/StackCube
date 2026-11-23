#include "HidGamepad.h"
#include <algorithm>

namespace stackcube::usb {
namespace {
constexpr uint32_t buttons[] = {
    Accept, Back, FaceX, FaceY, L1, R1, Select, Menu, L3, R3, 0, 0, 0, 0
};
constexpr uint32_t directions[] = {
    Up, Up | Right, Right, Down | Right, Down, Down | Left, Left, Up | Left
};
}

void HidGamepad::Parse(const uint16_t length, const uint8_t* data, const uint16_t&) {
    for (uint16_t i = 0; i < length; ++i) {
        const uint8_t byte = data[i];
        if (skip) {
            --skip;
        } else if (prefix == 0xfe) {
            skip = byte + 1; // Long item tag and payload
            prefix = 0;
        } else if (!bytes) {
            prefix = byte;
            bytes = byte & 3;
            if (bytes == 3) bytes = 4;
            read = 0;
            value = 0;
            if (!bytes && prefix != 0xfe) item(prefix & 0xfc, 0, 0);
        } else {
            value |= uint32_t(byte) << (read++ * 8);
            if (read != bytes) continue;
            const unsigned shift = 32 - bytes * 8;
            item(prefix & 0xfc, value, int32_t(value << shift) >> shift);
            bytes = 0;
        }
    }
}

HidGamepad::Report* HidGamepad::currentReport() {
    for (uint8_t i = 0; i < reportCount; ++i) {
        if (reports[i].id == global.report) return &reports[i];
    }
    if (reportCount == sizeof(reports) / sizeof(reports[0])) return nullptr;
    reports[reportCount].id = global.report;
    return &reports[reportCount++];
}

void HidGamepad::item(uint8_t tag, uint32_t data, int32_t signedData) {
    switch (tag) {
        case 0x04: global.page = data; break;
        case 0x14: global.minimum = signedData; break;
        case 0x24: global.maximum = global.minimum < 0 ? signedData : int32_t(data); break;
        case 0x74: global.size = data; break;
        case 0x84: global.report = data; numbered = true; break;
        case 0x94: global.count = data; break;
        case 0xa4:
            if (stackDepth < 4) saved[stackDepth++] = global;
            break;
        case 0xb4:
            if (stackDepth) global = saved[--stackDepth];
            break;
        case 0x08:
            if (usageCount < 24) usages[usageCount++] = data;
            break;
        case 0x18: usageMin = data; break;
        case 0xa0:
            ++depth;
            if (data == 1 && global.page == 1 && usageCount &&
                (usages[0] == 4 || usages[0] == 5)) gamepadDepth = depth;
            break;
        case 0xc0:
            if (depth == gamepadDepth) gamepadDepth = 0;
            if (depth) --depth;
            break;
        case 0x80:
            if (auto* report = currentReport()) {
                for (uint16_t i = 0; i < global.count; ++i) {
                    const uint16_t usage = i < usageCount ? usages[i] : usageMin + i;
                    if (gamepadDepth && (data & 3) == 2 && global.size <= 32 &&
                        report->bits < 512) field(report->bits, usage);
                    if (report->bits < 512) report->bits += global.size;
                }
            }
            break;
    }
    if ((tag & 0x0c) == 0) {
        usageCount = 0;
        usageMin = 0;
    }
}

void HidGamepad::field(uint16_t bit, uint16_t usage) {
    if (!global.size || fieldCount == sizeof(fields) / sizeof(fields[0])) return;
    uint8_t control;
    if (global.page == 9 && usage && usage <= sizeof(buttons) / sizeof(buttons[0])) {
        control = FirstButton + usage - 1;
    } else if (global.page == 1) {
        switch (usage) {
            case 0x30: control = X; break;
            case 0x31: control = Y; break;
            case 0x32: control = Z; break;
            case 0x33: control = RX; break;
            case 0x34: control = RY; break;
            case 0x35: control = RZ; break;
            case 0x39: control = Hat; break;
            default: return;
        }
    } else {
        return;
    }
    fields[fieldCount++] = {bit, global.size, global.report, control,
                            global.minimum, global.maximum};
}

void HidGamepad::apply(Gamepad& pad, const uint8_t* data, uint8_t length) const {
    if (!length) return;
    uint8_t id = 0;
    if (numbered) {
        id = *data++;
        --length;
    }
    for (uint8_t i = 0; i < fieldCount; ++i) {
        const auto& f = fields[i];
        if (f.report != id || f.bit + f.size > length * 8) continue;
        uint32_t raw = 0;
        for (unsigned b = 0; b < f.size; ++b) {
            const auto bit = f.bit + b;
            raw |= uint32_t((data[bit / 8] >> (bit % 8)) & 1) << b;
        }
        const unsigned shift = 32 - f.size;
        const int64_t v = f.minimum < 0 ? int32_t(raw << shift) >> shift : int64_t(raw);
        if (f.control >= FirstButton) {
            const uint32_t button = buttons[f.control - FirstButton];
            pad.buttons = raw ? pad.buttons | button : pad.buttons & ~button;
        } else if (f.control == Hat) {
            pad.buttons &= ~(Up | Down | Left | Right);
            const int64_t dir = v - f.minimum;
            if (dir >= 0 && dir < 8) pad.buttons |= directions[dir];
        } else if (f.maximum > f.minimum) {
            const int64_t axis = (v - f.minimum) * 65535 / (int64_t(f.maximum) - f.minimum) - 32768;
            const int16_t normalized = std::clamp<int64_t>(axis, -32768, 32767);
            switch (f.control) {
                case X: pad.x = normalized; break;
                case Y: pad.y = normalized; break;
                case RX: case Z: pad.rightX = normalized; break;
                case RY: case RZ: pad.rightY = normalized; break;
            }
        }
    }
}
}
