#pragma once

#include <stddef.h>
#include <string.h>

namespace timezones {

struct Zone {
    const char* id;
    const char* rule;
};

struct Region {
    const char* name;
    size_t first;
    size_t count;
};

extern const Zone zones[];
extern const Region regions[];
extern const size_t zoneCount;
extern const size_t regionCount;

int find(const char* id);

}
