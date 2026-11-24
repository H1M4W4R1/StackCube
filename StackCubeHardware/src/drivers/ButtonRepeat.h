#pragma once

#include <stdint.h>

namespace stackcube::usb {

class ButtonRepeat {
public:
    uint32_t update(uint32_t held, uint32_t pressed, uint32_t now) {
        uint32_t repeated = 0;
        for (unsigned i = 0; i < 32; ++i) {
            const auto button = uint32_t{1} << i;
            if (pressed & button) next[i] = now + InitialDelayMs;
            else if ((held & button) && int32_t(now - next[i]) >= 0) {
                repeated |= button;
                next[i] = now + IntervalMs;
            }
        }
        return repeated;
    }

private:
    static constexpr uint32_t InitialDelayMs = 350;
    static constexpr uint32_t IntervalMs = 90;
    uint32_t next[32]{};
};

}
