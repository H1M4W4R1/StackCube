#include <StackCube.h>
#include <lvgl.h>

#include <algorithm>
#include <cstdio>

using namespace stackcube;

namespace {
Console console;
bool ready;
bool wasAccepting;
bool toggleScreen;
bool uiActive;
bool redrawRaw;
unsigned clicks;
int x = 320, y = 180;

void drawRaw() {
    auto& screen = console.screens().raw();
    screen.fillScreen(Black);
    screen.setTextColor(White);
    screen.setTextSize(2);
    screen.drawString("Basic Example", 22, 18);
    screen.drawImage("box.png", 558, 8, 64, 38);
    screen.drawString("Move with arrows, stick or mouse. Tab: LVGL. Esc / B: exit.", 22, 322);
    screen.fillCircle(x, y, 9, Yellow);
}

bool openUi() {
    auto* root = console.screens().ui().open();
    if (!root) return false;
    auto* heading = lv_label_create(root);
    uint8_t imageHeader[8];
    const bool readable = console.data().read("box.png", imageHeader, sizeof(imageHeader)) == sizeof(imageHeader);
    char message[96];
    snprintf(message, sizeof(message), "LVGL example\nBox art: %ld bytes\nTab returns to M5GFX",
             readable ? long(console.data().size("box.png")) : -1L);
    lv_label_set_text(heading, message);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 50);
    auto* button = lv_btn_create(root);
    lv_obj_center(button);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, "Click me");
    clicks = 0;
    lv_obj_add_event_cb(button, [](lv_event_t* event) {
        char count[32];
        snprintf(count, sizeof(count), "Clicks: %u", ++clicks);
        lv_label_set_text(static_cast<lv_obj_t*>(lv_event_get_user_data(event)), count);
    }, LV_EVENT_CLICKED, label);
    return true;
}
}

void setup() {
    ready = console.begin();
    if (!ready) return;
    console.input().bindKey(0x2b, Gesture::Down, [](const InputEvent&, void*) {
        toggleScreen = true;
    });
    drawRaw();
}

void loop() {
    const auto& input = console.input().snapshot();
    if (input.pressed & (Back | Menu)) {
        console.returnToLauncher();
        return;
    }
    if (ready) {
        if (redrawRaw) {
            redrawRaw = false;
            drawRaw();
        }
        if (toggleScreen) {
            toggleScreen = false;
            if (!uiActive) uiActive = openUi();
            else {
                console.screens().ui().close();
                uiActive = false;
                redrawRaw = true;
            }
            return;
        }
        if (uiActive) return;
        const int dx = input.mouseX + ((input.held & Right) ? 3 : 0) - ((input.held & Left) ? 3 : 0);
        const int dy = input.mouseY + ((input.held & Down) ? 3 : 0) - ((input.held & Up) ? 3 : 0);
        const bool accepting = input.held & Accept;
        if (dx || dy || accepting != wasAccepting) {
            auto& screen = console.screens().raw();
            screen.fillCircle(x, y, 9, Black);
            x = std::clamp(x + dx, 10, screen.width() - 11);
            y = std::clamp(y + dy, 57, 300);
            screen.fillCircle(x, y, 9, accepting ? Green : Yellow);
            wasAccepting = accepting;
        }
    }
    console.sleep(12);
}
