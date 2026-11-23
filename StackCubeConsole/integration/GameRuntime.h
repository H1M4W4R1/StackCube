#pragma once

#include <Arduino.h>
#include <StackCubeAbi.h>
#include <StackCubeGameLaunch.h>
#include <StackCubeInput.h>
#include <esp_elf.h>

class GameRuntime {
public:
    bool start(const char* path, const stackcube::GameLaunch& launch);
    bool active() const { return game.tick != nullptr; }
    void poll(const stackcube::Input& input);
    void stop();
    const char* error() const { return failure; }

private:
    bool fail(const char* message, int result = 0);

    esp_elf_t elf{};
    stackcube::abi::GameV1 game{};
    const char* failure = "";
};
