#include "Bootloader.h"

#include <SD.h>
#include <esp_app_format.h>

bool Bootloader::start(const char* path) {
    if (active()) return false;
    failure.clear();
    written = 0;
    slot = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "game");
    const auto* running = esp_ota_get_running_partition();
    if (!slot || !running || running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        fail("Game partition unavailable");
        return false;
    }
    file = SD.open(path);
    total = file ? file.size() : 0;
    esp_image_header_t header;
    if (!file || file.isDirectory() || total < sizeof(header) || total > slot->size ||
        file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32S3 ||
        !file.seek(0)) {
        fail("Invalid or missing ESP32-S3 game binary");
        return false;
    }
    const auto result = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &ota);
    if (result != ESP_OK) {
        fail("Could not open game partition", result);
        return false;
    }
    Serial.printf("Loading %s (%u bytes)\n", path, unsigned(total));
    return true;
}

void Bootloader::poll() {
    if (!active()) return;
    const size_t count = std::min(sizeof(buffer), total - written);
    if (file.read(buffer, count) != count) {
        fail("SD read failed while copying the game");
        return;
    }
    auto result = esp_ota_write(ota, buffer, count);
    if (result != ESP_OK) {
        fail("Could not write game", result);
        return;
    }
    written += count;
    if (written != total) return;
    file.close();
    result = esp_ota_end(ota);
    ota = 0;
    if (result != ESP_OK) {
        fail("Game image did not pass verification", result);
        return;
    }
    result = esp_ota_set_boot_partition(slot);
    if (result != ESP_OK) {
        fail("Could not select game for boot", result);
        return;
    }
    Serial.println("Booting game");
    ESP.restart();
}

void Bootloader::fail(const char* message, esp_err_t result) {
    file.close();
    if (ota) esp_ota_abort(ota);
    ota = 0;
    failure = message;
    Serial.printf("%s (%s)\n", message, esp_err_to_name(result));
}
