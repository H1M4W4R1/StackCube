#include "Blocks.h"

#include <algorithm>
#include <numeric>

namespace blocks {
namespace {
// Each group encodes a row. The top row and its leftmost cell use the low bits.
constexpr Shape Shapes[PieceTypes] = {
    {0b0011'0011, 2, 2},  // O
    {0b1111,      4, 1},  // I
    {0b0011'0110, 3, 2},  // S
    {0b0110'0011, 3, 2},  // Z
    {0b0111'0100, 3, 2},  // L
    {0b0111'0001, 3, 2},  // J
    {0b0010'0111, 3, 2},  // T
};

Piece makePiece(uint8_t type) {
    const auto shape = Shapes[type - 1];
    return {shape, type, (Columns - shape.width) / 2, 0};
}
}

void Game::reset(uint32_t seed) {
    board = {};
    points = clearedRows = clearChain = 0;
    random.seed(seed);
    bagIndex = bag.size();
    nextType = drawType();
    spawn();
}

uint8_t Game::drawType() {
    if (bagIndex == bag.size()) {
        std::iota(bag.begin(), bag.end(), 1);
        std::shuffle(bag.begin(), bag.end(), random);
        bagIndex = 0;
    }
    return bag[bagIndex++];
}

Piece Game::next() const { return makePiece(nextType); }

void Game::spawn() {
    current = next();
    nextType = drawType();
    ended = !fits(current);
}

bool Game::fits(const Piece& piece) const {
    const auto& shape = piece.shape;
    if (piece.x < 0 || piece.x + shape.width > Columns ||
        piece.y < 0 || piece.y + shape.height > Rows) return false;

    for (int y = 0; y < shape.height; ++y) {
        for (int x = 0; x < shape.width; ++x) {
            if (shape.contains(x, y) && board[piece.y + y][piece.x + x]) return false;
        }
    }
    return true;
}

bool Game::tryMove(int dx, int dy) {
    if (ended) return false;
    Piece candidate = current;
    candidate.x += dx;
    candidate.y += dy;
    if (!fits(candidate)) return false;
    current = candidate;
    return true;
}

bool Game::move(int direction) { return tryMove(direction, 0); }

bool Game::stepDown() { return tryMove(0, 1); }

bool Game::rotate() {
    if (ended) return false;
    const auto& shape = current.shape;
    Shape rotated{0, shape.height, shape.width};
    for (int y = 0; y < shape.height; ++y) {
        for (int x = 0; x < shape.width; ++x) {
            if (shape.contains(x, y)) rotated.cells |= Shape::bit(shape.height - 1 - y, x);
        }
    }
    if (rotated.cells == shape.cells && rotated.width == shape.width) return false;

    constexpr int Kicks[][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0},
                                {0, -1}, {-1, -1}, {1, -1}, {0, -2}, {-1, -2}, {1, -2}};
    for (const auto& kick : Kicks) {
        const Piece candidate{rotated, current.type, current.x + kick[0], current.y + kick[1]};
        if (!fits(candidate)) continue;
        current = candidate;
        return true;
    }
    // The straight piece can need a third cell of clearance at the right wall or floor.
    if (rotated.width < Shape::MaxSide && rotated.height < Shape::MaxSide) return false;
    const Piece candidate{rotated, current.type, std::min(current.x, Columns - rotated.width),
                          std::min(current.y, Rows - rotated.height)};
    if (!fits(candidate)) return false;
    current = candidate;
    return true;
}

int Game::landingY() const {
    Piece candidate = current;
    while (fits(candidate)) ++candidate.y;
    return candidate.y - 1;
}

bool Game::grounded() const {
    Piece below = current;
    ++below.y;
    return !ended && !fits(below);
}

unsigned Game::clearRows() {
    int destination = Rows - 1;
    for (int source = Rows - 1; source >= 0; --source) {
        const auto& row = board[source];
        if (std::all_of(row.begin(), row.end(), [](uint8_t cell) { return cell != 0; })) continue;
        board[destination--] = row;
    }

    const unsigned removed = destination + 1;
    while (destination >= 0) board[destination--].fill(0);
    return removed;
}

void Game::lock() {
    if (!grounded()) return;
    const auto& shape = current.shape;
    for (int y = 0; y < shape.height; ++y) {
        for (int x = 0; x < shape.width; ++x) {
            if (shape.contains(x, y)) board[current.y + y][current.x + x] = current.type;
        }
    }

    const unsigned removed = clearRows();
    clearChain = removed ? clearChain + 1 : 0;
    clearedRows += removed;
    points += 100 * removed * removed * clearChain;
    spawn();
}

void Game::hardDrop() {
    if (ended) return;
    current.y = landingY();
    lock();
}

}
