#pragma once

#include <FS.h>
#include <esp_ota_ops.h>

class Bootloader {
public:
    bool start(const char* path);
    void poll();
    bool active() const { return file; }
    unsigned progress() const { return total ? written * 100 / total : 0; }
    const String& error() const { return failure; }

private:
    void fail(const char* message, esp_err_t result = ESP_OK);

    File file;
    const esp_partition_t* slot = nullptr;
    esp_ota_handle_t ota = 0;
    size_t total = 0, written = 0;
    String failure;
    uint8_t buffer[8192];
};
