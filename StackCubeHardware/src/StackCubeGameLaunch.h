#pragma once

#include <Arduino.h>

namespace stackcube {

enum class AudioOutput { Speaker, Hdmi, Off };
constexpr uint8_t MaxAudioVolume = 100;

struct AudioLevels {
    uint8_t master = MaxAudioVolume;
    uint8_t music = MaxAudioVolume;
    uint8_t effects = MaxAudioVolume;

    bool audible() const { return master && (music || effects); }
    bool valid() const {
        return master <= MaxAudioVolume && music <= MaxAudioVolume && effects <= MaxAudioVolume;
    }
    bool operator==(const AudioLevels& other) const {
        return master == other.master && music == other.music && effects == other.effects;
    }
};

const char* audioOutputName(AudioOutput output);
AudioOutput parseAudioOutput(const char* name);

struct GameLaunch {
    String folder;
    AudioOutput audioOutput = AudioOutput::Speaker;
    AudioLevels audioLevels;

    bool save() const;
    bool load();
};

}
