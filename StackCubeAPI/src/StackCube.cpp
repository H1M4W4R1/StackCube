#include "StackCube.h"
#include <StackCubeHal.h>

#include <esp_ota_ops.h>

namespace stackcube {
namespace {
bool selectLauncher() {
    const auto* launcher = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                     ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                                     "launcher");
    const auto* boot = esp_ota_get_boot_partition();
    if (!launcher) return false;
    if (boot && boot->address == launcher->address) return true;
    const auto result = esp_ota_set_boot_partition(launcher);
    if (result == ESP_OK) return true;
    Serial.printf("Could not select launcher: %s\n", esp_err_to_name(result));
    return false;
}
}

bool Console::begin() {
    Serial.begin(115200);
    const auto* running = esp_ota_get_running_partition();
    // A reset after SDK initialization should enter the launcher, even after a crash.
    if (running && running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
        !selectLauncher()) {
        Serial.println("StackCube recovery could not be armed");
    }
    return hal::begin();
}

const Input& Console::poll() { return hal::poll(); }
M5GFX& Console::screen() { return hal::display(); }
bool Console::setRumble(uint8_t slot, uint8_t left, uint8_t right) {
    return hal::setRumble(slot, left, right);
}

bool Console::returnToLauncher() {
    if (!selectLauncher()) return false;
    hal::stopRumble();
    ESP.restart();
    return true;
}
}
