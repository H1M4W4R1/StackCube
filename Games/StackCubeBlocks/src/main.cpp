#include <StackCube.h>

#include "Blocks.h"

#include <algorithm>
#include <cstdio>

using namespace stackcube;

namespace {
constexpr int Cell = 15;
constexpr int BoardX = (640 - blocks::Columns * Cell) / 2;
constexpr int BoardY = (360 - blocks::Rows * Cell) / 2;
constexpr int BoardWidth = blocks::Columns * Cell;
constexpr int BoardHeight = blocks::Rows * Cell;
constexpr int StatsX = 29;
constexpr int HelpX = 449;
constexpr uint8_t Ghost = blocks::PieceTypes + 1;
constexpr uint16_t Panel = 0x18E3;
constexpr uint16_t Grid = 0x2104;
constexpr uint16_t Border = 0x73AE;
constexpr uint16_t Heading = 0xBDF7;
constexpr uint16_t Muted = 0xAD55;
constexpr uint16_t Colors[] = {
    Black, 0xFFD0, 0x2DFA, 0x76C8, 0xFB1B, 0xFC85, 0x1E9A, 0xB3DF, Border,
};
static_assert(sizeof(Colors) / sizeof(*Colors) == Ghost + 1);

constexpr uint32_t FirstRepeat = 165;
constexpr uint32_t MoveRepeat = 60;
constexpr uint32_t SoftDropInterval = 45;
constexpr uint32_t LockDelay = 420;

Console console;
blocks::Game game;
blocks::Field drawn;
uint32_t lastFall, lastHorizontal, horizontalInterval, groundedAt;
int horizontalDirection;
bool ready, resting;

void label(const char* text, int x, int y, uint16_t color, int size = 1) {
    auto& screen = console.screen();
    screen.setTextSize(size);
    screen.setTextColor(color);
    screen.drawString(text, x, y);
}

void drawLayout() {
    auto& screen = console.screen();
    screen.fillScreen(Black);
    screen.fillRect(BoardX - 2, BoardY - 2, BoardWidth + 4, BoardHeight + 4, Border);
    screen.fillRect(BoardX, BoardY, BoardWidth, BoardHeight, Grid);

    label("STACKCUBE", StatsX, 23, Heading, 2);
    label("BLOCKS", StatsX, 44, White, 3);
    screen.fillRect(StatsX, 85, 162, 198, Panel);
    label("SCORE", StatsX + 12, 96, Heading);
    label("ROWS", StatsX + 12, 141, Heading);
    label("LEVEL", StatsX + 12, 186, Heading);
    label("CHAIN", StatsX + 12, 231, Heading);
    label("Clear rows on consecutive", StatsX, 299, Muted);
    label("drops to build a chain.", StatsX, 313, Muted);

    screen.fillRect(HelpX, 22, 162, 106, Panel);
    label("NEXT", HelpX + 12, 32, Heading);
    label("MOVE", HelpX, 150, Heading);
    label("Left / Right", HelpX, 164, White);
    label("ROTATE", HelpX, 187, Heading);
    label("Up", HelpX, 201, White);
    label("SOFT DROP", HelpX, 224, Heading);
    label("Hold Down", HelpX, 238, White);
    label("HARD DROP", HelpX, 261, Heading);
    label("Space / Enter / A", HelpX, 275, White);
    label("Esc / B to exit", HelpX, 313, Muted);
}

void drawValue(int y, unsigned value, const char* format = "%u") {
    auto& screen = console.screen();
    screen.fillRect(StatsX + 12, y, 138, 24, Panel);
    char text[16];
    std::snprintf(text, sizeof(text), format, value);
    label(text, StatsX + 12, y + 3, White, 2);
}

void drawStatus() {
    drawValue(109, game.score());
    drawValue(154, game.rowsCleared());
    drawValue(199, game.level());
    drawValue(244, game.chain(), "x%u");

    auto& screen = console.screen();
    screen.fillRect(HelpX + 12, 46, 138, 76, Panel);
    const auto preview = game.next();
    constexpr int PreviewCell = 20;
    const int left = HelpX + (162 - preview.shape.width * PreviewCell) / 2;
    const int top = 46 + (76 - preview.shape.height * PreviewCell) / 2;
    for (int y = 0; y < preview.shape.height; ++y) {
        for (int x = 0; x < preview.shape.width; ++x) {
            if (!preview.shape.contains(x, y)) continue;
            screen.fillRect(left + x * PreviewCell, top + y * PreviewCell,
                            PreviewCell - 2, PreviewCell - 2, Colors[preview.type]);
        }
    }
}

void stamp(blocks::Field& frame, const blocks::Piece& piece, int top, uint8_t color) {
    for (int y = 0; y < piece.shape.height; ++y) {
        for (int x = 0; x < piece.shape.width; ++x) {
            if (piece.shape.contains(x, y)) frame[top + y][piece.x + x] = color;
        }
    }
}

void drawBoard() {
    auto frame = game.field();
    if (!game.over()) {
        const auto& piece = game.active();
        stamp(frame, piece, game.landingY(), Ghost);
        stamp(frame, piece, piece.y, piece.type);
    }

    auto& screen = console.screen();
    for (int y = 0; y < blocks::Rows; ++y) {
        for (int x = 0; x < blocks::Columns; ++x) {
            const uint8_t cell = frame[y][x];
            if (cell == drawn[y][x]) continue;
            drawn[y][x] = cell;
            const int left = BoardX + x * Cell + 1;
            const int top = BoardY + y * Cell + 1;
            screen.fillRect(left, top, Cell - 2, Cell - 2, Colors[cell == Ghost ? 0 : cell]);
            if (cell == Ghost) screen.drawRect(left, top, Cell - 2, Cell - 2, Colors[Ghost]);
        }
    }
}

void drawFrame(bool updateStatus = false) {
    auto& screen = console.screen();
    screen.startWrite();
    drawBoard();
    if (updateStatus) drawStatus();
    if (game.over()) {
        screen.fillRect(BoardX + 2, 147, BoardWidth - 4, 66, Black);
        screen.drawRect(BoardX + 2, 147, BoardWidth - 4, 66, Border);
        screen.setTextAlign(TextAlign::Center);
        label("Game over", 320, 159, White, 2);
        label("Enter / A to retry", 320, 188, Heading);
        screen.setTextAlign(TextAlign::Left);
    }
    screen.endWrite();
}

void startPiece(uint32_t now) {
    lastFall = now;
    resting = false;
}

void restart() {
    game.reset(console.random());
    horizontalDirection = 0;
    for (auto& row : drawn) row.fill(255);
    auto& screen = console.screen();
    screen.startWrite();
    drawLayout();
    drawStatus();
    drawBoard();
    screen.endWrite();
    startPiece(console.millis());
}

bool moveHorizontal(const Input& input, uint32_t now) {
    const int direction = int(bool(input.held & Right)) - int(bool(input.held & Left));
    if (direction != horizontalDirection) {
        horizontalDirection = direction;
        lastHorizontal = now;
        horizontalInterval = FirstRepeat;
        return direction != 0 && game.move(direction);
    }
    if (!direction || now - lastHorizontal < horizontalInterval) return false;
    lastHorizontal = now;
    horizontalInterval = MoveRepeat;
    return game.move(direction);
}

void play(const Input& input, uint32_t now) {
    bool changed = moveHorizontal(input, now);
    if (input.pressed & Up) changed |= game.rotate();

    if (input.pressed & Accept) {
        game.hardDrop();
        startPiece(now);
        drawFrame(true);
        return;
    }

    const uint32_t fallInterval = input.held & Down ? SoftDropInterval :
        760 - 65 * std::min(game.level() - 1, 9u);
    if ((input.pressed & Down) || now - lastFall >= fallInterval) {
        lastFall = now;
        if (game.stepDown()) {
            resting = false;
            changed = true;
        }
    }

    if (!game.grounded()) {
        resting = false;
    } else if (!resting) {
        resting = true;
        groundedAt = now;
    } else if (now - groundedAt >= LockDelay) {
        game.lock();
        startPiece(now);
        drawFrame(true);
        return;
    }
    if (changed) drawFrame();
}
}

void setup() {
    ready = console.begin();
    if (ready) restart();
}

void loop() {
    const auto& input = console.poll();
    if (input.pressed & (Back | Menu)) {
        console.returnToLauncher();
        return;
    }
    if (ready) {
        if (game.over()) {
            if (input.pressed & Accept) restart();
        } else {
            play(input, console.millis());
        }
    }
    console.sleep(4);
}
