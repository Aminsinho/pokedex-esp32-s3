#include "Theme.h"

const lv_color_t Theme::bg       = lv_color_hex(0xB8C6D1);
const lv_color_t Theme::card     = lv_color_hex(0xE3EBF0);
const lv_color_t Theme::red      = lv_color_hex(0xBE3856);
const lv_color_t Theme::red_dark = lv_color_hex(0x70263F);
const lv_color_t Theme::white    = lv_color_hex(0xF7F7F7);
const lv_color_t Theme::gray     = lv_color_hex(0x53677A);
const lv_color_t Theme::blue     = lv_color_hex(0x246483);
const lv_color_t Theme::yellow   = lv_color_hex(0xF6C945);
const lv_color_t Theme::ink      = lv_color_hex(0x263544);
const lv_color_t Theme::steel    = lv_color_hex(0x8197AA);
const lv_color_t Theme::teal     = lv_color_hex(0x86B9BC);

void Theme::panel(lv_obj_t* obj, lv_color_t fill) {
    lv_obj_set_style_bg_color(obj, fill, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, steel, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 7, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void Theme::button(lv_obj_t* obj, lv_color_t fill) {
    panel(obj, fill);
    lv_obj_set_style_bg_grad_color(obj, lv_color_mix(ink, fill, 100), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xD0E4EF), 0);
    lv_obj_set_style_text_color(obj, white, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x348F9A), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x70F0EF), LV_STATE_PRESSED);
    lv_obj_set_style_opa(obj, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* Theme::shell() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 320, 240);
    lv_obj_set_style_bg_color(scr, red_dark, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* plate = makeCard(scr, 3, 3, 314, 234);
    panel(plate, bg);
    lv_obj_set_style_border_color(plate, ink, 0);
    lv_obj_clear_flag(plate, LV_OBJ_FLAG_CLICKABLE);
    return scr;
}

lv_obj_t* Theme::header(lv_obj_t* parent, const char* title) {
    lv_obj_t* bar = makeCard(parent, 4, 4, 312, 52);
    panel(bar, card);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* lbl = makeLabel(bar, title, font_name, ink);
    lv_obj_center(lbl);
    return bar;
}

lv_obj_t* Theme::pokeball(lv_obj_t* parent, int x, int y, int size) {
    lv_obj_t* ball = makeCard(parent, x, y, size, size);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ball, 2, 0);
    lv_obj_set_style_border_color(ball, ink, 0);
    lv_obj_set_style_bg_color(ball, red, 0);
    lv_obj_set_style_bg_grad_color(ball, white, 0);
    lv_obj_set_style_bg_grad_dir(ball, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(ball, 127, 0);
    lv_obj_set_style_bg_grad_stop(ball, 128, 0);
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* band = makeCard(ball, 0, size / 2 - 3, size, 4);
    lv_obj_set_style_bg_color(band, ink, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* core = makeCard(ball, 0, 0, size / 3, size / 3);
    lv_obj_set_style_bg_color(core, white, 0);
    lv_obj_set_style_border_color(core, ink, 0);
    lv_obj_set_style_border_width(core, 2, 0);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(core);
    lv_obj_clear_flag(core, LV_OBJ_FLAG_CLICKABLE);
    return ball;
}

const lv_font_t* Theme::font_title  = &lv_font_montserrat_22;
const lv_font_t* Theme::font_name   = &lv_font_montserrat_18;
const lv_font_t* Theme::font_normal = &lv_font_montserrat_14;
const lv_font_t* Theme::font_small  = &lv_font_montserrat_14;
