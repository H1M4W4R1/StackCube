#include "GameRuntime.h"
#include "../ui/LvglDisplay.h"

#include <SD.h>
#include <StackCubeDinPorts.h>
#include <StackCubeHal.h>
#include <lvgl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace {
using namespace stackcube;

static_assert(hal::Width == 640 && hal::Height == 360);
constexpr size_t MaxModuleBytes = 4 * 1024 * 1024;
bool drawing;
String gameFolder;
lv_obj_t* gameScreen;
lv_obj_t* launcherScreen;
lv_indev_t* gamePointer;
lv_obj_t* pointerCursor;
lv_point_t pointerPosition;
bool pointerDown;
bool deletingScreen;
bool closeRequested;

void beginDraw() {
    if (drawing || gameScreen) return;
    hal::display().startWrite();
    drawing = true;
}

void endDraw() {
    if (!drawing) return;
    hal::display().endWrite();
    drawing = false;
}

String assetPath(const char* name) {
    if (gameFolder.isEmpty() || !name || !*name || strlen(name) > 160) return {};
    for (const char* segment = name; *segment;) {
        const char* end = segment;
        while (*end && *end != '/') {
            if (*end == '\\' || *end == ':' || uint8_t(*end) < 32) return {};
            ++end;
        }
        if (end == segment || end[-1] == '.' || end[-1] == ' ') return {};
        if (!*end) break;
        if (!end[1]) return {};
        segment = end + 1;
    }
    return gameFolder + '/' + name;
}

File openAsset(const char* name) {
    endDraw();
    const auto path = assetPath(name);
    if (path.isEmpty()) return {};
    auto file = SD.open(path.c_str(), FILE_READ);
    if (!file || file.isDirectory()) return {};
    return file;
}

int32_t fileSize(const char* name) {
    auto file = openAsset(name);
    return file && file.size() <= INT32_MAX ? int32_t(file.size()) : -1;
}

int32_t readFile(const char* name, uint32_t offset, void* output, uint32_t size) {
    if ((!output && size) || size > INT32_MAX) return -1;
    auto file = openAsset(name);
    if (!file || offset > file.size()) return -1;
    if (!size || offset == file.size()) return 0;
    if (offset && !file.seek(offset)) return -1;
    const size_t expected = std::min<size_t>(size, file.size() - offset);
    const size_t received = file.read(static_cast<uint8_t*>(output), expected);
    return received == expected ? int32_t(received) : -1;
}

bool drawImage(const char* name, int x, int y, int maxWidth, int maxHeight, float scale) {
    if (gameScreen || maxWidth < 0 || maxHeight < 0 || !std::isfinite(scale) || scale <= 0) return false;
    endDraw();
    auto path = assetPath(name);
    if (path.isEmpty()) return false;
    auto& display = hal::display();
    const char* ext = strrchr(path.c_str(), '.');
    if (!ext) return false;
    if (!strcasecmp(ext, ".png")) return display.drawPngFile(SD, path.c_str(), x, y, maxWidth, maxHeight, 0, 0, scale);
    if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg")) {
        return display.drawJpgFile(SD, path.c_str(), x, y, maxWidth, maxHeight, 0, 0, scale);
    }
    if (!strcasecmp(ext, ".bmp")) return display.drawBmpFile(SD, path.c_str(), x, y, maxWidth, maxHeight, 0, 0, scale);
    return false;
}

lv_obj_t* openUi() {
    if (deletingScreen || gameFolder.isEmpty() || !lv_disp_get_default()) return nullptr;
    if (gameScreen) return gameScreen;
    endDraw();
    hal::display().clearClipRect();
    launcherScreen = lv_scr_act();
    gameScreen = lv_obj_create(nullptr);
    if (!gameScreen) {
        launcherScreen = nullptr;
        return nullptr;
    }
    lv_scr_load(gameScreen);
    static lv_indev_drv_t pointer;
    lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER;
    pointer.read_cb = [](lv_indev_drv_t*, lv_indev_data_t* data) {
        data->point = pointerPosition;
        data->state = pointerDown ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    };
    pointerPosition = {hal::Width / 2, hal::Height / 2};
    pointerDown = false;
    gamePointer = lv_indev_drv_register(&pointer);
    if (gamePointer) {
        lv_timer_set_period(pointer.read_timer, 1);
        pointerCursor = lv_obj_create(gameScreen);
        if (pointerCursor) {
            lv_obj_set_size(pointerCursor, 9, 9);
            lv_obj_set_style_radius(pointerCursor, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(pointerCursor, lv_color_white(), 0);
            lv_obj_set_style_border_color(pointerCursor, lv_color_black(), 0);
            lv_obj_set_style_border_width(pointerCursor, 1, 0);
            lv_obj_clear_flag(pointerCursor, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(pointerCursor, LV_OBJ_FLAG_HIDDEN);
            lv_indev_set_cursor(gamePointer, pointerCursor);
        }
    }
    return gameScreen;
}

void closeUi() {
    if (!gameScreen || deletingScreen) return;
    deletingScreen = true;
    if (gamePointer) lv_indev_delete(gamePointer);
    gamePointer = nullptr;
    if (pointerCursor) lv_obj_del(pointerCursor);
    pointerCursor = nullptr;
    lv_scr_load(launcherScreen);
    lv_obj_del(gameScreen);
    gameScreen = launcherScreen = nullptr;
    deletingScreen = false;
    closeRequested = false;
}

constexpr abi::ServicesV1 baseServices{
    abi::Version1, sizeof(abi::ServicesV7),
    []() -> uint32_t { return millis(); }, esp_random,
    [](uint32_t ms) { delay(ms); },
    beginDraw, endDraw,
    [](int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        if (!gameScreen) hal::display().fillRect(x, y, w, h, color);
    },
    [](int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
        if (!gameScreen) hal::display().drawRect(x, y, w, h, color);
    },
    [](int32_t x, int32_t y, int32_t radius, uint16_t color) {
        if (!gameScreen) hal::display().fillCircle(x, y, radius, color);
    },
    [](const char* text, int32_t x, int32_t y, uint16_t color, uint8_t scale, uint8_t centered) {
        if (gameScreen) return;
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

constexpr abi::ServicesV3 usbServices{
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

constexpr abi::ServicesV7 services{
    {{{usbServices, hal::din::i2cProbe}, hal::din::uartConfigure, hal::din::uartTxIdle},
     hal::din::uartConfigureWithPinSwap},
    fileSize, readFile, drawImage,
    [](int x0, int y0, int x1, int y1, uint16_t color) {
        if (!gameScreen) hal::display().drawLine(x0, y0, x1, y1, color);
    },
    [](int x, int y, int radius, uint16_t color) {
        if (!gameScreen) hal::display().drawCircle(x, y, radius, color);
    },
    [](int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
        if (!gameScreen) hal::display().fillTriangle(x0, y0, x1, y1, x2, y2, color);
    },
    [](int x, int y, int width, int height, const uint16_t* pixels, int32_t transparent) {
        if (gameScreen || !pixels || width <= 0 || height <= 0) return;
        auto* image = reinterpret_cast<const lgfx::rgb565_t*>(pixels);
        if (transparent < 0) hal::display().pushImage(x, y, width, height, image);
        else hal::display().pushImage(x, y, width, height, image, uint16_t(transparent));
    },
    [](int x, int y, int width, int height, int radius, uint16_t color, bool filled) {
        if (gameScreen) return;
        if (filled) hal::display().fillRoundRect(x, y, width, height, radius, color);
        else hal::display().drawRoundRect(x, y, width, height, radius, color);
    },
    [](int x, int y, int width, int height) {
        if (!gameScreen) hal::display().setClipRect(x, y, width, height);
    },
    []() { if (!gameScreen) hal::display().clearClipRect(); },
    openUi, []() { if (gameScreen && !deletingScreen) closeRequested = true; },
    [](lv_obj_t* object) {
        if (!gameScreen || deletingScreen || !object || object == gameScreen ||
            !lv_obj_is_valid(object) || lv_obj_get_screen(object) != gameScreen) return false;
        lv_obj_del(object);
        return true;
    },
};

esp_elf_symbol_table_t imports[] = {
    ESP_ELFSYM_EXPORT(memcpy), ESP_ELFSYM_EXPORT(memmove),
    ESP_ELFSYM_EXPORT(memset), ESP_ELFSYM_EXPORT(snprintf),
#define STACKCUBE_LVGL_EXPORT(symbol) ESP_ELFSYM_EXPORT(symbol),
#include <StackCubeLvglExports.h>
#undef STACKCUBE_LVGL_EXPORT
    ESP_ELFSYM_END,
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

    stackcube::abi::LaunchV1 request{&services.v6.v5.v4.v3.v2.v1, {}};
    char* arguments[] = {reinterpret_cast<char*>(&request), nullptr};
    result = esp_elf_request(&elf, 0, 1, arguments);
    if (result || !request.game.start || !request.game.tick) {
        return fail("Incompatible StackCube game ABI", result);
    }

    game = request.game;
    gameFolder = launch.folder;
    Serial.printf("Launching %s with game ABI %u\n", path, unsigned(request.services->version));
    stackcube::hal::beginGameAudio(launch);
    game.start();
    endDraw();
    if (closeRequested) closeUi();
    return true;
}

void GameRuntime::poll(const stackcube::Input& input) {
    if (!active()) return;
    abi::InputV3 frame{{snapshot(input), input.repeated}, {}};
    memcpy(frame.keys, input.keys, sizeof(frame.keys));
    const bool keepPlaying = game.tick(&frame.v2.v1);
    endDraw();
    if (!keepPlaying) {
        stop();
        return;
    }
    if (!closeRequested && gameScreen) {
        pointerPosition.x = std::clamp<int>(pointerPosition.x + input.mouseX, 0, hal::Width - 1);
        pointerPosition.y = std::clamp<int>(pointerPosition.y + input.mouseY, 0, hal::Height - 1);
        pointerDown = input.mouseConnected && (input.mouseButtons & 1);
        if (pointerCursor) {
            if (input.mouseConnected) lv_obj_clear_flag(pointerCursor, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(pointerCursor, LV_OBJ_FLAG_HIDDEN);
        }
        tickLvglDisplay();
    }
    if (closeRequested) closeUi();
}

void GameRuntime::stop() {
    game = {};
    endDraw();
    closeUi();
    esp_elf_deinit(&elf);
    gameFolder = "";
    stackcube::hal::display().clearClipRect();
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
