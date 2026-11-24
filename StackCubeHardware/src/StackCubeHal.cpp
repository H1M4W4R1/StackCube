#include "StackCubeHal.h"
#include "drivers/Audio.h"
#include "drivers/UsbInput.h"

#include <M5ModuleDisplay.h>
#include <M5Unified.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp32-hal-spi.h>

namespace stackcube::hal {
namespace {
M5ModuleDisplay hdmi(Width, Height, 60.0f, 1280, 720, 2, 2);
constexpr uint8_t IoExpander = 0x58;
constexpr uint8_t InputPort0 = 0x00;
constexpr uint8_t TfSwitch = 1 << 4;
constexpr uint32_t BoardI2cClock = 100000;
constexpr uint32_t SdTransferClock = 8000000;
bool audioMounted;
}

void prepareSdBus() {
    for (int pin : {UsbCs, SdCs, HdmiCs, LcdCs}) digitalWrite(pin, HIGH);
    // The internal LCD also uses GPIO35 as its D/C output.
    spiAttachMISO(SPI.bus(), Miso);
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
    // Register the amplifier callback. The playback driver starts it only for game audio.
    config.internal_spk = true;
    config.internal_mic = false;
    config.external_display_value = 0;
    config.external_speaker_value = 0;
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

bool readSdPresence(bool& inserted) {
    uint8_t pins;
    if (!M5.In_I2C.readRegister(IoExpander, InputPort0, &pins, 1, BoardI2cClock)) return false;
    inserted = (pins & TfSwitch) == 0;
    return true;
}

bool mountSd() {
    prepareSdBus();
    if (SD.begin(SdCs, SPI, SdTransferClock)) return true;
    bool inserted;
    if (!readSdPresence(inserted) || !inserted) return false;
    Serial.println("SD mount failed, restarting slot power");
    if (!restartSdPower()) return false;
    prepareSdBus();
    return SD.begin(SdCs, SPI, SdTransferClock);
}

const Input& poll() {
    if (audioMounted) hdmi.waitDMA();
    const auto& input = usb::poll();
    if (audioMounted) audio::poll();
    return input;
}

int32_t readUsbGpio(uint8_t channel) {
    hdmi.waitDMA();
    return usb::readGpio(channel);
}

bool writeUsbGpio(uint8_t channel, bool high) {
    hdmi.waitDMA();
    return usb::writeGpio(channel, high);
}

void resetUsbGpioOutputs() {
    hdmi.waitDMA();
    usb::resetGpioOutputs();
}

bool setRumble(uint8_t slot, uint8_t left, uint8_t right) { return usb::setRumble(slot, left, right); }
void stopRumble() { usb::stopRumble(); }

void beginGameAudio(const GameLaunch& launch) {
    if (audioMounted || launch.audioOutput == AudioOutput::Off || !launch.audioLevels.audible()) return;
    audioMounted = audio::begin(launch);
    if (!audioMounted) Serial.println("Audio: could not start output");
}

void endGameAudio() {
    if (!audioMounted) return;
    audio::end();
    audioMounted = false;
}

bool preloadSfx(const char* name) {
    if (!audioMounted) return false;
    hdmi.waitDMA();
    return audio::preloadSfx(name);
}

bool playSfx(const char* name) { return audio::playSfx(name); }
bool playBgTrack(const char* name) { return audio::playBgTrack(name); }
void stopBgTrack() { audio::stopBgTrack(); }
M5GFX& display() { return hdmi; }

}
