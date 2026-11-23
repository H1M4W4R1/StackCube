#include "Settings.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <algorithm>
#include <esp_sntp.h>
#include <sys/time.h>

namespace {
constexpr char Directory[] = "/settings";
constexpr char Path[] = "/settings/console.json";
constexpr char Backup[] = "/settings/console.bak";
constexpr char Temporary[] = "/settings/console.tmp";
constexpr time_t ValidClock = 1704067200; // 2024-01-01 UTC

bool valid(const Settings& settings) {
    return settings.ssid.length() <= 32 && settings.password.length() <= 64 &&
           settings.timezone.length() > 0 && settings.timezone.length() <= 95 &&
           std::all_of(settings.timezone.begin(), settings.timezone.end(), [](unsigned char ch) {
               return isalnum(ch) || strchr("+-,.:/<>", ch);
           });
}

bool readSettings(const char* path, Settings& settings) {
    File file = SD.open(path);
    JsonDocument json;
    if (!file || file.isDirectory() || file.size() > 2048 ||
        deserializeJson(json, file, DeserializationOption::NestingLimit(3))) return false;
    settings = {json["ssid"] | "", json["password"] | "", json["timezone"] | "UTC0"};
    return valid(settings);
}
}

void NetworkClock::begin() {
    WiFi.persistent(false);
    setenv("TZ", "UTC0", 1);
    tzset();
    const auto rtc = M5.Rtc.getDateTime();
    if (rtc.date.year < 2024 || rtc.date.year > 2099) return;
    tm utc = {};
    utc.tm_year = rtc.date.year - 1900;
    utc.tm_mon = rtc.date.month - 1;
    utc.tm_mday = rtc.date.date;
    utc.tm_hour = rtc.time.hours;
    utc.tm_min = rtc.time.minutes;
    utc.tm_sec = rtc.time.seconds;
    const timeval value = {mktime(&utc), 0};
    if (value.tv_sec >= ValidClock) settimeofday(&value, nullptr);
}

Settings NetworkClock::load() const {
    Settings settings;
    if (readSettings(Path, settings) || readSettings(Backup, settings)) return settings;
    return {};
}

bool NetworkClock::save(const Settings& settings) const {
    if (!valid(settings) || (!SD.exists(Directory) && !SD.mkdir(Directory))) return false;
    if (SD.exists(Temporary) && !SD.remove(Temporary)) return false;
    JsonDocument json;
    json["ssid"] = settings.ssid;
    json["password"] = settings.password;
    json["timezone"] = settings.timezone;
    File file = SD.open(Temporary, FILE_WRITE);
    if (!file) return false;
    const bool written = serializeJson(json, file) == measureJson(json);
    file.close();
    Settings check;
    if (!written || !readSettings(Temporary, check) || check.ssid != settings.ssid ||
        check.password != settings.password || check.timezone != settings.timezone) return false;
    if (SD.exists(Path)) {
        Settings previous;
        if (readSettings(Path, previous)) {
            if (SD.exists(Backup) && !SD.remove(Backup)) return false;
            if (!SD.rename(Path, Backup)) return false;
        } else if (!SD.remove(Path)) {
            return false;
        }
    }
    if (!SD.rename(Temporary, Path)) return false;
    if (SD.exists(Backup)) SD.remove(Backup);
    return true;
}

void NetworkClock::apply(const Settings& settings) {
    timezone = settings.timezone;
    if (sntp_enabled()) sntp_stop();
    setenv("TZ", timezone.c_str(), 1);
    tzset();
    online = false;
    WiFi.disconnect();
    if (settings.ssid.isEmpty()) {
        WiFi.mode(WIFI_OFF);
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
}

void NetworkClock::poll() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !online) configTzTime(timezone.c_str(), "pool.ntp.org");
    online = connected;
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        const time_t now = time(nullptr);
        tm utc;
        if (now >= ValidClock && gmtime_r(&now, &utc)) M5.Rtc.setDateTime(&utc);
    }
}

void NetworkClock::formatTime(char (&text)[6]) const {
    const time_t now = time(nullptr);
    tm local;
    if (now < ValidClock || !localtime_r(&now, &local)) {
        strcpy(text, "--:--");
        return;
    }
    strftime(text, sizeof(text), "%H:%M", &local);
}
