#pragma once

#include <StackCube.h>

#include "UartFormat.h"

namespace uartconsole {

class ReceiveLog {
public:
    static constexpr int Top = 72;
    static constexpr int Bottom = 280;
    static constexpr uint16_t Background = 0x1082;

    void append(const uint8_t* data, size_t count);
    void clear();
    void draw(stackcube::Screen& screen, Format format);

private:
    static constexpr size_t Capacity = 4096;
    static constexpr int Left = 16;
    static constexpr int CharacterWidth = 6;
    static constexpr int Columns = (stackcube::Screen::width() - 2 * Left) / CharacterWidth;
    static constexpr int RowHeight = 16;
    static constexpr int Rows = (Bottom - Top) / RowHeight;

    struct Row {
        char text[Columns];
        uint8_t frames[Columns];
        int length;

        void append(const char* label, int size, bool framed);
        bool matches(const Row& other) const;
        void draw(stackcube::Screen& screen, int y) const;
    };

    uint8_t bytes[Capacity]{};
    size_t first = 0;
    size_t count = 0;
    Row rows[Rows]{};
    Row drawn[Rows]{};
    int newest = 0;
    int visible = 0;

    uint8_t at(size_t offset) const;
    void newRow();
    void add(const char* label, int size, bool framed = false);
    void formatRows(Format format);
};

}
