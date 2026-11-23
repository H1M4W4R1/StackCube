#include "StackCubeHal.h"
#include "drivers/UsbInput.h"

#include <M5ModuleDisplay.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp32-hal-spi.h>

namespace stackcube::hal {
namespace {
M5ModuleDisplay hdmi(Width, Height, 60.0f, 1280, 720, 2, 2);
constexpr uint8_t IoExpander = 0x58;
constexpr uint8_t InputPort0 = 0x00;
constexpr uint8_t TfSwitch = 1 << 4;
constexpr uint32_t BoardI2cClock = 100000;
}

bool begin() {
    beginBoard();
    return beginPeripherals();
}

void beginBoard() {
    Serial.begin(115200);
    for (int pin : {UsbCs, SdCs, HdmiCs, LcdCs}) {
        digitalWrite(pin, HIGH);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    auto config = M5.config();
    config.serial_baudrate = 0;
    config.internal_spk = false;
    config.internal_mic = false;
    config.external_display_value = 0;
    M5.begin(config);
    M5.Display.sleep();
    SPI.begin(Sck, Miso, Mosi, SdCs);

    constexpr auto psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    constexpr auto dma = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    Serial.printf("PSRAM: %zu total, %zu free, %zu largest block (bytes)\n",
                  heap_caps_get_total_size(psram), heap_caps_get_free_size(psram),
                  heap_caps_get_largest_free_block(psram));
    Serial.printf("Internal DMA heap: %zu free, %zu largest block (bytes)\n",
                  heap_caps_get_free_size(dma), heap_caps_get_largest_free_block(dma));
}

bool beginPeripherals() {
    // M5GFX and Arduino use SPI2 on CoreS3. Call them only from the Arduino loop.
    const bool displayReady = hdmi.init();
    prepareSdBus();
    Serial.println(usb::begin() ? "MAX3421E ready" : "MAX3421E failed to initialize");
    Serial.println(displayReady ? "HDMI ready" : "HDMI initialization failed");
    return displayReady;
}

void prepareSdBus() {
    for (int pin : {UsbCs, SdCs, HdmiCs, LcdCs}) digitalWrite(pin, HIGH);
    // The internal LCD also uses GPIO35 as its D/C output.
    spiAttachMISO(SPI.bus(), Miso);
}

bool readSdPresence(bool& inserted) {
    uint8_t pins;
    if (!M5.In_I2C.readRegister(IoExpander, InputPort0, &pins, 1, BoardI2cClock)) return false;
    inserted = (pins & TfSwitch) == 0;
    return true;
}

bool restartSdPower() {
    // ALDO4 is the dedicated slot supply. The caller has already released the failed mount.
    constexpr uint8_t LdoEnable = 0x90;
    constexpr uint8_t Aldo4 = 1 << 3;
    auto& pmic = M5.Power.Axp2101;
    const bool disabled = pmic.bitOff(LdoEnable, Aldo4);
    if (disabled) delay(30);
    const bool enabled = pmic.bitOn(LdoEnable, Aldo4);
    if (enabled) delay(10);
    return disabled && enabled;
}

const Input& poll() { return usb::poll(); }
bool setRumble(uint8_t slot, uint8_t left, uint8_t right) { return usb::setRumble(slot, left, right); }
void stopRumble() { usb::stopRumble(); }
M5GFX& display() { return hdmi; }

}
