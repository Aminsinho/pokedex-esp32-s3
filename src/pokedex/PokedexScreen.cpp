#include "PokedexScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "PokemonService.h"
#include "Pokemon.h"
#include "FavoritesManager.h"
#include "TouchDiagnostics.h"
#include "CatalogSprite.h"
#include <stdio.h>

static lv_obj_t* scr = nullptr;
static lv_obj_t* list = nullptr;
static lv_obj_t* count_lbl = nullptr;
static lv_obj_t* state_lbl = nullptr;
static bool listDirty = true;
static constexpr int PAGE_SIZE = 12;
static int pageStart = 0;
static lv_obj_t* previous_btn = nullptr;
static lv_obj_t* next_btn = nullptr;
static CatalogSprite thumbnails[PAGE_SIZE];
static lv_obj_t* favoriteMarks[PAGE_SIZE] = {};
static uint16_t visibleIds[PAGE_SIZE] = {};
static int visibleCount = 0;

static void open_detail_cb(lv_event_t* e) {
    lv_obj_t* target = (lv_obj_t*)lv_event_get_user_data(e);
    if (!target) return;
    lv_obj_t* id_lbl = lv_obj_get_child(target, 0);
    const char* txt = lv_label_get_text(id_lbl);
    int id = 0;
    if (txt && txt[0] == '#') sscanf(txt + 1, "%d", &id);
    if (id > 0) {
        ScreenManager::getInstance().showDetail(id);
    }
}

#if TOUCH_DIAGNOSTICS
static void row_diag_cb(lv_event_t* e) {
    lv_obj_t* target = (lv_obj_t*)lv_event_get_user_data(e);
    if (!target) return;
    lv_obj_t* id_lbl = lv_obj_get_child(target, 0);
    const char* txt = id_lbl ? lv_label_get_text(id_lbl) : "?";
    char buf[24];
    snprintf(buf, sizeof(buf), "ROW %s", txt ? txt : "?");
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) TouchDiagnostics::notifyButton(buf, true);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) TouchDiagnostics::notifyButton(buf, false);
}
#endif

static void buildRow(lv_obj_t* parent, uint16_t id, const char* name, uint32_t typeColor, int slot) {
    if (!parent || !lv_obj_is_valid(parent)) return;
    lv_obj_t* row = lv_obj_create(parent);
    if (!row) return;
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, 316);
    lv_obj_set_height(row, 52);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_bg_color(row, Theme::card, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, Theme::steel, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xB5E5ED), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(row, Theme::blue, LV_STATE_PRESSED);
    lv_obj_set_style_pad_bottom(row, 2, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, open_detail_cb, LV_EVENT_CLICKED, row);
#if TOUCH_DIAGNOSTICS
    lv_obj_add_event_cb(row, row_diag_cb, LV_EVENT_ALL, row);
#endif

    // ID
    char buf[16];
    snprintf(buf, sizeof(buf), "#%03u", id);
    lv_obj_t* id_lbl = lv_label_create(row);
    if (!id_lbl) { lv_obj_del(row); return; }
    lv_label_set_text(id_lbl, buf);
    lv_obj_set_style_text_font(id_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(id_lbl, Theme::gray, 0);
    lv_obj_set_pos(id_lbl, 60, 4);

    // Name
    lv_obj_t* nm = lv_label_create(row);
    if (!nm) { lv_obj_del(row); return; }
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(nm, Theme::ink, 0);
    lv_obj_set_pos(nm, 60, 24);
    lv_obj_set_width(nm, 212);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);

    // Favorite star
    {
        lv_obj_t* star = lv_label_create(row);
        lv_label_set_text(star, LV_SYMBOL_OK);
        lv_obj_set_style_text_font(star, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(star, lv_color_hex(0xE8B93E), 0);
        lv_obj_align(star, LV_ALIGN_RIGHT_MID, -28, 0);
        favoriteMarks[slot] = star;
        visibleIds[slot] = id;
        if (!FavoritesMgr::isFavorite(id)) lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
    }

    thumbnails[slot].create(row, 4, 1, false, Theme::card);
    if (!thumbnails[slot].show(id)) Theme::pokeball(row, 14, 12, 26);

    // Type dot
    lv_obj_t* dot = lv_obj_create(row);
    if (!dot) { lv_obj_del(row); return; }
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(typeColor), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_90, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
}

static void rebuildList() {
    if (!list || !lv_obj_is_valid(list)) return;
    visibleCount = 0;
    for (int i = 0; i < PAGE_SIZE; ++i) favoriteMarks[i] = nullptr;
    // Clear existing rows
    for (int i = lv_obj_get_child_cnt(list) - 1; i >= 0; i--)
        lv_obj_del(lv_obj_get_child(list, i));

    if (!PokemonService::hasList()) return;

    int n = PokemonService::listCount();
    if (pageStart >= n) pageStart = 0;
    for (int i = pageStart; i < n && i < pageStart + PAGE_SIZE; i++) {
        const Pokemon& p = PokemonService::listItem(i);
        uint32_t tc = (p.type_count > 0) ? p.types[0].color : 0x5A5A5A;
        buildRow(list, p.id, p.name.c_str(), tc, i - pageStart);
        visibleCount++;
    }
    lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
    char count[32];
    snprintf(count, sizeof(count), "%d/%d", n ? pageStart / PAGE_SIZE + 1 : 0,
             (n + PAGE_SIZE - 1) / PAGE_SIZE);
    lv_label_set_text(count_lbl, count);
    if (pageStart == 0) lv_obj_add_state(previous_btn, LV_STATE_DISABLED);
    else lv_obj_clear_state(previous_btn, LV_STATE_DISABLED);
    if (pageStart + PAGE_SIZE >= n) lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    else lv_obj_clear_state(next_btn, LV_STATE_DISABLED);
    listDirty = false;
}

static void page_cb(lv_event_t* e) {
    const int n = PokemonService::listCount();
    const int step = lv_event_get_target(e) == next_btn ? PAGE_SIZE : -PAGE_SIZE;
    if (pageStart + step < 0 || pageStart + step >= n) return;
    pageStart += step;
    rebuildList();
}

static void back_cb(lv_event_t* e) {
    (void)e;
    ScreenManager::getInstance().show(ScreenId::HOME);
}

lv_obj_t* PokedexScreen::create() {
    scr = Theme::shell();
    if (!scr) return nullptr;
    Theme::header(scr, "REGISTRO");

    // Full-width BACK bar; keep the validated touch target.
    lv_obj_t* back = lv_obj_create(scr);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 100, 56);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(back, 4, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2A2D30), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    Theme::button(back, Theme::red);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_PRESSED, nullptr);
#if TOUCH_DIAGNOSTICS
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) TouchDiagnostics::notifyButton("BACK", true);
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) TouchDiagnostics::notifyButton("BACK", false);
    }, LV_EVENT_ALL, nullptr);
#endif
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " VOLVER");
    lv_obj_set_style_text_font(bl, Theme::font_small, 0);
    lv_obj_set_style_text_color(bl, Theme::white, 0);
    lv_obj_center(bl);

    // Page counter between the compact footer buttons.
    count_lbl = lv_label_create(scr);
    lv_label_set_text(count_lbl, "");
    lv_obj_set_style_text_font(count_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(count_lbl, Theme::gray, 0);
    lv_obj_align(count_lbl, LV_ALIGN_BOTTOM_MID, 0, -14);

    // Reclaim search and footer space for the list.
    list = lv_obj_create(scr);
    if (!list) return nullptr;
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, 316, 136);
    lv_obj_set_pos(list, 2, 60);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // State overlay (SEARCHING / NOT FOUND / OFFLINE)
    state_lbl = lv_label_create(scr);
    lv_label_set_long_mode(state_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(state_lbl, 300);
    lv_obj_set_style_text_font(state_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(state_lbl, Theme::gray, 0);
    lv_obj_align(state_lbl, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);

    previous_btn = lv_btn_create(scr);
    next_btn = lv_btn_create(scr);
    lv_obj_t* buttons[] = {previous_btn, next_btn};
    const char* captions[] = {"< PREV", "NEXT >"};
    for (int i = 0; i < 2; ++i) {
        lv_obj_set_size(buttons[i], 100, 36);
        lv_obj_set_pos(buttons[i], 4 + i * 212, 200);
        lv_obj_set_style_pad_all(buttons[i], 0, 0);
        lv_obj_set_style_text_font(buttons[i], &lv_font_montserrat_14, 0);
        lv_obj_clear_flag(buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        Theme::button(buttons[i], Theme::blue);
        lv_obj_add_event_cb(buttons[i], page_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* label = lv_label_create(buttons[i]);
        lv_label_set_text(label, captions[i]);
        lv_obj_center(label);
    }

    return scr;
}

void PokedexScreen::onShow() {
    // Preserve page and scroll position when returning from detail.
    lv_obj_add_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
    if (PokemonService::hasList()) {
        if (listDirty) rebuildList();
        for (int i = 0; i < visibleCount; ++i) {
            if (!favoriteMarks[i] || !lv_obj_is_valid(favoriteMarks[i])) continue;
            if (FavoritesMgr::isFavorite(visibleIds[i])) lv_obj_clear_flag(favoriteMarks[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(favoriteMarks[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Request list if not loaded
        PokemonService::requestList();
        lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(state_lbl, "LOADING...");
        lv_obj_set_style_text_color(state_lbl, Theme::gray, 0);
    }
}

void PokedexScreen::renderList() {
    rebuildList();
}

void PokedexScreen::showOffline() {
    lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(state_lbl, "LOAD FAILED\nCHECK SD CARD");
    lv_obj_set_style_text_color(state_lbl, lv_color_hex(0xE8B93E), 0);
}

void PokedexScreen::setSearching(bool s) {
    if (s) {
        lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(state_lbl, "SEARCHING...");
        lv_obj_set_style_text_color(state_lbl, lv_color_hex(0xE8B93E), 0);
    } else {
        lv_obj_add_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void PokedexScreen::updateSearchResult(const char* name, bool found) {
    if (found && name) {
        lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
        char buf[64];
        snprintf(buf, sizeof(buf), "FOUND: %s\n(tap to open)", name);
        lv_label_set_text(state_lbl, buf);
        lv_obj_set_style_text_color(state_lbl, lv_color_hex(0x4CAF50), 0);
    } else {
        lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(state_lbl, "NOT FOUND");
        lv_obj_set_style_text_color(state_lbl, lv_color_hex(0xE8B93E), 0);
    }
}
