#pragma once

#include "BoxArt.h"
#include "../integration/Catalog.h"
#include "../integration/Settings.h"

#include <StackCubeInput.h>
#include <lvgl.h>

class LauncherUi {
public:
    explicit LauncherUi(const Catalog& catalog) : catalog(catalog) {}
    void begin();
    void mediaChanged(bool hasInsertedCard, bool hasMountedCard, const Settings& settings);
    void handle(const stackcube::Input& input, uint32_t now);
    void loadCovers(uint32_t now);
    void status(bool online, const char* time);
    void loading(unsigned progress);
    void launchError(const char* error);
    int takeGame();
    bool takeSettings(Settings& settings);
    void settingsResult(bool saved, const Settings& result);

private:
    struct Card {
        LauncherUi* ui;
        lv_obj_t* object;
        lv_obj_t* image;
        lv_obj_t* title;
        lv_obj_t* disc;
        lv_obj_t* symbol;
        int index;
        int offset;
    };
    void makeHome(lv_obj_t* root);
    void makeSettings(lv_obj_t* root);
    void showSettings(bool show);
    void edit(lv_obj_t* target, bool showKeyboard);
    void setTimeZone(const String& id);
    void updateCities(unsigned city = 0);
    String selectedTimeZone() const;
    void handleSettings(const stackcube::Input& input, uint32_t padButtons);
    void updateCards();
    void updateCard(Card& card);
    void move(int direction);
    void select(int offset);
    int itemAt(int offset) const;
    void readPointer(lv_indev_data_t* data) const;

    const Catalog& catalog;
    BoxArt covers;
    Card cards[5] = {};
    lv_obj_t* title = nullptr;
    lv_obj_t* mediaStatus = nullptr;
    lv_obj_t* help = nullptr;
    lv_obj_t* sdIcon = nullptr;
    lv_obj_t* mouseIcon = nullptr;
    lv_obj_t* keyboardIcon = nullptr;
    lv_obj_t* gamepadIcon = nullptr;
    lv_obj_t* wifi = nullptr;
    lv_obj_t* clock = nullptr;
    lv_obj_t* cursor = nullptr;
    lv_obj_t* settingsPanel = nullptr;
    lv_obj_t* settingsStatus = nullptr;
    lv_obj_t* fields[2] = {};
    lv_obj_t* region = nullptr;
    lv_obj_t* city = nullptr;
    lv_obj_t* audio = nullptr;
    lv_obj_t* save = nullptr;
    lv_obj_t* back = nullptr;
    lv_obj_t* keyboard = nullptr;
    lv_obj_t* textField = nullptr;
    lv_group_t* group = nullptr;
    lv_point_t pointer = {320, 180};
    uint8_t pointerButtons = 0;
    bool mouseConnected = false, pointerDown = false, ignorePointerAccept = false;
    bool editing = false, saving = false, mounted = false, busy = false, cardUnreadable = false;
    bool coversPending = false, wifiOnline = false;
    int selected = 0, requestedGame = -1;
    uint32_t selectedAt = 0, lastPointerMove = 0;
    Settings settings;
};
