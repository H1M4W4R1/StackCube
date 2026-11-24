#pragma once

#include <stddef.h>
#include <stdint.h>

namespace uartconsole {

enum class Format : uint8_t { Ascii, Hex, DecimalLE, DecimalBE };

constexpr size_t DraftCapacity = 96;
constexpr size_t SendCapacity = ((DraftCapacity + 1) / 2) * 4;

Format nextFormat(Format format);
const char* formatName(Format format);
char hexDigit(uint8_t value);
uint32_t decodeU32(const uint8_t* bytes, Format format);
size_t encode(const char* draft, size_t length, Format format, uint8_t (&output)[SendCapacity]);

}
