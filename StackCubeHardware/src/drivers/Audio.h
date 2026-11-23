#pragma once

#include "../StackCubeGameLaunch.h"

namespace stackcube::audio {

bool begin(const GameLaunch& launch);
void end();
void poll();
bool preloadSfx(const char* name);
bool playSfx(const char* name);
bool playBgTrack(const char* name);
void stopBgTrack();

}
