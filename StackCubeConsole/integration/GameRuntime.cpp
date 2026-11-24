#include "GameRuntime.h"

#include <SD.h>
#include <StackCubeDinPorts.h>
#include <StackCubeHal.h>
#include <cstring>
#include <memory>

namespace {
using namespace stackcube;

static_assert(hal::Width == 640 && hal::Height == 360);
constexpr size_t MaxModuleBytes = 4 * 1024 * 1024;
bool drawing;

void beginDraw() {
    if (drawing) return;
    hal::display().startWrite();
    drawing = true;
}

void endDraw() {
    if (!drawing) return;
    hal::display().endWrite();
    drawing = false;
}

constexpr abi::ServicesV1 baseServices{
    abi::Version1, sizeof(abi::ServicesV3),
    []() -> uint32_t { return millis(); }, esp_random,
    [](uint32_t ms) { delay(ms); },
    beginDraw, endDraw,
    [](int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        hal::display().fillRect(x, y, w, h, color);
    },
    [](int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        hal::display().drawRect(x, y, w, h, color);
    },
    [](int32_t x, int32_t y, int32_t radius, uint16_t color) {
        hal::display().fillCircle(x, y, radius, color);
    },
    [](const char* text, int32_t x, int32_t y, uint16_t color, uint8_t scale, uint8_t centered) {
        auto& screen = hal::display();
        screen.setTextColor(color);
        screen.setTextSize(scale);
        screen.setTextDatum(centered ? lgfx::textdatum_t::top_center : lgfx::textdatum_t::top_left);
        screen.drawString(text, x, y);
    },
    [](uint8_t slot, uint8_t left, uint8_t right) -> uint8_t {
        endDraw();
        return hal::setRumble(slot, left, right);
    },
    [](const char* name) -> uint8_t {
        endDraw();
        return hal::preloadSfx(name);
    },
    [](const char* name) -> uint8_t { return hal::playSfx(name); },
    [](const char* name) -> uint8_t { return hal::playBgTrack(name); },
    hal::stopBgTrack,
};

constexpr abi::ServicesV3 services{
    {baseServices, {
        hal::din::gpioMode, hal::din::gpioRead, hal::din::gpioWrite,
        hal::din::uartBegin, hal::din::uartRead, hal::din::uartWrite,
        hal::din::i2cBegin, hal::din::i2cTransfer, hal::din::end,
    }},
    [](uint8_t channel) -> int32_t {
        endDraw();
        return hal::readUsbGpio(channel);
    },
    [](uint8_t channel, uint8_t high) -> uint8_t {
        endDraw();
        return hal::writeUsbGpio(channel, high != 0);
    },
};

esp_elf_symbol_table_t imports[] = {
    ESP_ELFSYM_EXPORT(memcpy), ESP_ELFSYM_EXPORT(memmove),
    ESP_ELFSYM_EXPORT(memset), ESP_ELFSYM_EXPORT(snprintf), ESP_ELFSYM_END,
};

abi::InputV1 snapshot(const Input& input) {
    abi::InputV1 frame{input.held, input.pressed, input.mouseX, input.mouseY, input.wheel,
                       input.key, input.text, input.mouseButtons, input.mouseConnected,
                       input.keyboardConnected, 0, {}};
    for (size_t i = 0; i < 4; ++i) {
        const auto& pad = input.pads[i];
        frame.pads[i] = {pad.buttons, pad.x, pad.y, pad.rightX, pad.rightY,
                         pad.leftTrigger, pad.rightTrigger, pad.connected, {}};
    }
    return frame;
}
}

bool GameRuntime::start(const char* path, const stackcube::GameLaunch& launch) {
    if (active()) return false;
    failure = "";
    File file = SD.open(path);
    const size_t bytes = file ? file.size() : 0;
    if (!file || file.isDirectory() || !bytes || bytes > MaxModuleBytes) {
        return fail("Invalid or missing game module");
    }

    std::unique_ptr<uint8_t, decltype(&free)> image(static_cast<uint8_t*>(ps_malloc(bytes)), free);
    if (!image) return fail("Insufficient PSRAM for game module");
    if (file.read(image.get(), bytes) != bytes) return fail("Could not read game module");
    file.close();

    static const int registered = esp_elf_register_symbol(imports);
    if (registered) return fail("Could not register game imports", registered);
    int result = esp_elf_init(&elf);
    if (!result) result = esp_elf_relocate(&elf, image.get(), bytes);
    image.reset();
    if (result) return fail("Could not load game module", result);

    stackcube::abi::LaunchV1 request{&services.v2.v1, {}};
    char* arguments[] = {reinterpret_cast<char*>(&request), nullptr};
    result = esp_elf_request(&elf, 0, 1, arguments);
    if (result || !request.game.start || !request.game.tick) {
        return fail("Incompatible StackCube game ABI", result);
    }

    game = request.game;
    Serial.printf("Launching %s with game ABI %u\n", path, unsigned(services.v2.v1.version));
    stackcube::hal::beginGameAudio(launch);
    game.start();
    endDraw();
    return true;
}

void GameRuntime::poll(const stackcube::Input& input) {
    if (!active()) return;
    const abi::InputV2 frame{snapshot(input), input.repeated};
    const bool keepPlaying = game.tick(&frame.v1);
    endDraw();
    if (!keepPlaying) stop();
}

void GameRuntime::stop() {
    game = {};
    esp_elf_deinit(&elf);
    endDraw();
    stackcube::hal::din::endAll();
    stackcube::hal::resetUsbGpioOutputs();
    stackcube::hal::stopRumble();
    stackcube::hal::endGameAudio();
}

bool GameRuntime::fail(const char* message, int result) {
    esp_elf_deinit(&elf);
    endDraw();
    stackcube::hal::din::endAll();
    stackcube::hal::resetUsbGpioOutputs();
    failure = message;
    Serial.printf("%s (%d)\n", message, result);
    return false;
}
