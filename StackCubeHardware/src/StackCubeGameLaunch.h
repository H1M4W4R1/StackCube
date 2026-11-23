#pragma once

#include <Arduino.h>

namespace stackcube {

enum class AudioOutput { Speaker, Hdmi, Off };

const char* audioOutputName(AudioOutput output);
AudioOutput parseAudioOutput(const char* name);

struct GameLaunch {
    String folder;
    AudioOutput audioOutput = AudioOutput::Speaker;

    bool save() const;
    bool load();
};

}
