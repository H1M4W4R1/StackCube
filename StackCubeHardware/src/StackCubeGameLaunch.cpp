#include "StackCubeGameLaunch.h"

#include <Preferences.h>

namespace stackcube {
namespace {
constexpr char Storage[] = "stackcube";
constexpr char LaunchKey[] = "game_launch";
constexpr char GameRoot[] = "/games/";

bool validGameFolder(const String& folder) {
    if (folder.length() < sizeof(GameRoot) || !folder.startsWith(GameRoot)) return false;
    const char* leaf = folder.c_str() + sizeof(GameRoot) - 1;
    return strcmp(leaf, ".") && strcmp(leaf, "..") && !strpbrk(leaf, "/\\:");
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
    if (!validGameFolder(folder)) return false;
    String record(audioOutputName(audioOutput));
    if (!record.concat(':') || !record.concat(folder)) return false;
    Preferences preferences;
    return preferences.begin(Storage) && preferences.putString(LaunchKey, record) == record.length();
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
    return true;
}

}
