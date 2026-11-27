#include "StackCube.h"

void setup();
void loop();

namespace stackcube {
namespace {
const abi::ServicesV1* services;
Input frameInput{};
bool exitRequested;
InputService inputService;

const abi::ServicesV7* extendedServices() {
    return services && services->size >= sizeof(abi::ServicesV7)
        ? reinterpret_cast<const abi::ServicesV7*>(services) : nullptr;
}

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

const abi::ServicesV5* uartServices() {
    return services && services->size >= sizeof(abi::ServicesV5)
        ? reinterpret_cast<const abi::ServicesV5*>(services) : nullptr;
}

uint8_t tick(const abi::InputV1* input) {
    if (exitRequested) return 0;
    static_cast<abi::InputV1&>(frameInput) = *input;
    frameInput.repeated = hasServicesV2() ? reinterpret_cast<const abi::InputV2*>(input)->repeated : 0;
    if (extendedServices()) {
        const auto& keys = reinterpret_cast<const abi::InputV3*>(input)->keys;
        for (size_t i = 0; i < sizeof(keys); ++i) frameInput.keys[i] = keys[i];
    }
    inputService.dispatch();
    if (!exitRequested) loop();
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
InputService& Console::input() { return inputService; }
const Input& InputService::snapshot() const { return frameInput; }
bool InputService::setRumble(uint8_t slot, uint8_t left, uint8_t right) const {
    return services->setRumble(slot, left, right);
}

int32_t PortService::readUsbGpio(uint8_t channel) const {
    const auto* usb = usbGpioServices();
    return usb ? usb->readUsbGpio(channel) : -1;
}

bool PortService::writeUsbGpio(uint8_t channel, bool high) const {
    const auto* usb = usbGpioServices();
    return usb && usb->writeUsbGpio(channel, high);
}

bool SoundService::preloadEffect(const char* filename) const { return services->preloadSound(filename); }
bool SoundService::playEffect(const char* filename) const { return services->playSound(filename); }
bool SoundService::playMusic(const char* filename) const { return services->playMusic(filename); }
void SoundService::stopMusic() const { services->stopMusic(); }

int32_t DataService::size(const char* path) const {
    const auto* api = extendedServices();
    return api ? api->fileSize(path) : -1;
}

int32_t DataService::read(const char* path, void* output, uint32_t size, uint32_t offset) const {
    const auto* api = extendedServices();
    return api ? api->readFile(path, offset, output, size) : -1;
}

_lv_obj_t* UIScreenService::open() {
    const auto* api = extendedServices();
    return api ? api->openUi() : nullptr;
}

void UIScreenService::close() {
    if (const auto* api = extendedServices()) api->closeUi();
}

bool UIScreenService::removeWidget(_lv_obj_t* object) {
    const auto* api = extendedServices();
    return api && api->removeUiWidget(object);
}

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

bool DinPort::beginUart(const UartSettings& settings) const {
    if (services && services->size >= sizeof(abi::ServicesV6)) {
        const auto* extended = reinterpret_cast<const abi::ServicesV6*>(services);
        return extended->uartConfigureWithPinSwap(
            id, settings.baud, settings.dataBits, static_cast<uint8_t>(settings.parity),
            static_cast<uint8_t>(settings.stopBits), settings.swapPins);
    }
    if (settings.swapPins) return false;
    if (const auto* uart = uartServices()) {
        return uart->uartConfigure(id, settings.baud, settings.dataBits, static_cast<uint8_t>(settings.parity),
                                   static_cast<uint8_t>(settings.stopBits));
    }
    return settings.dataBits == 8 && settings.parity == UartParity::None && settings.stopBits == UartStopBits::One &&
           beginUart(settings.baud);
}

int32_t DinPort::uartRead(uint8_t* data, uint32_t size) const {
    const auto* din = dinServices();
    return din ? din->uartRead(id, data, size) : -1;
}

int32_t DinPort::uartWrite(const uint8_t* data, uint32_t size) const {
    const auto* din = dinServices();
    return din ? din->uartWrite(id, data, size) : -1;
}

bool DinPort::uartTxIdle() const {
    const auto* uart = uartServices();
    return uart && uart->uartTxIdle(id);
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

void RawScreenService::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    if (const auto* api = extendedServices()) api->drawLine(x0, y0, x1, y1, color);
}

void RawScreenService::drawCircle(int x, int y, int radius, uint16_t color) {
    if (const auto* api = extendedServices()) api->drawCircle(x, y, radius, color);
}

void RawScreenService::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (const auto* api = extendedServices()) api->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void RawScreenService::pushImage(int x, int y, int width, int height, const uint16_t* rgb565) {
    if (const auto* api = extendedServices()) api->pushImage(x, y, width, height, rgb565, -1);
}

void RawScreenService::pushImage(int x, int y, int width, int height,
                                 const uint16_t* rgb565, uint16_t transparent) {
    if (const auto* api = extendedServices()) api->pushImage(x, y, width, height, rgb565, transparent);
}

void RawScreenService::drawRoundRect(int x, int y, int width, int height, int radius, uint16_t color) {
    if (const auto* api = extendedServices()) api->drawRoundRect(x, y, width, height, radius, color, false);
}

void RawScreenService::fillRoundRect(int x, int y, int width, int height, int radius, uint16_t color) {
    if (const auto* api = extendedServices()) api->drawRoundRect(x, y, width, height, radius, color, true);
}

void RawScreenService::setClip(int x, int y, int width, int height) {
    if (const auto* api = extendedServices()) api->setClip(x, y, width, height);
}

void RawScreenService::clearClip() {
    if (const auto* api = extendedServices()) api->clearClip();
}

bool RawScreenService::drawImage(const char* path, int x, int y, int maxWidth, int maxHeight, float scale) {
    const auto* api = extendedServices();
    return api && api->drawImage(path, x, y, maxWidth, maxHeight, scale);
}

}
