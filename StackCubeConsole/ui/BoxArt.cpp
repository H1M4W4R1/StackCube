#include "BoxArt.h"

#include <SD.h>
#include <lgfx/utility/lgfx_pngle.h>
#include <algorithm>
#include <new>

namespace {
constexpr unsigned ArtWidth = 99;
constexpr unsigned ArtHeight = 150;
constexpr size_t ReadChunk = 4096;
constexpr uint8_t CoverMagic[] = {'S', 'C', 'V', '1'};
static_assert(LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0, "Covers use little-endian RGB565");

struct PngCover {
    fs::File& file;
    lv_color_t* pixels;
    unsigned width, height, outWidth, outHeight;
    size_t written = 0;
    bool readFailed = false;

    static uint32_t read(void* context, uint8_t* buffer, uint32_t count) {
        auto& png = *static_cast<PngCover*>(context);
        const auto position = png.file.position();
        const auto size = png.file.size();
        if (position <= size && count <= size - position &&
            (buffer ? png.file.read(buffer, count) == count : png.file.seek(position + count))) return count;
        png.readFailed = true;
        return 0;
    }

    static void draw(void* context, uint32_t x, uint32_t y, uint_fast8_t step, size_t count, const uint8_t* argb) {
        auto& png = *static_cast<PngCover*>(context);
        if (!step || !count || x >= png.width || y >= png.height) return;
        const auto background = lv_color_hex(BoxArt::Background);
        const auto ceilScale = [](unsigned value, unsigned out, unsigned in) {
            return (value * out + in - 1) / in;
        };
        const unsigned top = ceilScale(y, png.outHeight, png.height);
        const unsigned bottom = ceilScale(y + 1, png.outHeight, png.height);
        if (top == bottom) return;

        // Interlaced passes supply every step-th source pixel. Write only the
        // destination pixels whose nearest source sample belongs to this pass.
        const unsigned left = ceilScale(x, png.outWidth, png.width);
        const unsigned end = std::min<size_t>(png.width, x + (count - 1) * step + 1);
        const unsigned right = ceilScale(end, png.outWidth, png.width);
        for (unsigned dx = left; dx < right; ++dx) {
            const unsigned source = dx * png.width / png.outWidth - x;
            if (source % step) continue;
            const auto* pixel = argb + (source / step) * 4;
            const auto color = lv_color_mix(lv_color_make(pixel[1], pixel[2], pixel[3]), background, pixel[0]);
            for (unsigned dy = top; dy < bottom; ++dy) png.pixels[dy * png.outWidth + dx] = color;
            png.written += bottom - top;
        }
    }
};
}

bool BoxArt::contains(int index) const {
    return slots[index % std::size(slots)].index == index;
}

const lv_img_dsc_t* BoxArt::image(int index) const {
    const auto& slot = slots[index % std::size(slots)];
    return slot.index == index && slot.pixels ? &slot.image : nullptr;
}

bool BoxArt::Slot::allocate(unsigned width, unsigned height) {
    pixels.reset(new (std::nothrow) lv_color_t[width * height]);
    if (!pixels) return false;
    image.header.cf = LV_IMG_CF_TRUE_COLOR;
    image.header.w = width;
    image.header.h = height;
    image.data_size = width * height * sizeof(lv_color_t);
    image.data = reinterpret_cast<const uint8_t*>(pixels.get());
    return true;
}

bool BoxArt::openPrepared(const String& path) {
    if (path.isEmpty()) return false;
    source = SD.open(path);
    uint8_t header[8];
    if (!source || source.isDirectory() || source.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, CoverMagic, sizeof(CoverMagic))) return false;

    const unsigned width = header[4] | (unsigned(header[5]) << 8);
    const unsigned height = header[6] | (unsigned(header[7]) << 8);
    if (!width || width > ArtWidth || !height || height > ArtHeight) return false;
    size = width * height * sizeof(lv_color_t);
    return source.size() == sizeof(header) + size && loading.allocate(width, height);
}

bool BoxArt::poll(int index, const GameEntry& game) {
    auto& slot = slots[index % std::size(slots)];
    if (loading.index != index) {
        cancel();
        slot = {};
        loading.index = index;
        openPrepared(game.preparedCover);
    }

    if (loading.pixels) {
        auto* data = reinterpret_cast<uint8_t*>(loading.pixels.get());
        const size_t count = std::min(ReadChunk, size - read);
        if (source.read(data + read, count) == count) {
            read += count;
            if (read < size) return false;
        } else {
            loading.pixels.reset();
        }
    }
    source.close();
    if (!loading.pixels && !game.cover.isEmpty()) decode(game);

    if (!loading.pixels) Serial.printf("Cover could not be loaded for %s\n", game.title.c_str());
    slot = std::move(loading);
    cancel();
    return true;
}

void BoxArt::clear() {
    cancel();
    for (auto& slot : slots) slot = {};
}

void BoxArt::cancel() {
    source.close();
    loading = {};
    size = read = 0;
}

void BoxArt::decode(const GameEntry& game) {
    auto file = SD.open(game.cover);
    if (!file || file.isDirectory() || file.size() < 33 || file.size() > GameEntry::MaxCoverSize) return;
    const unsigned width = game.coverWidth;
    const unsigned height = game.coverHeight;
    uint8_t header[29];
    if (file.read(header, sizeof(header)) != sizeof(header) ||
        header[16] || header[17] || ((unsigned(header[18]) << 8) | header[19]) != width ||
        header[20] || header[21] || ((unsigned(header[22]) << 8) | header[23]) != height ||
        !width || !height || header[28] > 1 || !file.seek(0)) return;

    const unsigned outWidth = std::max(1u, std::min(ArtWidth, width * ArtHeight / height));
    const unsigned outHeight = std::max(1u, std::min(ArtHeight, height * ArtWidth / width));
    std::unique_ptr<pngle_t, decltype(&lgfx_pngle_destroy)> decoder(lgfx_pngle_new(), lgfx_pngle_destroy);
    if (!decoder) return;
    PngCover png{file, nullptr, width, height, outWidth, outHeight};
    if (lgfx_pngle_prepare(decoder.get(), PngCover::read, &png) < 0) return;
    if (!loading.allocate(outWidth, outHeight)) return;
    png.pixels = loading.pixels.get();
    if (lgfx_pngle_decomp(decoder.get(), PngCover::draw) < 0 || png.readFailed || png.written != outWidth * outHeight) {
        loading.pixels.reset();
    }
}
