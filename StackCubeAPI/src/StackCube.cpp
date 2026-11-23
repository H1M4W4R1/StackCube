#include "StackCube.h"

void setup();
void loop();

namespace stackcube {
namespace {
const abi::ServicesV1* services;
Input frameInput{};
bool exitRequested;

uint8_t tick(const abi::InputV1* input) {
    if (exitRequested) return 0;
    frameInput = *input;
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
bool Console::preload_sfx(const char* filename) { return services->preloadSound(filename); }
bool Console::play_sfx(const char* filename) { return services->playSound(filename); }
bool Console::play_bg_track(const char* filename) { return services->playMusic(filename); }
void Console::stop_bg_track() { services->stopMusic(); }

bool Console::returnToLauncher() {
    exitRequested = true;
    return true;
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
