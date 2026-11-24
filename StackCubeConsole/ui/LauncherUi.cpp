#include "LauncherUi.h"
#include "../integration/TimeZones.h"

#include <StackCubeHal.h>

namespace {
using namespace stackcube;
constexpr int Width = hal::Width;
constexpr int Height = hal::Height;
constexpr int ScreenInset = 22;
constexpr int StatusRowTop = 17;
constexpr int HeaderHeight = 49;
constexpr int Pitch = 131;
constexpr int CoverWidth = 112;
constexpr int CoverHeight = 162;
constexpr uint32_t Background = 0x101922;
constexpr uint32_t Surface = BoxArt::Background;
constexpr uint32_t Line = 0x354858;
constexpr uint32_t Text = 0xF0F4F7;
constexpr uint32_t Muted = 0xA4B6C4;
constexpr uint32_t Accent = 0xF5BF74;
constexpr uint32_t Green = 0x86D9B2;
constexpr AudioOutput AudioOptions[] = {AudioOutput::Speaker, AudioOutput::Hdmi, AudioOutput::Off};
constexpr int VolumeStep = 5;

// One-bit opacity masks, stored as two bytes per 16-pixel row.
constexpr uint8_t MousePixels[] = {
    0x00, 0x00,  0x03, 0xc0,  0x07, 0xe0,  0x0f, 0xf0,
    0x1e, 0x78,  0x1e, 0x78,  0x1e, 0x78,  0x1f, 0xf8,
    0x1f, 0xf8,  0x1f, 0xf8,  0x1f, 0xf8,  0x1f, 0xf8,
    0x0f, 0xf0,  0x07, 0xe0,  0x03, 0xc0,  0x00, 0x00,
};
constexpr uint8_t GamepadPixels[] = {
    0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x0f, 0xf0,
    0x1f, 0xf8,  0x3f, 0xdc,  0x37, 0xfc,  0x63, 0xae,
    0x77, 0xfe,  0x7f, 0xde,  0x7c, 0x3e,  0x38, 0x1c,
    0x10, 0x08,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,
};
constexpr auto StatusIconHeader = [] {
    lv_img_header_t header{};
    header.cf = LV_IMG_CF_ALPHA_1BIT;
    header.w = header.h = 16;
    return header;
}();
constexpr lv_img_dsc_t MouseImage{StatusIconHeader, sizeof(MousePixels), MousePixels};
constexpr lv_img_dsc_t GamepadImage{StatusIconHeader, sizeof(GamepadPixels), GamepadPixels};

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

lv_obj_t* statusImage(lv_obj_t* parent, const lv_img_dsc_t& source) {
    auto* object = lv_img_create(parent);
    lv_img_set_src(object, &source);
    lv_obj_set_style_img_recolor(object, lv_color_hex(Text), 0);
    lv_obj_set_style_img_recolor_opa(object, LV_OPA_COVER, 0);
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

void setVisible(lv_obj_t* object, bool visible) {
    if (visible && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    if (!visible && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

int directionOf(uint32_t buttons) {
    return buttons & (Left | Up) ? -1 : buttons & (Right | Down) ? 1 : 0;
}

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
    mediaChanged(false, false, {});
}

void LauncherUi::makeHome(lv_obj_t* root) {
    auto* statusIcons = panel(root, LV_SIZE_CONTENT, lv_font_montserrat_16.line_height, 0, 0, Background);
    lv_obj_align(statusIcons, LV_ALIGN_TOP_RIGHT, -79, StatusRowTop);
    lv_obj_set_flex_flow(statusIcons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(statusIcons, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(statusIcons, 8, 0);
    sdIcon = label(statusIcons, LV_SYMBOL_SD_CARD, &lv_font_montserrat_16, Text);
    mouseIcon = statusImage(statusIcons, MouseImage);
    keyboardIcon = label(statusIcons, LV_SYMBOL_KEYBOARD, &lv_font_montserrat_16, Text);
    gamepadIcon = statusImage(statusIcons, GamepadImage);
    for (auto* icon : {sdIcon, mouseIcon, keyboardIcon, gamepadIcon}) setVisible(icon, false);
    wifi = label(statusIcons, LV_SYMBOL_WIFI, &lv_font_montserrat_16, Muted);
    lv_obj_set_style_text_opa(wifi, LV_OPA_50, 0);
    clock = label(root, "--:--", &lv_font_montserrat_16, Text);
    lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -ScreenInset, StatusRowTop);
    panel(root, Width - 2 * ScreenInset, 1, ScreenInset, HeaderHeight - 1, Line);

    auto* carousel = panel(root, Width, 183, 0, 64, Background);
    for (int offset = -2; offset <= 2; ++offset) {
        auto& card = cards[offset + 2];
        card.ui = this;
        card.offset = offset;
        card.object = panel(carousel, CoverWidth, CoverHeight,
                            (Width - CoverWidth) / 2 + offset * Pitch, 8, Surface);
        lv_obj_set_style_radius(card.object, 6, 0);
        lv_obj_set_style_border_width(card.object, offset ? 1 : 3, 0);
        lv_obj_set_style_border_color(card.object, lv_color_hex(offset ? Line : Accent), 0);
        lv_obj_set_style_shadow_width(card.object, offset ? 0 : 12, 0);
        lv_obj_set_style_shadow_color(card.object, lv_color_hex(0x080E14), 0);
        panel(card.object, 4, CoverHeight - 12, 2, 6, Line);
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
    mediaStatus = label(root, "", &lv_font_montserrat_12, Muted);
    lv_obj_align(mediaStatus, LV_ALIGN_TOP_MID, 0, 292);
    panel(root, Width - 44, 1, 22, 314, Line);
    help = label(root, LV_SYMBOL_LEFT "  " LV_SYMBOL_RIGHT "  Browse     Enter / A  Select", &lv_font_montserrat_12, Muted);
    lv_obj_set_pos(help, 22, 334);
}

void LauncherUi::makeSettings(lv_obj_t* root) {
    constexpr int fieldX = 182;
    constexpr int fieldWidth = Width - ScreenInset - fieldX;
    constexpr int rowTop = 8;
    constexpr int rowPitch = 40;
    constexpr int fieldHeight = 34;
    const int labelOffset = (fieldHeight - lv_font_montserrat_14.line_height) / 2;

    group = lv_group_create();
    settingsTitle = label(root, "Settings", &lv_font_montserrat_20, Text);
    const int titleOffset = (lv_font_montserrat_16.line_height - lv_font_montserrat_20.line_height) / 2;
    lv_obj_set_pos(settingsTitle, ScreenInset, StatusRowTop + titleOffset);
    lv_obj_add_flag(settingsTitle, LV_OBJ_FLAG_HIDDEN);
    settingsPanel = panel(root, Width, Height - HeaderHeight, 0, HeaderHeight, Background);
    constexpr const char* names[] = {"Wi-Fi network", "Wi-Fi password"};
    constexpr unsigned max[] = {32, 64};
    for (unsigned i = 0; i < std::size(fields); ++i) {
        const int y = rowTop + rowPitch * i;
        lv_obj_set_pos(label(settingsPanel, names[i], &lv_font_montserrat_14, Text), ScreenInset, y + labelOffset);
        auto* field = fields[i] = lv_textarea_create(settingsPanel);
        lv_obj_set_size(field, fieldWidth, fieldHeight);
        lv_obj_set_pos(field, fieldX, y);
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
    constexpr int timeZoneY = rowTop + 2 * rowPitch;
    constexpr int audioOutputY = timeZoneY + rowPitch;
    lv_obj_set_pos(label(settingsPanel, "Time zone", &lv_font_montserrat_14, Text), ScreenInset, timeZoneY + labelOffset);
    region = lv_dropdown_create(settingsPanel);
    city = lv_dropdown_create(settingsPanel);
    lv_obj_set_pos(label(settingsPanel, "Audio output", &lv_font_montserrat_14, Text), ScreenInset, audioOutputY + labelOffset);
    audio = lv_dropdown_create(settingsPanel);
    lv_dropdown_set_options_static(audio, "Built-in speaker\nHDMI\nOff");
    constexpr int regionWidth = 172;
    constexpr int cityOffset = regionWidth + 12;
    lv_obj_set_pos(region, fieldX, timeZoneY);
    lv_obj_set_pos(city, fieldX + cityOffset, timeZoneY);
    lv_obj_set_pos(audio, fieldX, audioOutputY);
    lv_obj_set_size(region, regionWidth, fieldHeight);
    lv_obj_set_size(city, fieldWidth - cityOffset, fieldHeight);
    lv_obj_set_size(audio, 260, fieldHeight);
    for (auto* dropdown : {region, city, audio}) {
        lv_dropdown_set_dir(dropdown, LV_DIR_BOTTOM);
        lv_obj_set_style_pad_ver(dropdown, 7, 0);
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

    constexpr const char* volumeLabels[] = {"Master volume", "Music volume", "Effects volume"};
    constexpr int volumeTop = audioOutputY + fieldHeight + 18;
    constexpr int volumePitch = 30;
    constexpr int valueWidth = 44;
    constexpr int valueX = Width - ScreenInset - valueWidth;
    constexpr int sliderHeight = 8;
    const int volumeLabelOffset = lv_font_montserrat_14.line_height / 2;
    for (unsigned i = 0; i < std::size(volumeControls); ++i) {
        auto& control = volumeControls[i];
        const int y = volumeTop + volumePitch * i;
        lv_obj_set_pos(label(settingsPanel, volumeLabels[i], &lv_font_montserrat_14, Text), ScreenInset, y - volumeLabelOffset);
        control.value = label(settingsPanel, "", &lv_font_montserrat_14, Text);
        lv_obj_set_width(control.value, valueWidth);
        lv_obj_set_style_text_align(control.value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(control.value, valueX, y - volumeLabelOffset);
        auto* slider = control.slider = lv_slider_create(settingsPanel);
        lv_obj_set_pos(slider, fieldX, y - sliderHeight / 2);
        lv_obj_set_size(slider, valueX - fieldX - 12, sliderHeight);
        lv_slider_set_range(slider, 0, MaxAudioVolume);
        lv_obj_set_style_bg_color(slider, lv_color_hex(Line), LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, lv_color_hex(Accent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(Text), LV_PART_KNOB);
        lv_obj_set_style_outline_color(slider, lv_color_hex(Accent), LV_STATE_FOCUSED);
        lv_obj_set_style_outline_width(slider, 2, LV_STATE_FOCUSED);
        lv_obj_add_event_cb(slider, [](lv_event_t* event) {
            auto* control = static_cast<VolumeControl*>(lv_event_get_user_data(event));
            lv_label_set_text_fmt(control->value, "%d%%", int(lv_slider_get_value(control->slider)));
        }, LV_EVENT_VALUE_CHANGED, &control);
        lv_group_add_obj(group, slider);
    }

    constexpr int buttonWidth = 78;
    constexpr int buttonGap = 10;
    constexpr int buttonX = (Width - 2 * buttonWidth - buttonGap) / 2;
    constexpr int buttonY = Height - HeaderHeight - 53;
    auto* save = button(settingsPanel, "Save", buttonWidth, buttonX, buttonY);
    auto* back = button(settingsPanel, "Back", buttonWidth, buttonX + buttonWidth + buttonGap, buttonY);
    lv_obj_add_event_cb(save, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->saving = true;
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(back, [](lv_event_t* event) {
        static_cast<LauncherUi*>(lv_event_get_user_data(event))->showSettings(false);
    }, LV_EVENT_CLICKED, this);
    lv_group_add_obj(group, save);
    lv_group_add_obj(group, back);
    settingsStatus = label(settingsPanel, "", &lv_font_montserrat_12, Muted);
    lv_obj_align(settingsStatus, LV_ALIGN_BOTTOM_MID, 0, -3);

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
    if (show) covers.cancel();
    pointerDown = false;
    edit(nullptr, false);
    for (auto* dropdown : {region, city, audio}) {
        if (lv_dropdown_is_open(dropdown)) sendKey(dropdown, LV_KEY_ESC);
    }
    setVisible(settingsTitle, show);
    if (!show) {
        lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_textarea_set_text(fields[0], settings.ssid.c_str());
    lv_textarea_set_text(fields[1], settings.password.c_str());
    setTimeZone(settings.timezone);
    for (unsigned i = 0; i < std::size(AudioOptions); ++i) {
        if (AudioOptions[i] == settings.audioOutput) lv_dropdown_set_selected(audio, i);
    }
    const uint8_t levels[] = {settings.audioLevels.master, settings.audioLevels.music, settings.audioLevels.effects};
    for (unsigned i = 0; i < std::size(volumeControls); ++i) {
        auto* slider = volumeControls[i].slider;
        lv_slider_set_value(slider, levels[i], LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    }
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

void LauncherUi::mediaChanged(bool hasInsertedCard, bool hasMountedCard, const Settings& newSettings) {
    mounted = hasMountedCard;
    cardUnreadable = hasInsertedCard && !hasMountedCard;
    setVisible(sdIcon, hasInsertedCard);
    settings = newSettings;
    requestedGame = -1;
    saving = false;
    selected = 0;
    for (auto& card : cards) {
        lv_img_set_src(card.image, nullptr);
        card.index = -2;
    }
    covers.clear();
    coversPending = true;
    selectedAt = millis();
    showSettings(false);
    updateCards();
}

void LauncherUi::updateCards() {
    for (auto& card : cards) {
        const int index = itemAt(card.offset);
        if (index == card.index) continue;
        card.index = index;
        updateCard(card);
    }
    const bool gameSelected = size_t(selected) < catalog.size();
    lv_label_set_text(title, gameSelected ? catalog[selected].title.c_str() : "Settings");

    const char* status = "";
    if (!gameSelected) {
        if (cardUnreadable) status = "SD card could not be mounted";
        else if (!mounted) status = "Insert an SD card to load games";
        else if (!catalog.size()) status = "Copy game folders to /games";
    }
    lv_label_set_text(mediaStatus, status);
}

void LauncherUi::updateCard(Card& card) {
    const bool valid = card.index >= 0;
    const bool game = valid && size_t(card.index) < catalog.size();
    const auto* image = game ? covers.image(card.index) : nullptr;
    if (lv_img_get_src(card.image) != image) {
        lv_img_set_src(card.image, image);
        if (image) lv_obj_align(card.image, LV_ALIGN_CENTER, 2, 0);
    }
    setVisible(card.object, valid);
    setVisible(card.image, image != nullptr);
    setVisible(card.disc, game && !image);
    setVisible(card.symbol, valid && !game);
    setVisible(card.title, !image);
    if (valid && !image) lv_label_set_text(card.title, game ? catalog[card.index].title.c_str() : "Settings");
}

void LauncherUi::loadCovers(uint32_t now) {
    // Commit input and draw the new selection before reading its art from the SD card.
    if (busy || editing || !coversPending || now - selectedAt < 60) return;
    constexpr int nearestFirst[] = {2, 1, 3, 0, 4};
    for (const int slot : nearestFirst) {
        auto& card = cards[slot];
        if (card.index < 0 || size_t(card.index) >= catalog.size() || covers.contains(card.index)) continue;
        const auto& game = catalog[card.index];
        if (game.cover.isEmpty() && game.preparedCover.isEmpty()) continue;
        if (covers.poll(card.index, game)) updateCard(card);
        return;
    }
    coversPending = false;
}

void LauncherUi::move(int direction) {
    const int target = itemAt(direction);
    if (target < 0) return;
    covers.cancel();
    selected = target;
    selectedAt = millis();
    coversPending = true;
    updateCards();
}

void LauncherUi::select(int offset) {
    if (busy || editing) return;
    if (offset) move(offset);
    else if (size_t(selected) == catalog.size()) showSettings(true);
    else requestedGame = selected;
}

void LauncherUi::handle(const Input& input, uint32_t now) {
    uint32_t padButtons = 0;
    bool gamepadConnected = false;
    for (const auto& pad : input.pads) {
        if (!pad.connected) continue;
        gamepadConnected = true;
        if (!editing) break;
        padButtons |= pad.buttons;
        if (pad.x < -14500) padButtons |= Left;
        if (pad.x > 14500) padButtons |= Right;
        if (pad.y < -14500) padButtons |= Up;
        if (pad.y > 14500) padButtons |= Down;
    }
    setVisible(mouseIcon, input.mouseConnected);
    setVisible(keyboardIcon, input.keyboardConnected);
    setVisible(gamepadIcon, gamepadConnected);
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
    setVisible(cursor, mouseConnected || virtualPointer);
    if (busy) return;
    if (editing) {
        handleSettings(input, padButtons);
        return;
    }

    if (input.pressed & Menu) {
        showSettings(true);
        return;
    }
    const int direction = directionOf(input.pressed);
    if (input.pressed & Accept) select(0);
    else if (direction) move(direction);
    else if (input.wheel) move(input.wheel > 0 ? -1 : 1);
}

void LauncherUi::handleSettings(const Input& input, uint32_t padButtons) {
    auto* focused = lv_group_get_focused(group);
    const bool dropdown = focused == region || focused == city || focused == audio;
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
    if (!keyboardVisible) {
        const auto pressed = input.pressed & padButtons;
        const bool left = input.key == 0x50 || (pressed & Left);
        const bool right = input.key == 0x4f || (pressed & Right);
        if (left != right) {
            for (const auto& control : volumeControls) {
                if (focused != control.slider) continue;
                lv_slider_set_value(focused, lv_slider_get_value(focused) + (right ? VolumeStep : -VolumeStep), LV_ANIM_OFF);
                lv_event_send(focused, LV_EVENT_VALUE_CHANGED, nullptr);
                return;
            }
        }
    }
    int direction = keyboardVisible ? 0 : directionOf(input.pressed & padButtons);
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
    if (strcmp(lv_label_get_text(clock), time)) {
        lv_label_set_text(clock, time);
        lv_obj_align(clock, LV_ALIGN_TOP_RIGHT, -ScreenInset, StatusRowTop);
    }
    if (online == wifiOnline) return;
    wifiOnline = online;
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
    const auto level = [this](unsigned index) { return uint8_t(lv_slider_get_value(volumeControls[index].slider)); };
    result = {lv_textarea_get_text(fields[0]), lv_textarea_get_text(fields[1]), selectedTimeZone(),
              AudioOptions[lv_dropdown_get_selected(audio)], {level(0), level(1), level(2)}};
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
