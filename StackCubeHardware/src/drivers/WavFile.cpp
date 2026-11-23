#include "WavFile.h"

#include <SD.h>

namespace stackcube::audio {
namespace {
uint16_t read16(const uint8_t* bytes) {
    return uint16_t(bytes[0]) | uint16_t(bytes[1]) << 8;
}

uint32_t read32(const uint8_t* bytes) {
    return uint32_t(read16(bytes)) | uint32_t(read16(bytes + 2)) << 16;
}
}

bool WavFile::open(const char* path) {
    file.close();
    File candidate = SD.open(path);
    uint8_t header[12];
    if (!candidate || candidate.isDirectory() || candidate.size() < sizeof(header) ||
        candidate.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4)) return false;

    const uint32_t riffSize = read32(header + 4);
    if (riffSize < 4 || riffSize > candidate.size() - 8 || riffSize > UINT32_MAX - 8) return false;
    const uint32_t end = riffSize + 8;
    uint16_t channels = 0;
    uint32_t offset = sizeof(header);
    // Bound metadata work for malformed files. Typical PCM files have two to four chunks.
    for (unsigned chunk = 0; chunk < 16 && end - offset >= 8; ++chunk) {
        uint8_t tag[8];
        if (!candidate.seek(offset) || candidate.read(tag, sizeof(tag)) != sizeof(tag)) return false;
        offset += sizeof(tag);
        const uint32_t size = read32(tag + 4);
        if (size > end - offset || (size & 1) > end - offset - size) return false;

        if (!memcmp(tag, "fmt ", 4)) {
            uint8_t format[16];
            if (size < sizeof(format) || candidate.read(format, sizeof(format)) != sizeof(format)) return false;
            channels = read16(format + 2);
            if (read16(format) != 1 || (channels != 1 && channels != 2) ||
                read32(format + 4) != SampleRate || read32(format + 8) != SampleRate * channels * 2 ||
                read16(format + 12) != channels * 2 || read16(format + 14) != 16) return false;
        } else if (!memcmp(tag, "data", 4)) {
            if (!channels || !size || size % (channels * 2)) return false;
            dataStart = offset;
            length = unread = size;
            isStereo = channels == 2;
            file = std::move(candidate);
            return true;
        }
        offset += size + (size & 1);
    }
    return false;
}

bool WavFile::rewind() {
    if (!file.seek(dataStart)) return false;
    unread = length;
    return true;
}

bool WavFile::read(void* buffer, size_t bytes) {
    if (bytes > unread || file.read(static_cast<uint8_t*>(buffer), bytes) != bytes) return false;
    unread -= bytes;
    return true;
}

}
