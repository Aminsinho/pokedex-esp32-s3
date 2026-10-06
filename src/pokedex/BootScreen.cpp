#include "BootScreen.h"
#include "Theme.h"
#include "ScreenManager.h"

static lv_obj_t* scr = nullptr;

static void boot_timer_cb(lv_timer_t* t) {
    ScreenManager::getInstance().show(ScreenId::HOME);
    lv_timer_del(t);
}

lv_obj_t* BootScreen::create() {
    scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 320, 240);
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xD62828), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    const int cx = 160, cy = 78;
    const lv_color_t dark = lv_color_hex(0x2A1515);
    const lv_color_t white = lv_color_hex(0xF7F7F7);
    const lv_color_t red = lv_color_hex(0xC02020);

    // 1. BORDER (dark circle)
    lv_obj_t* border = lv_obj_create(scr);
    lv_obj_remove_style_all(border);
    lv_obj_set_size(border, 88, 88);
    lv_obj_set_pos(border, cx - 44, cy - 44);
    lv_obj_set_style_radius(border, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(border, dark, 0);
    lv_obj_set_style_bg_opa(border, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(border, 0, 0);
    lv_obj_clear_flag(border, LV_OBJ_FLAG_SCROLLABLE);

    // 2. BALL BODY (red circle)
    lv_obj_t* ball = lv_obj_create(scr);
    lv_obj_remove_style_all(ball);
    lv_obj_set_size(ball, 80, 80);
    lv_obj_set_pos(ball, cx - 40, cy - 40);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ball, red, 0);
    lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ball, 0, 0);
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_SCROLLABLE);

    // 3. DOT RING (dark)
    lv_obj_t* dot_ring = lv_obj_create(scr);
    lv_obj_remove_style_all(dot_ring);
    lv_obj_set_size(dot_ring, 26, 26);
    lv_obj_set_pos(dot_ring, cx - 13, cy - 13);
    lv_obj_set_style_radius(dot_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_ring, dark, 0);
    lv_obj_set_style_bg_opa(dot_ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot_ring, 0, 0);
    lv_obj_clear_flag(dot_ring, LV_OBJ_FLAG_SCROLLABLE);

    // 4. DOT (white)
    lv_obj_t* dot = lv_obj_create(scr);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 18, 18);
    lv_obj_set_pos(dot, cx - 9, cy - 9);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, white, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    // 5. DOT CENTER (dark)
    lv_obj_t* dot_in = lv_obj_create(scr);
    lv_obj_remove_style_all(dot_in);
    lv_obj_set_size(dot_in, 8, 8);
    lv_obj_set_pos(dot_in, cx - 4, cy - 4);
    lv_obj_set_style_radius(dot_in, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_in, dark, 0);
    lv_obj_set_style_bg_opa(dot_in, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot_in, 0, 0);
    lv_obj_clear_flag(dot_in, LV_OBJ_FLAG_SCROLLABLE);

    // === TEXT ===
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "POKEDEX");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, white, 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 138);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, "SYSTEM ONLINE");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, white, 0);
    lv_obj_set_style_text_opa(sub, LV_OPA_70, 0);
    lv_obj_set_style_text_letter_space(sub, 2, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 178);

    // Transition after 1500ms
    lv_timer_create(boot_timer_cb, 1500, nullptr);

    return scr;
}
