#pragma once

#include <Arduino.h>
#include <vector>

struct GameEntry {
    static constexpr size_t MaxCoverSize = 256 * 1024;

    String folder;
    String title;
    String binary;
    String cover;
    String preparedCover;
    uint16_t coverWidth = 0, coverHeight = 0;
};

class Catalog {
public:
    void scan();
    void clear() { games.clear(); }
    size_t size() const { return games.size(); }
    const GameEntry& operator[](size_t index) const { return games[index]; }

private:
    std::vector<GameEntry> games;
};
