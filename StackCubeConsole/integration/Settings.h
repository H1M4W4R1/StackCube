#pragma once

#include <Arduino.h>

struct Settings {
    String ssid;
    String password;
    String timezone = "UTC0";
};

class NetworkClock {
public:
    void begin();
    Settings load() const;
    bool save(const Settings& settings) const;
    void apply(const Settings& settings);
    void poll();
    bool connected() const { return online; }
    void formatTime(char (&text)[6]) const;

private:
    String timezone;
    bool online = false;
};
