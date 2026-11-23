#pragma once

#include "../integration/Catalog.h"

#include <FS.h>
#include <lvgl.h>
#include <memory>

class BoxArt {
public:
    static constexpr uint32_t Background = 0x1C2A37;

    bool contains(int index) const;
    const lv_img_dsc_t* image(int index) const;
    bool poll(int index, const GameEntry& game);
    void cancel();
    void clear();

private:
    struct Slot {
        int index = -1;
        lv_img_dsc_t image = {};
        std::unique_ptr<lv_color_t[]> pixels;

        bool allocate(unsigned width, unsigned height);
    };

    bool openPrepared(const String& path);
    void decode(const GameEntry& game);

    // One slot per visible position keeps the cache bounded on boards without PSRAM.
    Slot slots[5];
    Slot loading;
    fs::File source;
    size_t size = 0, read = 0;
};
