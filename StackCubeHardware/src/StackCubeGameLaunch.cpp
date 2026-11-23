#include "StackCubeGameLaunch.h"

#include <Preferences.h>

namespace stackcube {
namespace {
constexpr char Storage[] = "stackcube";
constexpr char LaunchKey[] = "game_launch";
constexpr char VolumeKey[] = "audio_volume";
constexpr char MusicVolumeKey[] = "music_volume";
constexpr char EffectsVolumeKey[] = "effects_volume";
constexpr char GameRoot[] = "/games/";

bool validGameFolder(const String& folder) {
    if (folder.length() < sizeof(GameRoot) || !folder.startsWith(GameRoot)) return false;
    const char* leaf = folder.c_str() + sizeof(GameRoot) - 1;
    return strcmp(leaf, ".") && strcmp(leaf, "..") && !strpbrk(leaf, "/\\:");
}

uint8_t loadVolume(Preferences& preferences, const char* key) {
    const uint8_t volume = preferences.getUChar(key, MaxAudioVolume);
    return volume <= MaxAudioVolume ? volume : MaxAudioVolume;
}
}

const char* audioOutputName(AudioOutput output) {
    switch (output) {
        case AudioOutput::Hdmi: return "hdmi";
        case AudioOutput::Off: return "off";
        default: return "speaker";
    }
}

AudioOutput parseAudioOutput(const char* name) {
    if (!strcmp(name, "hdmi")) return AudioOutput::Hdmi;
    if (!strcmp(name, "off")) return AudioOutput::Off;
    return AudioOutput::Speaker;
}

bool GameLaunch::save() const {
    if (!validGameFolder(folder) || !audioLevels.valid()) return false;
    String record(audioOutputName(audioLevels.audible() ? audioOutput : AudioOutput::Off));
    if (!record.concat(':') || !record.concat(folder)) return false;
    Preferences preferences;
    // Keep the launch record readable by games compiled before volume was added.
    return preferences.begin(Storage) && preferences.putUChar(VolumeKey, audioLevels.master) == sizeof(uint8_t) &&
           preferences.putUChar(MusicVolumeKey, audioLevels.music) == sizeof(uint8_t) &&
           preferences.putUChar(EffectsVolumeKey, audioLevels.effects) == sizeof(uint8_t) &&
           preferences.putString(LaunchKey, record) == record.length();
}

bool GameLaunch::load() {
    Preferences preferences;
    if (!preferences.begin(Storage, true)) return false;
    const String record = preferences.getString(LaunchKey);
    const int separator = record.indexOf(':');
    if (separator < 0) return false;
    const String path = record.substring(separator + 1);
    if (!validGameFolder(path)) return false;
    folder = path;
    audioOutput = parseAudioOutput(record.substring(0, separator).c_str());
    audioLevels = {loadVolume(preferences, VolumeKey), loadVolume(preferences, MusicVolumeKey),
                   loadVolume(preferences, EffectsVolumeKey)};
    return true;
}

}
