#include <StackCube.h>

#include <algorithm>

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
    screen.fillScreen(Black);
    screen.setTextColor(White);
    screen.setTextSize(2);
    screen.drawString("Basic Example", 22, 18);
    screen.drawString("Move with arrows, stick or mouse. Esc / B returns.", 22, 322);
    screen.fillCircle(x, y, 9, Yellow);
}

void loop() {
    const auto& input = console.poll();
    if (input.pressed & (Back | Menu)) {
        console.returnToLauncher();
        return;
    }
    if (ready) {
        const int dx = input.mouseX + ((input.held & Right) ? 3 : 0) - ((input.held & Left) ? 3 : 0);
        const int dy = input.mouseY + ((input.held & Down) ? 3 : 0) - ((input.held & Up) ? 3 : 0);
        const bool accepting = input.held & Accept;
        if (dx || dy || accepting != wasAccepting) {
            auto& screen = console.screen();
            screen.fillCircle(x, y, 9, Black);
            x = std::clamp(x + dx, 10, screen.width() - 11);
            y = std::clamp(y + dy, 57, 300);
            screen.fillCircle(x, y, 9, accepting ? Green : Yellow);
            wasAccepting = accepting;
        }
    }
    console.sleep(12);
}
