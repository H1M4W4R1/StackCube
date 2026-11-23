#include <StackCube.h>

using namespace stackcube;

namespace {
Console console;
bool ready;
bool wasAccepting;
int x = 320, y = 180;
}

void setup() {
    ready = console.begin();
    if (!ready) return;
    auto& screen = console.screen();
    screen.fillScreen(TFT_NAVY);
    screen.setTextColor(TFT_WHITE);
    screen.setTextSize(2);
    screen.drawString("StackCube input example", 22, 18);
    screen.drawString("Move with arrows, stick or mouse. Esc / B returns.", 22, 322);
    screen.fillCircle(x, y, 9, TFT_YELLOW);
}

void loop() {
    const auto& input = console.poll();
    if (input.pressed & (Back | Menu)) console.returnToLauncher();
    if (ready) {
        const int dx = input.mouseX + ((input.held & Right) ? 3 : 0) - ((input.held & Left) ? 3 : 0);
        const int dy = input.mouseY + ((input.held & Down) ? 3 : 0) - ((input.held & Up) ? 3 : 0);
        const bool accepting = input.held & Accept;
        if (dx || dy || accepting != wasAccepting) {
            auto& screen = console.screen();
            screen.fillCircle(x, y, 9, TFT_NAVY);
            x = constrain(x + dx, 10, screen.width() - 11);
            y = constrain(y + dy, 57, 300);
            screen.fillCircle(x, y, 9, accepting ? TFT_GREEN : TFT_YELLOW);
            wasAccepting = accepting;
        }
    }
    delay(12);
}
