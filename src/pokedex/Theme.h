#pragma once
#include "lvgl.h"

namespace Theme {
    // Colors
    extern const lv_color_t bg;
    extern const lv_color_t card;
    extern const lv_color_t red;
    extern const lv_color_t red_dark;
    extern const lv_color_t white;
    extern const lv_color_t gray;
    extern const lv_color_t blue;
    extern const lv_color_t yellow;
    extern const lv_color_t ink;
    extern const lv_color_t steel;
    extern const lv_color_t teal;
    lv_obj_t* shell();
    void panel(lv_obj_t* obj, lv_color_t fill);
    void button(lv_obj_t* obj, lv_color_t fill);
    lv_obj_t* header(lv_obj_t* parent, const char* title);
    lv_obj_t* pokeball(lv_obj_t* parent, int x, int y, int size);

    // Fonts
    extern const lv_font_t* font_title;
    extern const lv_font_t* font_name;
    extern const lv_font_t* font_normal;
    extern const lv_font_t* font_small;

    // Helpers
    inline lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h) {
        lv_obj_t* obj = lv_obj_create(parent);
        lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_style_bg_color(obj, card, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(obj, 8, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    inline lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, font, 0);
        lv_obj_set_style_text_color(lbl, color, 0);
        return lbl;
    }
}
