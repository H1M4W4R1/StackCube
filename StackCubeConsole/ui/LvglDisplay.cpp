#include "LvglDisplay.h"

#include <SD.h>
#include <StackCubeHal.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <new>

namespace {
void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
    auto& screen = stackcube::hal::display();
    screen.pushImage(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area),
                     reinterpret_cast<lgfx::rgb565_t*>(pixels));
    lv_disp_flush_ready(driver);
}

void* open(lv_fs_drv_t*, const char* path, lv_fs_mode_t mode) {
    if (mode != LV_FS_MODE_RD) return nullptr;
    String absolute = path[0] == '/' ? path : String("/") + path;
    File file = SD.open(absolute);
    if (!file || file.isDirectory()) return nullptr;
    return new (std::nothrow) File(std::move(file));
}

lv_fs_res_t close(lv_fs_drv_t*, void* handle) {
    delete static_cast<File*>(handle);
    return LV_FS_RES_OK;
}

lv_fs_res_t read(lv_fs_drv_t*, void* handle, void* buffer, uint32_t count, uint32_t* received) {
    const size_t result = static_cast<File*>(handle)->read(static_cast<uint8_t*>(buffer), count);
    if (result > count) return LV_FS_RES_FS_ERR;
    *received = result;
    return LV_FS_RES_OK;
}

lv_fs_res_t seek(lv_fs_drv_t*, void* handle, uint32_t offset, lv_fs_whence_t whence) {
    auto& file = *static_cast<File*>(handle);
    if (whence == LV_FS_SEEK_CUR) offset += file.position();
    if (whence == LV_FS_SEEK_END) offset += file.size();
    return file.seek(offset) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t tell(lv_fs_drv_t*, void* handle, uint32_t* offset) {
    *offset = static_cast<File*>(handle)->position();
    return LV_FS_RES_OK;
}
}

bool beginLvglDisplay() {
    constexpr size_t bufferPixels = stackcube::hal::Width * 20;
    auto* pixels = static_cast<lv_color_t*>(heap_caps_malloc(bufferPixels * sizeof(lv_color_t),
                                                             MALLOC_CAP_DMA));
    if (!pixels) return false;
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
    static lv_fs_drv_t fs;
    lv_fs_drv_init(&fs);
    fs.letter = 'S';
    fs.open_cb = open;
    fs.close_cb = close;
    fs.read_cb = read;
    fs.seek_cb = seek;
    fs.tell_cb = tell;
    lv_fs_drv_register(&fs);
    return true;
}

void tickLvglDisplay() {
    static uint32_t previous = millis();
    const uint32_t now = millis();
    lv_tick_inc(now - previous);
    previous = now;
    lv_timer_handler();
}
