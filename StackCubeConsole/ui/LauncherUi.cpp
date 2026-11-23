#include "LauncherUi.h"
#include "../integration/TimeZones.h"

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
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
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

void sendKey(lv_obj_t* dropdown, uint32_t key) {
    lv_event_send(dropdown, LV_EVENT_KEY, &key);
}

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
        card.symbol = label(card.object, LV_SYMBOL_SETTINGS, &lv_font_montserrat_28, Accent);
        lv_obj_align(card.symbol, LV_ALIGN_CENTER, 2, -12);
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
    help = label(root, LV_SYMBOL_LEFT "  " LV_SYMBOL_RIGHT "  Browse     Enter / A  Select", &lv_font_montserrat_12, Muted);
    lv_obj_set_pos(help, 22, 334);
}

void LauncherUi::makeSettings(lv_obj_t* root) {
    group = lv_group_create();
    settingsPanel = panel(root, Width, Height - 49, 0, 49, Background);
    lv_obj_set_pos(label(settingsPanel, "Settings", &lv_font_montserrat_20, Text), 22, 13);
    save = button(settingsPanel, "Save", 78, 182, 184);
    back = button(settingsPanel, "Back", 78, 270, 184);
    lv_obj_add_event_cb(save, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->saving = true;
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(back, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->showSettings(false);
    }, LV_EVENT_CLICKED, this);
    constexpr const char* names[] = {"Wi-Fi network", "Wi-Fi password"};
    constexpr unsigned max[] = {32, 64};
    for (unsigned i = 0; i < std::size(fields); ++i) {
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
    lv_obj_set_pos(label(settingsPanel, "Time zone", &lv_font_montserrat_14, Text), 22, 141);
    region = lv_dropdown_create(settingsPanel);
    city = lv_dropdown_create(settingsPanel);
    lv_obj_set_pos(region, 182, 131);
    lv_obj_set_pos(city, 366, 131);
    lv_obj_set_width(region, 172);
    lv_obj_set_width(city, 252);
    for (auto* dropdown : {region, city}) {
        lv_dropdown_set_dir(dropdown, LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(dropdown, lv_color_hex(Surface), 0);
        lv_obj_add_event_cb(dropdown, [](lv_event_t* event) {
            auto* ui = static_cast<LauncherUi*>(lv_event_get_user_data(event));
            auto* dropdown = lv_event_get_target(event);
            ui->edit(nullptr, false);
            lv_group_focus_obj(dropdown);
            lv_obj_set_style_max_height(lv_dropdown_get_list(dropdown), 133, 0);
        }, LV_EVENT_READY, this);
        lv_group_add_obj(group, dropdown);
    }
    lv_obj_add_event_cb(region, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->updateCities();
    }, LV_EVENT_VALUE_CHANGED, this);
    lv_group_add_obj(group, save);
    lv_group_add_obj(group, back);
    settingsStatus = label(settingsPanel, "", &lv_font_montserrat_12, Muted);
    lv_obj_set_pos(settingsStatus, 182, 228);

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
    textField = target;
    lv_keyboard_set_textarea(keyboard, target);
    if (target) lv_group_focus_obj(target);
    if (showKeyboard) {
        if (!mouseConnected) pointer = {Width / 2, Height - 60};
        ignorePointerAccept = true;
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        pointerDown = false;
    }
}

void LauncherUi::showSettings(bool show) {
    editing = show;
    repeatDirection = 0;
    pointerDown = false;
    edit(nullptr, false);
    for (auto* dropdown : {region, city}) {
        if (lv_dropdown_is_open(dropdown)) sendKey(dropdown, LV_KEY_ESC);
    }
    if (!show) {
        lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_textarea_set_text(fields[0], settings.ssid.c_str());
    lv_textarea_set_text(fields[1], settings.password.c_str());
    setTimeZone(settings.timezone);
    lv_label_set_text(settingsStatus, mounted ? "" : "Insert an SD card to save.");
    lv_obj_clear_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
    lv_group_focus_obj(fields[0]);
}

void LauncherUi::setTimeZone(const String& id) {
    const int zone = timezones::find(id == "UTC0" ? "Etc/UTC" : id.c_str());
    unsigned regionIndex = 0;
    String options;
    for (unsigned i = 0; i < timezones::regionCount; ++i) {
        const auto& entry = timezones::regions[i];
        if (i) options += '\n';
        options += entry.name;
        if (zone >= 0 && entry.first <= size_t(zone) && size_t(zone) < entry.first + entry.count) {
            regionIndex = i;
        }
    }
    if (zone < 0) {
        options += "\nExisting setting";
        regionIndex = timezones::regionCount;
    }
    lv_dropdown_set_options(region, options.c_str());
    lv_dropdown_set_selected(region, regionIndex);
    updateCities(zone < 0 ? 0 : size_t(zone) - timezones::regions[regionIndex].first);
}

void LauncherUi::updateCities(unsigned selectedCity) {
    const unsigned selectedRegion = lv_dropdown_get_selected(region);
    if (selectedRegion >= timezones::regionCount) {
        lv_dropdown_set_options_static(city, "Existing time zone");
        return;
    }
    const auto& entry = timezones::regions[selectedRegion];
    String options;
    options.reserve(entry.count * 24);
    for (size_t i = 0; i < entry.count; ++i) {
        if (i) options += '\n';
        String name = strchr(timezones::zones[entry.first + i].id, '/') + 1;
        name.replace('/', ' ');
        name.replace('_', ' ');
        options += name;
    }
    lv_dropdown_set_options(city, options.c_str());
    lv_dropdown_set_selected(city, selectedCity);
}

String LauncherUi::selectedTimeZone() const {
    const unsigned selectedRegion = lv_dropdown_get_selected(region);
    if (selectedRegion >= timezones::regionCount) return settings.timezone;
    const auto& entry = timezones::regions[selectedRegion];
    return timezones::zones[entry.first + lv_dropdown_get_selected(city)].id;
}

int LauncherUi::itemAt(int offset) const {
    const int count = catalog.size() + 1;
    const int target = selected + offset;
    return target >= 0 && target < count ? target : -1;
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
        card.index = itemAt(card.offset);
        if (card.index < 0) {
            lv_obj_add_flag(card.object, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(card.object, LV_OBJ_FLAG_HIDDEN);
        if (size_t(card.index) == catalog.size()) {
            lv_obj_add_flag(card.image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card.disc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card.symbol, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card.title, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(card.title, "Settings");
            continue;
        }
        lv_obj_add_flag(card.symbol, LV_OBJ_FLAG_HIDDEN);
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
    if (size_t(selected) < catalog.size()) {
        lv_label_set_text(title, catalog[selected].title.c_str());
        lv_label_set_text_fmt(position, "%u / %u", unsigned(selected + 1), unsigned(catalog.size()));
    } else {
        lv_label_set_text(title, "Settings");
        const char* status = "";
        if (cardUnreadable) status = "SD card could not be mounted";
        else if (!mounted) status = "Insert an SD card to load games";
        else if (!catalog.size()) status = "Copy game folders to /games";
        lv_label_set_text(position, status);
    }
}

void LauncherUi::move(int direction) {
    if (animationStep || itemAt(direction) < 0) return;
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
        ui->selected = ui->itemAt(ui->animationStep);
        ui->animationStep = 0;
        lv_obj_set_x(ui->row, 0);
        ui->updateCards();
    });
    lv_anim_start(&animation);
}

void LauncherUi::select(int offset) {
    if (busy || editing || animationStep) return;
    if (offset) move(offset);
    else if (size_t(selected) == catalog.size()) showSettings(true);
    else requestedGame = selected;
}

void LauncherUi::handle(const Input& input, uint32_t now) {
    uint32_t padButtons = 0;
    if (editing) {
        for (const auto& pad : input.pads) {
            if (!pad.connected) continue;
            padButtons |= pad.buttons;
            if (pad.x < -14500) padButtons |= Left;
            if (pad.x > 14500) padButtons |= Right;
            if (pad.y < -14500) padButtons |= Up;
            if (pad.y > 14500) padButtons |= Down;
        }
    }
    int dx = input.mouseX;
    int dy = input.mouseY;
    const bool virtualPointer = editing && !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    if (virtualPointer && now - lastPointerMove >= 15) {
        lastPointerMove = now;
        dx += padButtons & Left ? -5 : padButtons & Right ? 5 : 0;
        dy += padButtons & Up ? -5 : padButtons & Down ? 5 : 0;
    }
    pointer.x = constrain(pointer.x + dx, 0, Width - 1);
    pointer.y = constrain(pointer.y + dy, 0, Height - 1);
    pointerButtons = input.mouseButtons;
    mouseConnected = input.mouseConnected;
    if (!(padButtons & Accept)) ignorePointerAccept = false;
    pointerDown = virtualPointer && !ignorePointerAccept && (padButtons & Accept);
    if (mouseConnected || virtualPointer) lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    if (busy) return;
    if (editing) {
        handleSettings(input, padButtons, now);
        return;
    }

    if (input.pressed & Menu) {
        showSettings(true);
        return;
    }
    if (input.pressed & Accept) select(0);
    if (input.wheel) move(input.wheel > 0 ? -1 : 1);
    const int direction = input.held & (Left | Up) ? -1 : input.held & (Right | Down) ? 1 : 0;
    if (repeatStep(direction, now)) move(direction);
}

bool LauncherUi::repeatStep(int direction, uint32_t now) {
    if (direction != repeatDirection) {
        repeatDirection = direction;
        nextRepeat = now + 330;
        return direction != 0;
    } else if (direction && due(now, nextRepeat)) {
        nextRepeat = now + 200;
        return true;
    }
    return false;
}

void LauncherUi::handleSettings(const Input& input, uint32_t padButtons, uint32_t now) {
    auto* focused = lv_group_get_focused(group);
    const bool dropdown = focused == region || focused == city;
    const bool choosing = dropdown && lv_dropdown_is_open(focused);
    const bool enter = input.key == 0x28 || input.key == 0x58;
    const bool backPressed = input.key == 0x29 || (input.pressed & padButtons & Back);
    const bool keyboardVisible = !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    if (input.pressed & Menu) {
        showSettings(false);
        return;
    }
    if (backPressed) {
        if (choosing) sendKey(focused, LV_KEY_ESC);
        else if (textField) edit(nullptr, false);
        else showSettings(false);
        return;
    }

    const bool selectPressed = enter || (!keyboardVisible && (input.pressed & padButtons & Accept));
    const int padDirection = padButtons & (Up | Left) ? -1 : padButtons & (Down | Right) ? 1 : 0;
    int direction = repeatStep(keyboardVisible ? 0 : padDirection, now) ? padDirection : 0;
    if (input.key == 0x52 || (!textField && input.key == 0x50)) direction = -1;
    if (input.key == 0x51 || (!textField && input.key == 0x4f)) direction = 1;
    if (input.key == 0x2b) {
        if (choosing) sendKey(focused, LV_KEY_ESC);
        edit(nullptr, false);
        lv_group_focus_next(group);
        return;
    }
    if (choosing) {
        if (direction) sendKey(focused, direction < 0 ? LV_KEY_UP : LV_KEY_DOWN);
        if (selectPressed) lv_event_send(focused, LV_EVENT_RELEASED, nullptr);
        return;
    }
    if (direction) {
        edit(nullptr, false);
        if (direction < 0) lv_group_focus_prev(group);
        else lv_group_focus_next(group);
        return;
    }

    const bool isTextField = focused == fields[0] || focused == fields[1];
    if (isTextField && (input.text || input.key == 0x2a || input.key == 0x4c)) {
        if (textField != focused) edit(focused, false);
        if (input.text) lv_textarea_add_char(focused, input.text);
        if (input.key == 0x2a) lv_textarea_del_char(focused);
        if (input.key == 0x4c) lv_textarea_del_char_forward(focused);
    }
    if (isTextField && textField == focused) {
        if (input.key == 0x50) lv_textarea_cursor_left(focused);
        if (input.key == 0x4f) lv_textarea_cursor_right(focused);
    }
    if (!selectPressed) return;
    if (dropdown) {
        lv_dropdown_open(focused);
    } else if (isTextField && textField == focused) {
        edit(nullptr, false);
    } else if (focused) {
        lv_event_send(focused, LV_EVENT_CLICKED, nullptr);
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
    lv_label_set_text_fmt(help, "Loading game... %u%%", progress);
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
              selectedTimeZone()};
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
