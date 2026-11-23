#include <StackCube.h>

#include <esp_system.h>

using namespace stackcube;

namespace {
constexpr int Columns = 28;
constexpr int Rows = 16;
constexpr int Cell = 18;
constexpr int LeftEdge = 68;
constexpr int TopEdge = 46;
constexpr uint16_t Board = 0x1185;
constexpr uint16_t Body = 0x5E10;
constexpr uint16_t Head = 0xAF92;
constexpr uint16_t Food = 0xFBA9;

struct Point {
    int x, y;
    bool operator==(Point other) const { return x == other.x && y == other.y; }
};

Console console;
Point body[Columns * Rows];
Point direction, intended, food;
unsigned length, score;
uint32_t lastStep;
bool ready, ended, won;

void drawCell(Point point, uint16_t color) {
    console.screen().fillRect(LeftEdge + point.x * Cell, TopEdge + point.y * Cell,
                              Cell - 1, Cell - 1, color);
}

bool occupied(Point point, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if (body[i] == point) return true;
    }
    return false;
}

void placeFood() {
    unsigned skip = esp_random() % (Columns * Rows - length);
    for (int y = 0; y < Rows; ++y) {
        for (int x = 0; x < Columns; ++x) {
            if (occupied({x, y}, length)) continue;
            if (skip-- == 0) {
                food = {x, y};
                drawCell(food, Food);
                return;
            }
        }
    }
}

void showScore() {
    auto& screen = console.screen();
    screen.fillRect(380, 10, 192, 24, TFT_BLACK);
    screen.setTextColor(TFT_WHITE);
    screen.setTextSize(2);
    char text[24];
    snprintf(text, sizeof(text), "Score %u", score);
    screen.drawString(text, 380, 13);
}

void reset() {
    auto& screen = console.screen();
    screen.startWrite();
    screen.fillScreen(TFT_BLACK);
    screen.fillRect(LeftEdge, TopEdge, Columns * Cell, Rows * Cell, Board);
    screen.setTextColor(TFT_WHITE);
    screen.setTextSize(2);
    screen.drawString("Snake", LeftEdge, 13);
    length = 3;
    score = 0;
    direction = intended = {1, 0};
    ended = won = false;
    for (unsigned i = 0; i < length; ++i) {
        body[i] = {Columns / 2 - int(i), Rows / 2};
        drawCell(body[i], i ? Body : Head);
    }
    placeFood();
    showScore();
    screen.endWrite();
    lastStep = millis();
}

void endGame() {
    ended = true;
    auto& screen = console.screen();
    screen.fillRect(172, 138, 296, 94, TFT_BLACK);
    screen.setTextDatum(lgfx::textdatum_t::top_center);
    screen.drawString(won ? "Board complete!" : "Game over", 320, 156);
    screen.drawString("Enter / A to restart", 320, 187);
    screen.setTextDatum(lgfx::textdatum_t::top_left);
}

void step() {
    direction = intended;
    const Point next = {body[0].x + direction.x, body[0].y + direction.y};
    const bool growing = next == food;
    if (next.x < 0 || next.x >= Columns || next.y < 0 || next.y >= Rows ||
        occupied(next, growing ? length : length - 1)) {
        endGame();
        return;
    }
    auto& screen = console.screen();
    screen.startWrite();
    if (!growing) drawCell(body[length - 1], Board);
    drawCell(body[0], Body);
    if (growing) ++length;
    for (unsigned i = length - 1; i > 0; --i) body[i] = body[i - 1];
    body[0] = next;
    drawCell(body[0], Head);
    if (growing) {
        ++score;
        showScore();
        won = length == Columns * Rows;
        if (won) endGame();
        else placeFood();
    }
    screen.endWrite();
}
}

void setup() {
    ready = console.begin();
    if (ready) reset();
}

void loop() {
    const auto& input = console.poll();
    if (input.pressed & (Back | Menu)) console.returnToLauncher();
    if (ready) {
        if (ended) {
            if (input.pressed & Accept) reset();
        } else {
            if (!direction.y && input.pressed & Up) intended = {0, -1};
            if (!direction.y && input.pressed & Down) intended = {0, 1};
            if (!direction.x && input.pressed & Left) intended = {-1, 0};
            if (!direction.x && input.pressed & Right) intended = {1, 0};
            const uint32_t now = millis();
            if (now - lastStep >= 135) {
                lastStep = now;
                step();
            }
        }
    }
    delay(4);
}
