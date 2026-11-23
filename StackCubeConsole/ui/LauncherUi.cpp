#include "LauncherUi.h"

#include <StackCubeHal.h>
#include <algorithm>

namespace {
using namespace stackcube;
constexpr int Width = hal::Width;
constexpr int Height = hal::Height;
constexpr int Pitch = 131;
constexpr int CoverWidth = 112;
constexpr int CoverHeight = 162;
constexpr int ArtWidth = 99;
constexpr int ArtHeight = 150;
constexpr uint32_t Background = 0x101922;
constexpr uint32_t Surface = 0x1C2A37;
constexpr uint32_t Line = 0x354858;
constexpr uint32_t Text = 0xF0F4F7;
constexpr uint32_t Muted = 0xA4B6C4;
constexpr uint32_t Accent = 0xF5BF74;
constexpr uint32_t Green = 0x86D9B2;

lv_obj_t* panel(lv_obj_t* parent, int width, int height, int x, int y, uint32_t color) {
    auto* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, x, y);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color) {
    auto* object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(object, font, 0);
    return object;
}

lv_obj_t* button(lv_obj_t* parent, const char* text, int width, int x, int y) {
    auto* object = lv_btn_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, 32);
    lv_obj_set_style_bg_color(object, lv_color_hex(Line), 0);
    lv_obj_set_style_radius(object, 6, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_outline_color(object, lv_color_hex(Accent), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(object, 2, LV_STATE_FOCUSED);
    lv_obj_center(label(object, text, &lv_font_montserrat_14, Text));
    return object;
}

bool due(uint32_t now, uint32_t target) { return int32_t(now - target) >= 0; }
}

void LauncherUi::begin() {
    auto* display = lv_disp_get_default();
    lv_disp_set_theme(display, lv_theme_default_init(display, lv_color_hex(Accent),
                                                     lv_color_hex(Line), true, LV_FONT_DEFAULT));
    lv_obj_t* root = lv_scr_act();
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_hex(Background), 0);
    makeHome(root);
    makeSettings(root);

    static lv_indev_drv_t pointerDriver;
    lv_indev_drv_init(&pointerDriver);
    pointerDriver.type = LV_INDEV_TYPE_POINTER;
    pointerDriver.user_data = this;
    pointerDriver.read_cb = [](lv_indev_drv_t* driver, lv_indev_data_t* data) {
        static_cast<LauncherUi*>(driver->user_data)->readPointer(data);
    };
    auto* input = lv_indev_drv_register(&pointerDriver);
    cursor = panel(lv_layer_sys(), 10, 10, 0, 0, Accent);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(cursor, lv_color_hex(Background), 0);
    lv_obj_set_style_border_width(cursor, 2, 0);
    lv_obj_clear_flag(cursor, LV_OBJ_FLAG_CLICKABLE);
    if (input) lv_indev_set_cursor(input, cursor);
    lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    mediaChanged(false, {});
}

void LauncherUi::makeHome(lv_obj_t* root) {
    lv_obj_set_pos(label(root, "StackCube", &lv_font_montserrat_20, Text), 22, 14);
    wifi = label(root, LV_SYMBOL_WIFI, &lv_font_montserrat_16, Muted);
    lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -79, 17);
    clock = label(root, "--:--", &lv_font_montserrat_16, Text);
    lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -22, 17);
    panel(root, Width - 44, 1, 22, 48, Line);

    carousel = panel(root, Width, 183, 0, 64, Background);
    row = panel(carousel, Width, 183, 0, 0, Background);
    lv_obj_set_style_clip_corner(carousel, true, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    for (int offset = -3; offset <= 3; ++offset) {
        auto& card = cards[offset + 3];
        card.ui = this;
        card.offset = offset;
        card.object = panel(row, CoverWidth, CoverHeight,
                            (Width - CoverWidth) / 2 + offset * Pitch, 8, Surface);
        lv_obj_set_style_radius(card.object, 6, 0);
        lv_obj_set_style_clip_corner(card.object, true, 0);
        lv_obj_set_style_border_width(card.object, offset ? 1 : 3, 0);
        lv_obj_set_style_border_color(card.object, lv_color_hex(offset ? Line : Accent), 0);
        lv_obj_set_style_shadow_width(card.object, offset ? 0 : 12, 0);
        lv_obj_set_style_shadow_color(card.object, lv_color_hex(0x080E14), 0);
        panel(card.object, 4, CoverHeight, 2, 0, Line);
        card.disc = panel(card.object, 62, 62, 25, 30, Line);
        lv_obj_set_style_radius(card.disc, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card.disc, 10, 0);
        lv_obj_set_style_border_color(card.disc, lv_color_hex(0x2B3B48), 0);
        auto* hub = panel(card.disc, 12, 12, 0, 0, Surface);
        lv_obj_center(hub);
        card.title = label(card.object, "", &lv_font_montserrat_12, Text);
        lv_obj_set_width(card.title, CoverWidth - 23);
        lv_obj_set_style_text_align(card.title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(card.title, LV_LABEL_LONG_DOT);
        lv_obj_align(card.title, LV_ALIGN_BOTTOM_MID, 2, -21);
        card.image = lv_img_create(card.object);
        lv_obj_clear_flag(card.image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(card.object, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card.object, [](lv_event_t* event) {
            auto* card = static_cast<Card*>(lv_event_get_user_data(event));
            if (card->index >= 0) card->ui->select(card->offset);
        }, LV_EVENT_CLICKED, &card);
    }
    title = label(root, "", &lv_font_montserrat_28, Text);
    lv_obj_set_width(title, Width - 70);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 253);
    position = label(root, "", &lv_font_montserrat_12, Muted);
    lv_obj_align(position, LV_ALIGN_TOP_MID, 0, 292);
    panel(root, Width - 44, 1, 22, 314, Line);
    help = label(root, LV_SYMBOL_LEFT "  " LV_SYMBOL_RIGHT "  Browse     Enter / A  Play", &lv_font_montserrat_12, Muted);
    lv_obj_set_pos(help, 22, 334);
    auto* play = button(root, "Play", 66, 426, 322);
    lv_obj_add_event_cb(play, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->select(0);
    }, LV_EVENT_CLICKED, this);
    auto* options = button(root, LV_SYMBOL_SETTINGS "  Settings", 118, 500, 322);
    lv_obj_add_event_cb(options, [](lv_event_t* event) {
        auto* ui = static_cast<LauncherUi*>(lv_event_get_user_data(event));
        if (!ui->busy) ui->showSettings(true);
    }, LV_EVENT_CLICKED, this);
}

void LauncherUi::makeSettings(lv_obj_t* root) {
    group = lv_group_create();
    settingsPanel = panel(root, Width, Height - 49, 0, 49, Background);
    lv_obj_set_pos(label(settingsPanel, "Settings", &lv_font_montserrat_20, Text), 22, 13);
    save = button(settingsPanel, "Save", 78, 450, 8);
    back = button(settingsPanel, "Back", 78, 540, 8);
    lv_obj_add_event_cb(save, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->saving = true;
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(back, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->showSettings(false);
    }, LV_EVENT_CLICKED, this);
    constexpr const char* names[] = {"Wi-Fi network", "Wi-Fi password", "Time zone (POSIX)"};
    constexpr unsigned max[] = {32, 64, 95};
    for (unsigned i = 0; i < 3; ++i) {
        const int y = 51 + 40 * i;
        lv_obj_set_pos(label(settingsPanel, names[i], &lv_font_montserrat_14, Text), 22, y + 9);
        auto* field = fields[i] = lv_textarea_create(settingsPanel);
        lv_obj_set_size(field, 436, 34);
        lv_obj_set_pos(field, 182, y);
        lv_obj_set_style_pad_top(field, 6, 0);
        lv_obj_set_style_pad_bottom(field, 6, 0);
        lv_obj_set_style_bg_color(field, lv_color_hex(Surface), 0);
        lv_textarea_set_one_line(field, true);
        lv_textarea_set_max_length(field, max[i]);
        lv_obj_add_event_cb(field, [](lv_event_t* event) {
            auto* ui = static_cast<LauncherUi*>(lv_event_get_user_data(event));
            ui->edit(lv_event_get_target(event), true);
        }, LV_EVENT_CLICKED, this);
        lv_group_add_obj(group, field);
    }
    lv_textarea_set_password_mode(fields[1], true);
    lv_textarea_set_placeholder_text(fields[0], "Leave blank to disable Wi-Fi");
    lv_textarea_set_placeholder_text(fields[2], "UTC0");
    lv_group_add_obj(group, save);
    lv_group_add_obj(group, back);
    settingsStatus = label(settingsPanel, "", &lv_font_montserrat_12, Muted);
    lv_obj_set_pos(settingsStatus, 22, 167);

    keyboard = lv_keyboard_create(settingsPanel);
    lv_obj_set_size(keyboard, Width, 123);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard, [](lv_event_t* event) {
        auto* ui = static_cast<LauncherUi*>(lv_event_get_user_data(event));
        ui->edit(nullptr, false);
    }, LV_EVENT_READY, this);
    lv_obj_add_event_cb(keyboard, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->edit(nullptr, false);
    }, LV_EVENT_CANCEL, this);
    lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
}

void LauncherUi::edit(lv_obj_t* target, bool showKeyboard) {
    lv_keyboard_set_textarea(keyboard, target);
    if (target) lv_group_focus_obj(target);
    if (showKeyboard) lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void LauncherUi::showSettings(bool show) {
    editing = show;
    controllerPointer = false;
    pointerDown = false;
    if (!show) {
        lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_textarea_set_text(fields[0], settings.ssid.c_str());
    lv_textarea_set_text(fields[1], settings.password.c_str());
    lv_textarea_set_text(fields[2], settings.timezone.c_str());
    lv_label_set_text(settingsStatus, mounted ? "Tab to move between fields." : "Insert an SD card to save.");
    lv_obj_clear_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
    edit(fields[0], false);
}

int LauncherUi::gameAt(int offset) const {
    const int count = catalog.size();
    if (!count) return -1;
    if (count < 7) {
        if (count < 3) {
            const int target = selected + offset;
            return target >= 0 && target < count ? target : -1;
        }
        if (offset < -(count - 1) / 2 || offset > count / 2) return -1;
    }
    return (selected + offset + count) % count;
}

void LauncherUi::mediaChanged(bool hasMountedCard, const Settings& newSettings, bool unreadable) {
    mounted = hasMountedCard;
    cardUnreadable = unreadable;
    settings = newSettings;
    requestedGame = -1;
    saving = false;
    selected = 0;
    animationStep = 0;
    lv_anim_del(row, nullptr);
    lv_obj_set_x(row, 0);
    lv_img_cache_invalidate_src(nullptr);
    showSettings(false);
    updateCards();
}

void LauncherUi::updateCards() {
    for (auto& card : cards) {
        card.index = gameAt(card.offset);
        if (card.index < 0) {
            lv_obj_add_flag(card.object, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(card.object, LV_OBJ_FLAG_HIDDEN);
        const auto& game = catalog[card.index];
        const bool artwork = !game.cover.isEmpty();
        if (artwork) {
            const String source = "S:" + game.cover;
            const unsigned zoom = std::min(ArtWidth * 256 / game.coverWidth, ArtHeight * 256 / game.coverHeight);
            lv_img_set_src(card.image, source.c_str());
            lv_img_set_zoom(card.image, zoom);
            lv_obj_align(card.image, LV_ALIGN_CENTER, 2, 0);
            lv_obj_clear_flag(card.image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card.disc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card.title, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(card.image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card.disc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card.title, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(card.title, game.title.c_str());
        }
    }
    if (catalog.size()) {
        lv_label_set_text(title, catalog[selected].title.c_str());
        lv_label_set_text_fmt(position, "%u / %u", unsigned(selected + 1), unsigned(catalog.size()));
    } else if (cardUnreadable) {
        lv_label_set_text(title, "SD card could not be mounted");
        lv_label_set_text(position, "Reinsert the card to retry");
    } else {
        lv_label_set_text(title, mounted ? "No games installed" : "Insert an SD card");
        lv_label_set_text(position, mounted ? "Copy game folders to /games" : "");
    }
}

void LauncherUi::move(int direction) {
    if (animationStep || gameAt(direction) < 0) return;
    animationStep = direction;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, row);
    lv_anim_set_user_data(&animation, this);
    lv_anim_set_values(&animation, 0, -direction * Pitch);
    lv_anim_set_time(&animation, 190);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
        lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
    });
    lv_anim_set_ready_cb(&animation, [](lv_anim_t* animation) {
        auto* ui = static_cast<LauncherUi*>(lv_anim_get_user_data(animation));
        ui->selected = ui->gameAt(ui->animationStep);
        ui->animationStep = 0;
        lv_obj_set_x(ui->row, 0);
        ui->updateCards();
    });
    lv_anim_start(&animation);
}

void LauncherUi::select(int offset) {
    if (busy || editing || animationStep || !catalog.size()) return;
    if (offset) move(offset);
    else requestedGame = selected;
}

void LauncherUi::handle(const Input& input, uint32_t now) {
    int dx = input.mouseX;
    int dy = input.mouseY;
    uint32_t padButtons = 0;
    bool padConnected = false;
    if (editing) {
        for (const auto& pad : input.pads) {
            if (!pad.connected) continue;
            padConnected = true;
            padButtons |= pad.buttons;
            if (pad.x < -14500) padButtons |= Left;
            if (pad.x > 14500) padButtons |= Right;
            if (pad.y < -14500) padButtons |= Up;
            if (pad.y > 14500) padButtons |= Down;
        }
    }
    if (!padConnected) controllerPointer = false;
    if (now - lastPointerMove >= 15 && (padButtons & (Left | Right | Up | Down))) {
        controllerPointer = true;
        lastPointerMove = now;
        dx += padButtons & Left ? -5 : padButtons & Right ? 5 : 0;
        dy += padButtons & Up ? -5 : padButtons & Down ? 5 : 0;
    }
    pointer.x = constrain(pointer.x + dx, 0, Width - 1);
    pointer.y = constrain(pointer.y + dy, 0, Height - 1);
    pointerButtons = input.mouseButtons;
    pointerDown = controllerPointer && (padButtons & Accept);
    if (input.mouseConnected || controllerPointer) lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    if (busy) return;
    if (editing) {
        if (input.pressed & (Back | Menu)) {
            showSettings(false);
            return;
        }
        auto* focused = lv_group_get_focused(group);
        auto** field = std::find(std::begin(fields), std::end(fields), focused);
        if (input.key == 0x2b || (input.key == 0x28 && field != std::end(fields))) {
            lv_group_focus_next(group);
            focused = lv_group_get_focused(group);
            field = std::find(std::begin(fields), std::end(fields), focused);
            edit(field != std::end(fields) ? focused : nullptr, false);
        } else if (field != std::end(fields)) {
            if (input.text) lv_textarea_add_char(focused, input.text);
            if (input.key == 0x2a) lv_textarea_del_char(focused);
            if (input.key == 0x4c) lv_textarea_del_char_forward(focused);
            if (input.key == 0x50) lv_textarea_cursor_left(focused);
            if (input.key == 0x4f) lv_textarea_cursor_right(focused);
        } else if (input.key == 0x28 || input.key == 0x58) {
            if (focused == save) saving = true;
            if (focused == back) showSettings(false);
        }
        return;
    }

    if (input.pressed & Menu) {
        showSettings(true);
        return;
    }
    if (input.pressed & Accept) select(0);
    if (input.wheel) move(input.wheel > 0 ? -1 : 1);
    const int direction = input.held & (Left | Up) ? -1 : input.held & (Right | Down) ? 1 : 0;
    if (direction != repeatDirection) {
        repeatDirection = direction;
        if (direction) move(direction);
        nextRepeat = now + 330;
    } else if (direction && due(now, nextRepeat)) {
        move(direction);
        nextRepeat = now + 200;
    }
}

void LauncherUi::readPointer(lv_indev_data_t* data) const {
    data->point = pointer;
    data->state = (pointerDown || (pointerButtons & 1)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void LauncherUi::status(bool online, const char* time) {
    lv_label_set_text(clock, time);
    lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -22, 17);
    lv_obj_set_style_text_color(wifi, lv_color_hex(online ? Green : Muted), 0);
    lv_obj_set_style_text_opa(wifi, online ? LV_OPA_COVER : LV_OPA_50, 0);
}

void LauncherUi::loading(unsigned progress) {
    busy = true;
    lv_label_set_text_fmt(help, "Loading game… %u%%", progress);
}

void LauncherUi::launchError(const char* error) {
    busy = false;
    lv_label_set_text(help, error);
}

int LauncherUi::takeGame() {
    const int request = requestedGame;
    requestedGame = -1;
    return request;
}

bool LauncherUi::takeSettings(Settings& result) {
    if (!saving) return false;
    saving = false;
    result = {lv_textarea_get_text(fields[0]), lv_textarea_get_text(fields[1]),
              lv_textarea_get_text(fields[2])};
    if (result.timezone.isEmpty()) result.timezone = "UTC0";
    return true;
}

void LauncherUi::settingsResult(bool saved, const Settings& result) {
    if (!saved) {
        lv_label_set_text(settingsStatus, mounted ? "Save failed. Check the time zone and SD card." :
                                                   "Insert an SD card to save.");
        return;
    }
    settings = result;
    lv_label_set_text(settingsStatus, "Saved to the SD card.");
}
