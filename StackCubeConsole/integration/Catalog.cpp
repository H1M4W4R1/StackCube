#include "Catalog.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <algorithm>

namespace {
constexpr size_t MaxGames = 512;
constexpr size_t MaxManifestSize = 4096;

String leaf(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

bool validLeaf(const char* name, const char* extension) {
    return name && *name && *name != '.' && strlen(name) <= 120 &&
           !strpbrk(name, "/\\:") && String(name).endsWith(extension);
}

String findManifest(const String& folder) {
    const String hidden = folder + "/.gameinfo";
    if (SD.exists(hidden)) return hidden;
    File directory = SD.open(folder);
    for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
        const String name = leaf(file.name());
        if (!file.isDirectory() && validLeaf(name.c_str(), ".gameinfo")) return folder + "/" + name;
    }
    return {};
}

uint32_t bigEndian(const uint8_t* bytes) {
    return uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 |
           uint32_t(bytes[2]) << 8 | bytes[3];
}

void readCover(GameEntry& entry, const char* filename) {
    if (!validLeaf(filename, ".png") && !validLeaf(filename, ".cover")) return;
    const String stem = entry.folder + "/" + String(filename).substring(0, strrchr(filename, '.') - filename);
    const String prepared = stem + ".cover";
    if (SD.exists(prepared)) entry.preparedCover = prepared;

    const String path = stem + ".png";
    File cover = SD.open(path);
    uint8_t header[24];
    constexpr uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    if (!cover || cover.isDirectory() || cover.size() > GameEntry::MaxCoverSize ||
        cover.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, signature, sizeof(signature))) return;
    const uint32_t width = bigEndian(header + 16);
    const uint32_t height = bigEndian(header + 20);
    if (!width || width > 256 || !height || height > 384) return;
    entry.cover = path;
    entry.coverWidth = width;
    entry.coverHeight = height;
}
}

void Catalog::scan() {
    games.clear();
    File directory = SD.open("/games");
    if (!directory || !directory.isDirectory()) return;
    for (File next = directory.openNextFile(); next && games.size() < MaxGames;
         next = directory.openNextFile()) {
        if (!next.isDirectory()) continue;
        GameEntry entry;
        entry.folder = "/games/" + leaf(next.name());
        const String path = findManifest(entry.folder);
        if (path.isEmpty()) continue;
        File manifest = SD.open(path);
        JsonDocument json;
        if (!manifest || manifest.isDirectory() || manifest.size() > MaxManifestSize ||
            deserializeJson(json, manifest, DeserializationOption::NestingLimit(4))) {
            Serial.printf("Invalid manifest: %s\n", path.c_str());
            continue;
        }
        const char* title = json["title"] | "";
        const char* binary = json["binary"] | "game.bin";
        if (!*title || strlen(title) > 120 || !validLeaf(binary, ".bin")) continue;
        entry.title = title;
        entry.binary = entry.folder + "/" + binary;
        if (!SD.exists(entry.binary)) continue;
        readCover(entry, json["cover"] | "box.png");
        games.push_back(std::move(entry));
    }
    std::sort(games.begin(), games.end(), [](const GameEntry& left, const GameEntry& right) {
        const int compare = strcasecmp(left.title.c_str(), right.title.c_str());
        return compare ? compare < 0 : left.folder.compareTo(right.folder) < 0;
    });
    Serial.printf("Games found: %u\n", unsigned(games.size()));
}
