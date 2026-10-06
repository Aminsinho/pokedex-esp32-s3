#include "HomeScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "PokemonService.h"
#include "CatalogSprite.h"

static lv_obj_t *scr, *well, *name_lbl, *id_lbl, *type_lbl, *status_lbl, *backend_dot;
static CatalogSprite sprite;
static uint16_t fav_id = 25;
static bool pending = true;
static uint32_t touchUntil = 0;

static void nav_pokedex_cb(lv_event_t*) { ScreenManager::getInstance().show(ScreenId::POKEDX_LIST); }
static void nav_scan_cb(lv_event_t*) { ScreenManager::getInstance().show(ScreenId::SCAN); }
static void sprite_cb(lv_event_t*) { HomeScreen::triggerBounce(); }

lv_obj_t* HomeScreen::create() {
    scr = Theme::shell();
    Theme::header(scr, "POKEDEX");
    Theme::pokeball(scr, 12, 13, 32);
    backend_dot = Theme::makeCard(scr, 292, 24, 10, 10);
    lv_obj_set_style_radius(backend_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(backend_dot, LV_OBJ_FLAG_CLICKABLE);
    auto* panel = Theme::makeCard(scr, 6, 60, 308, 116);
    Theme::panel(panel, Theme::card);
    well = Theme::makeCard(panel, 4, 4, 106, 104);
    Theme::panel(well, Theme::teal);
    lv_obj_add_event_cb(well, sprite_cb, LV_EVENT_PRESSED, nullptr);
    sprite.create(well, 3, 2, true, Theme::teal);
    id_lbl = Theme::makeLabel(panel, "No. 0025", Theme::font_small, Theme::gray);
    lv_obj_set_pos(id_lbl, 118, 8);
    name_lbl = Theme::makeLabel(panel, "", Theme::font_name, Theme::ink);
    lv_obj_set_pos(name_lbl, 118, 30);
    lv_obj_set_width(name_lbl, 176);
    lv_obj_set_height(name_lbl, 44);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_WRAP);
    type_lbl = Theme::makeLabel(panel, "", Theme::font_small, Theme::blue);
    lv_obj_set_pos(type_lbl, 118, 76);
    status_lbl = Theme::makeLabel(panel, "REGISTRO LOCAL", Theme::font_small, Theme::gray);
    lv_obj_set_pos(status_lbl, 118, 94);
    const char* captions[] = {LV_SYMBOL_LIST " REGISTRO", LV_SYMBOL_EYE_OPEN " SCAN"};
    for (int i = 0; i < 2; ++i) {
        auto* button = Theme::makeCard(scr, 6 + i * 158, 180, 150, 56);
        Theme::button(button, i ? Theme::blue : Theme::red);
        lv_obj_add_event_cb(button, i ? nav_scan_cb : nav_pokedex_cb, LV_EVENT_PRESSED, nullptr);
        lv_obj_center(Theme::makeLabel(button, captions[i], Theme::font_name, Theme::white));
    }
    return scr;
}

void HomeScreen::update() {
    if (pending) {
        pending = false;
        Pokemon p;
        if (PokemonService::getDetail(fav_id, p)) {
            lv_label_set_text(name_lbl, p.name.c_str());
            lv_label_set_text(type_lbl, p.type_count ? p.types[0].name.c_str() : "");
        } else {
            lv_label_set_text(name_lbl, "SIN DATOS");
            lv_label_set_text(type_lbl, "");
        }
        char text[24];
        snprintf(text, sizeof(text), "No. %04u", fav_id);
        lv_label_set_text(id_lbl, text);
        lv_label_set_text(status_lbl, sprite.show(fav_id) ? "REGISTRO LOCAL" : "SIN IMAGEN SD");
    }
    if (touchUntil && static_cast<int32_t>(lv_tick_get() - touchUntil) >= 0) {
        touchUntil = 0;
        lv_obj_set_style_border_color(well, Theme::steel, 0);
    }
}

void HomeScreen::setBackend(int state) {
    if (backend_dot) lv_obj_set_style_bg_color(backend_dot,
        state == 2 ? lv_color_hex(0x31BA73) : (state == 1 ? Theme::yellow : Theme::gray), 0);
}
void HomeScreen::setFavorite(uint16_t id) {
    fav_id = id ? id : 25;
    pending = true;
    sprite.hide();
}
uint16_t HomeScreen::getFavorite() { return fav_id; }
void HomeScreen::triggerBounce() {
    if (!well) return;
    touchUntil = lv_tick_get() + 250;
    lv_obj_set_style_border_color(well, lv_color_hex(0x53FFFF), 0);
}
