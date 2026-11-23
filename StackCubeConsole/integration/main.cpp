#include "../drivers/SdCard.h"
#include "../ui/LauncherUi.h"
#include "../ui/LvglDisplay.h"
#include "Bootloader.h"

#include <M5Unified.h>
#include <StackCubeHal.h>

namespace {
SdCard card;
Catalog catalog;
Bootloader bootloader;
NetworkClock network;
LauncherUi ui(catalog);
bool displayReady;

void updateMedia() {
    const bool mounted = card.mounted();
    if (mounted) catalog.scan();
    else catalog.clear();
    const auto settings = mounted ? network.loadOrCreate() : Settings{};
    network.apply(settings);
    if (displayReady) ui.mediaChanged(mounted, settings, card.inserted() && !mounted);
}
}

void setup() {
    stackcube::hal::beginBoard();
    card.poll(millis());
    displayReady = stackcube::hal::beginPeripherals() && beginLvglDisplay();
    network.begin();
    if (displayReady) ui.begin();
    else Serial.println("Launcher UI unavailable");
    updateMedia();
}

void loop() {
    const auto& input = stackcube::hal::poll();
    const uint32_t now = millis();
    if (bootloader.active()) {
        bootloader.poll();
        if (bootloader.active()) {
            static unsigned previous = 100;
            const unsigned progress = bootloader.progress();
            if (progress != previous) ui.loading(progress);
            previous = progress;
        } else {
            ui.launchError(bootloader.error().c_str());
        }
    } else {
        const auto change = card.poll(now);
        if (change != SdCard::Change::None) updateMedia();
    }

    network.poll();
    if (displayReady) {
        ui.handle(input, now);
        tickLvglDisplay();
        const int game = ui.takeGame();
        if (game >= 0 && card.mounted() && size_t(game) < catalog.size()) {
            if (bootloader.start(catalog[game].binary.c_str())) ui.loading(0);
            else ui.launchError(bootloader.error().c_str());
        }
        Settings settings;
        if (ui.takeSettings(settings)) {
            const bool saved = card.mounted() && network.save(settings);
            if (saved) network.apply(settings);
            ui.settingsResult(saved, settings);
        }
        static uint32_t lastStatus = now - 500;
        if (now - lastStatus >= 500) {
            char time[6];
            network.formatTime(time);
            ui.status(network.connected(), time);
            lastStatus = now;
        }
        ui.loadCovers(millis());
    }
    delay(2);
}
