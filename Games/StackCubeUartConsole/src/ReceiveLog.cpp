#include "ReceiveLog.h"

#include <cstdio>

namespace uartconsole {
namespace {
constexpr uint16_t Text = 0xe75c;
constexpr uint16_t FrameText = 0xf6d0;
constexpr uint16_t FrameBorder = 0x9369;
constexpr char ControlNames[][4] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
};
}

void ReceiveLog::append(const uint8_t* data, size_t incoming) {
    for (size_t i = 0; i < incoming; ++i) {
        if (count == Capacity) {
            // Drop a whole word so both decimal views keep their original alignment.
            first = (first + 4) % Capacity;
            count -= 4;
        }
        bytes[(first + count) % Capacity] = data[i];
        ++count;
    }
}

void ReceiveLog::clear() {
    first = 0;
    count = 0;
}

uint8_t ReceiveLog::at(size_t offset) const { return bytes[(first + offset) % Capacity]; }

void ReceiveLog::Row::append(const char* label, int size, bool framed) {
    if (framed) {
        frames[length] = size;
        text[length++] = ' ';
    }
    for (int i = 0; i < size; ++i) {
        frames[length] = 0;
        text[length++] = label[i];
    }
    if (framed) {
        frames[length] = 0;
        text[length++] = ' ';
    }
}

bool ReceiveLog::Row::matches(const Row& other) const {
    if (length != other.length) return false;
    for (int i = 0; i < length; ++i) {
        if (text[i] != other.text[i] || frames[i] != other.frames[i]) return false;
    }
    return true;
}

void ReceiveLog::Row::draw(stackcube::Screen& screen, int y) const {
    screen.fillRect(Left, y, Columns * CharacterWidth, RowHeight, Background);
    char label[Columns + 1];
    for (int i = 0; i < length;) {
        const int x = Left + i * CharacterWidth;
        const int frameSize = frames[i];
        int size = 0;
        if (frameSize) {
            for (int j = 0; j < frameSize; ++j) label[size++] = text[i + 1 + j];
            i += frameSize + 2;
            screen.drawRect(x + 1, y + 1, (frameSize + 2) * CharacterWidth - 2, RowHeight - 2, FrameBorder);
            screen.setTextColor(FrameText);
        } else {
            do {
                label[size++] = text[i++];
            } while (i < length && !frames[i]);
            screen.setTextColor(Text);
        }
        label[size] = '\0';
        screen.drawString(label, x + (frameSize ? CharacterWidth : 0), y + 4);
    }
}

void ReceiveLog::newRow() {
    newest = (newest + 1) % Rows;
    rows[newest].length = 0;
    if (visible < Rows) ++visible;
}

void ReceiveLog::add(const char* label, int size, bool framed) {
    if (rows[newest].length + size + (framed ? 2 : 0) > Columns) newRow();
    rows[newest].append(label, size, framed);
}

void ReceiveLog::formatRows(Format format) {
    const bool decimal = format == Format::DecimalLE || format == Format::DecimalBE;
    for (size_t i = 0; i < count;) {
        const uint8_t byte = at(i++);
        char label[12];
        if (format == Format::Ascii) {
            if (byte == '\n') {
                newRow();
            } else if (byte >= ' ' && byte <= '~') {
                const char character = byte;
                add(&character, 1);
            } else if (byte < ' ') {
                const char* name = ControlNames[byte];
                add(name, name[2] ? 3 : 2, true);
            } else if (byte == 0x7f) {
                add("DEL", 3, true);
            } else {
                const char hex[] = {'0', 'x', hexDigit(byte >> 4), hexDigit(byte)};
                add(hex, sizeof(hex), true);
            }
        } else if (decimal && count - i >= 3) {
            const uint8_t word[] = {byte, at(i), at(i + 1), at(i + 2)};
            const int size = std::snprintf(label, sizeof(label), "%lu ", static_cast<unsigned long>(decodeU32(word, format)));
            add(label, size);
            i += 3;
        } else {
            const char hex[] = {hexDigit(byte >> 4), hexDigit(byte), ' '};
            add(hex, decimal ? 2 : 3, decimal);
        }
    }
}

void ReceiveLog::draw(stackcube::Screen& screen, Format format) {
    newest = Rows - 1;
    visible = 0;
    newRow();
    formatRows(format);

    const Row empty{};
    const int oldest = (newest + Rows + 1 - visible) % Rows;
    screen.setTextSize(1);
    for (int i = 0; i < Rows; ++i) {
        const Row& row = i < visible ? rows[(oldest + i) % Rows] : empty;
        if (row.matches(drawn[i])) continue;
        row.draw(screen, Top + i * RowHeight);
        drawn[i] = row;
    }
}

}
