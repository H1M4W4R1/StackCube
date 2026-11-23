#pragma once

#include "../integration/Catalog.h"
#include "../integration/Settings.h"

#include <StackCubeInput.h>
#include <lvgl.h>

class LauncherUi {
public:
    explicit LauncherUi(const Catalog& catalog) : catalog(catalog) {}
    void begin();
    void mediaChanged(bool hasMountedCard, const Settings& settings, bool unreadable = false);
    void handle(const stackcube::Input& input, uint32_t now);
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
        int index;
        int offset;
    };
    void makeHome(lv_obj_t* root);
    void makeSettings(lv_obj_t* root);
    void showSettings(bool show);
    void edit(lv_obj_t* target, bool showKeyboard);
    void updateCards();
    void move(int direction);
    void select(int offset);
    int gameAt(int offset) const;
    void readPointer(lv_indev_data_t* data) const;

    const Catalog& catalog;
    Card cards[7] = {};
    lv_obj_t* carousel = nullptr;
    lv_obj_t* row = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* position = nullptr;
    lv_obj_t* help = nullptr;
    lv_obj_t* wifi = nullptr;
    lv_obj_t* clock = nullptr;
    lv_obj_t* cursor = nullptr;
    lv_obj_t* settingsPanel = nullptr;
    lv_obj_t* settingsStatus = nullptr;
    lv_obj_t* fields[3] = {};
    lv_obj_t* save = nullptr;
    lv_obj_t* back = nullptr;
    lv_obj_t* keyboard = nullptr;
    lv_group_t* group = nullptr;
    lv_point_t pointer = {320, 180};
    uint8_t pointerButtons = 0;
    bool controllerPointer = false, pointerDown = false;
    bool editing = false, saving = false, mounted = false, busy = false, cardUnreadable = false;
    int selected = 0, animationStep = 0, requestedGame = -1;
    int repeatDirection = 0;
    uint32_t nextRepeat = 0, lastPointerMove = 0;
    Settings settings;
};
