#include "UartFormat.h"

namespace uartconsole {
namespace {
constexpr char Digits[] = "0123456789ABCDEF";

int hexValue(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

int shiftForByte(size_t byte, Format format) {
    return 8 * (format == Format::DecimalLE ? byte : 3 - byte);
}

size_t encodeHex(const char* draft, size_t length, uint8_t* output) {
    size_t count = 0;
    for (size_t i = 0; i < length;) {
        if (draft[i] == ' ') {
            ++i;
            continue;
        }
        if (length - i < 2) return 0;
        const int high = hexValue(draft[i++]);
        const int low = hexValue(draft[i++]);
        if (high < 0 || low < 0) return 0;
        output[count++] = (high << 4) | low;
    }
    return count;
}

size_t encodeDecimal(const char* draft, size_t length, Format format, uint8_t* output) {
    size_t count = 0;
    for (size_t i = 0; i < length;) {
        if (draft[i] == ' ') {
            ++i;
            continue;
        }
        uint32_t value = 0;
        do {
            const char character = draft[i++];
            if (character < '0' || character > '9') return 0;
            const uint32_t digit = character - '0';
            if (value > (UINT32_MAX - digit) / 10) return 0;
            value = value * 10 + digit;
        } while (i < length && draft[i] != ' ');
        for (size_t byte = 0; byte < 4; ++byte) output[count++] = value >> shiftForByte(byte, format);
    }
    return count;
}
}

Format nextFormat(Format format) {
    return format == Format::DecimalBE ? Format::Ascii : static_cast<Format>(static_cast<uint8_t>(format) + 1);
}

const char* formatName(Format format) {
    switch (format) {
        case Format::Ascii: return "ASCII";
        case Format::Hex: return "HEX";
        case Format::DecimalLE: return "DEC LE";
        case Format::DecimalBE: return "DEC BE";
    }
    return "";
}

char hexDigit(uint8_t value) { return Digits[value & 0x0f]; }

uint32_t decodeU32(const uint8_t* bytes, Format format) {
    uint32_t value = 0;
    for (size_t byte = 0; byte < 4; ++byte) value |= uint32_t(bytes[byte]) << shiftForByte(byte, format);
    return value;
}

size_t encode(const char* draft, size_t length, Format format, uint8_t (&output)[SendCapacity]) {
    if (length > DraftCapacity) return 0;
    if (format == Format::Hex) return encodeHex(draft, length, output);
    if (format != Format::Ascii) return encodeDecimal(draft, length, format, output);
    for (size_t i = 0; i < length; ++i) output[i] = draft[i];
    return length;
}

}
