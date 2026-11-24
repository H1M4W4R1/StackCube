#include "StackCube.h"

void setup();
void loop();

namespace stackcube {
namespace {
const abi::ServicesV1* services;
Input frameInput{};
bool exitRequested;

bool hasServicesV2() {
    return services && services->size >= sizeof(abi::ServicesV2);
}

const abi::DinServices* dinServices() {
    return hasServicesV2() ? &reinterpret_cast<const abi::ServicesV2*>(services)->din : nullptr;
}

const abi::ServicesV3* usbGpioServices() {
    return services && services->size >= sizeof(abi::ServicesV3)
        ? reinterpret_cast<const abi::ServicesV3*>(services) : nullptr;
}

uint8_t tick(const abi::InputV1* input) {
    if (exitRequested) return 0;
    static_cast<abi::InputV1&>(frameInput) = *input;
    frameInput.repeated = hasServicesV2() ? reinterpret_cast<const abi::InputV2*>(input)->repeated : 0;
    loop();
    return !exitRequested;
}
}

extern "C" int stackcube_game_entry(int argc, char** argv) {
    if (argc != 1 || !argv || !argv[0]) return -1;
    auto& launch = *reinterpret_cast<abi::LaunchV1*>(argv[0]);
    if (!launch.services || launch.services->version != abi::Version1 ||
        launch.services->size < sizeof(abi::ServicesV1)) return -1;
    services = launch.services;
    launch.game = {setup, tick};
    return 0;
}

bool Console::begin() { return services != nullptr; }
const Input& Console::poll() { return frameInput; }
uint32_t Console::millis() { return services->millis(); }
uint32_t Console::random() { return services->random(); }
void Console::sleep(uint32_t ms) { services->sleep(ms); }
bool Console::setRumble(uint8_t slot, uint8_t left, uint8_t right) {
    return services->setRumble(slot, left, right);
}

int32_t Console::readUsbGpio(uint8_t channel) {
    const auto* usb = usbGpioServices();
    return usb ? usb->readUsbGpio(channel) : -1;
}

bool Console::writeUsbGpio(uint8_t channel, bool high) {
    const auto* usb = usbGpioServices();
    return usb && usb->writeUsbGpio(channel, high);
}

bool Console::preload_sfx(const char* filename) { return services->preloadSound(filename); }
bool Console::play_sfx(const char* filename) { return services->playSound(filename); }
bool Console::play_bg_track(const char* filename) { return services->playMusic(filename); }
void Console::stop_bg_track() { services->stopMusic(); }

bool Console::returnToLauncher() {
    exitRequested = true;
    return true;
}

bool DinPort::pinMode(DinPin pin, PinMode mode) const {
    const auto* din = dinServices();
    return din && din->gpioMode(id, static_cast<uint8_t>(pin), static_cast<uint8_t>(mode));
}

int32_t DinPort::digitalRead(DinPin pin) const {
    const auto* din = dinServices();
    return din ? din->gpioRead(id, static_cast<uint8_t>(pin)) : -1;
}

bool DinPort::digitalWrite(DinPin pin, bool high) const {
    const auto* din = dinServices();
    return din && din->gpioWrite(id, static_cast<uint8_t>(pin), high);
}

bool DinPort::beginUart(uint32_t baud) const {
    const auto* din = dinServices();
    return din && din->uartBegin(id, baud);
}

int32_t DinPort::uartRead(uint8_t* data, uint32_t size) const {
    const auto* din = dinServices();
    return din ? din->uartRead(id, data, size) : -1;
}

int32_t DinPort::uartWrite(const uint8_t* data, uint32_t size) const {
    const auto* din = dinServices();
    return din ? din->uartWrite(id, data, size) : -1;
}

bool DinPort::beginI2c(uint32_t clockHz) const {
    const auto* din = dinServices();
    return din && din->i2cBegin(id, clockHz);
}

I2cProbeResult DinPort::i2cProbe(uint8_t address) const {
    if (!services || services->size < sizeof(abi::ServicesV4)) return I2cProbeResult::Unavailable;
    const auto* extended = reinterpret_cast<const abi::ServicesV4*>(services);
    return extended->i2cProbe(id, address) ? I2cProbeResult::Acknowledged : I2cProbeResult::NoResponse;
}

bool DinPort::i2cTransfer(uint8_t address, const uint8_t* tx, uint32_t txSize, uint8_t* rx, uint32_t rxSize) const {
    const auto* din = dinServices();
    return din && din->i2cTransfer(id, address, tx, txSize, rx, rxSize);
}

void DinPort::end() const {
    if (const auto* din = dinServices()) din->end(id);
}

void Screen::startWrite() { services->beginDraw(); }
void Screen::endWrite() { services->endDraw(); }
void Screen::fillRect(int x, int y, int w, int h, uint16_t color) {
    services->fillRect(x, y, w, h, color);
}
void Screen::drawRect(int x, int y, int w, int h, uint16_t color) {
    services->drawRect(x, y, w, h, color);
}
void Screen::fillCircle(int x, int y, int radius, uint16_t color) {
    services->fillCircle(x, y, radius, color);
}
void Screen::drawString(const char* text, int x, int y) {
    services->drawText(text, x, y, textColor, textScale, textAlign == TextAlign::Center);
}

}
