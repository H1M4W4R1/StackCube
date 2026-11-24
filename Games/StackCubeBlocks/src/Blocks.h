#pragma once

#include <array>
#include <random>
#include <stdint.h>

namespace blocks {

constexpr int Columns = 10;
constexpr int Rows = 21;
constexpr int PieceTypes = 7;
using Field = std::array<std::array<uint8_t, Columns>, Rows>;

struct Shape {
    static constexpr int MaxSide = 4;

    uint16_t cells;
    int width, height;

    static constexpr uint16_t bit(int x, int y) {
        return 1u << (MaxSide * y + x);
    }

    constexpr bool contains(int x, int y) const {
        return cells & bit(x, y);
    }
};

struct Piece {
    Shape shape;
    uint8_t type;
    int x, y;
};

class Game {
public:
    void reset(uint32_t seed);
    bool move(int direction);
    bool rotate();
    bool stepDown();
    void lock();
    void hardDrop();

    const Field& field() const { return board; }
    const Piece& active() const { return current; }
    Piece next() const;
    int landingY() const;
    bool grounded() const;
    bool over() const { return ended; }
    unsigned score() const { return points; }
    unsigned rowsCleared() const { return clearedRows; }
    unsigned chain() const { return clearChain; }
    unsigned level() const { return 1 + clearedRows / 6; }

private:
    bool fits(const Piece& piece) const;
    bool tryMove(int dx, int dy);
    unsigned clearRows();
    uint8_t drawType();
    void spawn();

    Field board{};
    Piece current{};
    std::mt19937 random;
    std::array<uint8_t, PieceTypes> bag{};
    unsigned bagIndex = PieceTypes;
    uint8_t nextType = 0;
    unsigned points = 0, clearedRows = 0, clearChain = 0;
    bool ended = true;
};

}
