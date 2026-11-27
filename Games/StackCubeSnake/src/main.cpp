#include <StackCube.h>

#include <cstdio>

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
    console.screens().raw().fillRect(LeftEdge + point.x * Cell, TopEdge + point.y * Cell,
                              Cell - 1, Cell - 1, color);
}

bool occupied(Point point, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if (body[i] == point) return true;
    }
    return false;
}

void placeFood() {
    unsigned skip = console.random() % (Columns * Rows - length);
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
    auto& screen = console.screens().raw();
    screen.fillRect(380, 10, 192, 24, Black);
    screen.setTextColor(White);
    screen.setTextSize(2);
    char text[24];
    snprintf(text, sizeof(text), "Score %u", score);
    screen.drawString(text, 380, 13);
}

void reset() {
    auto& screen = console.screens().raw();
    screen.startWrite();
    screen.fillScreen(Black);
    screen.fillRect(LeftEdge, TopEdge, Columns * Cell, Rows * Cell, Board);
    screen.setTextColor(White);
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
    lastStep = console.millis();
}

void endGame() {
    ended = true;
    if (!won) console.sound().playEffect("collision.wav");
    auto& screen = console.screens().raw();
    screen.fillRect(172, 138, 296, 94, Black);
    screen.setTextAlign(TextAlign::Center);
    screen.drawString(won ? "Board complete!" : "Game over", 320, 156);
    screen.drawString("Enter / A to restart", 320, 187);
    screen.setTextAlign(TextAlign::Left);
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
    auto& screen = console.screens().raw();
    screen.startWrite();
    if (!growing) drawCell(body[length - 1], Board);
    drawCell(body[0], Body);
    if (growing) ++length;
    for (unsigned i = length - 1; i > 0; --i) body[i] = body[i - 1];
    body[0] = next;
    drawCell(body[0], Head);
    if (growing) {
        ++score;
        console.sound().playEffect("eat.wav");
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
    if (ready) {
        console.sound().preloadEffect("eat.wav");
        console.sound().preloadEffect("collision.wav");
        reset();
        console.sound().playMusic("background.wav");
    }
}

void loop() {
    const auto& input = console.input().snapshot();
    if (input.pressed & (Back | Menu)) {
        console.returnToLauncher();
        return;
    }
    if (ready) {
        if (ended) {
            if (input.pressed & Accept) reset();
        } else {
            if (!direction.y && input.pressed & Up) intended = {0, -1};
            if (!direction.y && input.pressed & Down) intended = {0, 1};
            if (!direction.x && input.pressed & Left) intended = {-1, 0};
            if (!direction.x && input.pressed & Right) intended = {1, 0};
            const uint32_t now = console.millis();
            if (now - lastStep >= 135) {
                lastStep = now;
                step();
            }
        }
    }
    console.sleep(4);
}
