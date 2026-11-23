#pragma once

#include <FS.h>

namespace stackcube::audio {

class WavFile {
public:
    static constexpr uint32_t SampleRate = 48000;

    bool open(const char* path);
    void close() { file.close(); }
    explicit operator bool() const { return bool(file); }
    size_t size() const { return length; }
    size_t remaining() const { return unread; }
    bool stereo() const { return isStereo; }
    bool rewind();
    bool read(void* buffer, size_t bytes);

private:
    File file;
    uint32_t dataStart = 0, length = 0, unread = 0;
    bool isStereo = false;
};

}
