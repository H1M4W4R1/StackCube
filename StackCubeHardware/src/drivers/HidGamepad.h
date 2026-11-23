#pragma once

#include "../StackCubeInput.h"
#include <usbhid.h>

namespace stackcube::usb {

class HidGamepad : public USBReadParser {
public:
    void Parse(const uint16_t length, const uint8_t* data, const uint16_t& offset) override;
    void apply(Gamepad& pad, const uint8_t* report, uint8_t length) const;
    bool available() const { return fieldCount != 0; }

private:
    enum Control : uint8_t { X, Y, RX, RY, Hat, Z, RZ, FirstButton };
    struct Field {
        uint16_t bit;
        uint8_t size, report, control;
        int32_t minimum, maximum;
    };
    struct Globals {
        uint16_t page = 0;
        int32_t minimum = 0, maximum = 0;
        uint8_t size = 0, count = 0, report = 0;
    };
    struct Report {
        uint8_t id = 0;
        uint16_t bits = 0;
    };

    void item(uint8_t tag, uint32_t value, int32_t signedValue);
    void field(uint16_t bit, uint16_t usage);
    Report* currentReport();

    Globals global;
    Globals saved[4];
    Report reports[8];
    Field fields[24];
    uint16_t usages[24], usageMin = 0;
    uint8_t fieldCount = 0, usageCount = 0, reportCount = 0, stackDepth = 0;
    uint8_t depth = 0, gamepadDepth = 0;
    uint8_t prefix = 0, bytes = 0, read = 0;
    uint16_t skip = 0;
    uint32_t value = 0;
    bool numbered = false;
};

}
