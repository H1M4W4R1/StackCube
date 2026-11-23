#include "LvglDisplay.h"

#include <StackCubeHal.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

namespace {
void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
    auto& screen = stackcube::hal::display();
    const int width = lv_area_get_width(area);
    const int height = lv_area_get_height(area);
    // The HDMI panel accepts big-endian RGB565 over SPI. Swap the buffer in
    // place so M5GFX can transfer the whole rectangle without row conversion.
    for (int i = 0; i < width * height; ++i) pixels[i].full = __builtin_bswap16(pixels[i].full);
    screen.pushImageDMA(area->x1, area->y1, width, height, reinterpret_cast<const lgfx::swap565_t*>(pixels));
    screen.waitDMA();
    lv_disp_flush_ready(driver);
}
}

bool beginLvglDisplay() {
    constexpr size_t bufferPixels = stackcube::hal::Width * 20;
    constexpr size_t bufferBytes = bufferPixels * sizeof(lv_color_t);
    constexpr auto dma = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    auto* pixels = static_cast<lv_color_t*>(heap_caps_malloc(bufferBytes, dma));
    if (!pixels) {
        Serial.printf("LVGL DMA allocation failed: %zu requested, %zu free, %zu largest block (bytes)\n",
                      bufferBytes, heap_caps_get_free_size(dma), heap_caps_get_largest_free_block(dma));
        return false;
    }
    stackcube::hal::display().setColorDepth(16);
    lv_init();
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, nullptr, bufferPixels);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = stackcube::hal::Width;
    display.ver_res = stackcube::hal::Height;
    display.draw_buf = &buffer;
    display.flush_cb = flush;
    if (!lv_disp_drv_register(&display)) {
        free(pixels);
        return false;
    }
    return true;
}

void tickLvglDisplay() {
    static uint32_t previous = millis();
    const uint32_t now = millis();
    lv_tick_inc(now - previous);
    previous = now;
    lv_timer_handler();
}
