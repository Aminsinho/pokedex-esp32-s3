#include "TouchDiagnostics.h"
#include "app_config.h"
#include "Touch.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#if TOUCH_DIAGNOSTICS

// ─── Statics ─────────────────────────────────────────────────────────────────
lv_obj_t* TouchDiagnostics::overlay_ = nullptr;
lv_obj_t* TouchDiagnostics::dot_ = nullptr;
lv_obj_t* TouchDiagnostics::counter_lbl_ = nullptr;
lv_obj_t* TouchDiagnostics::coords_lbl_ = nullptr;
lv_obj_t* TouchDiagnostics::btn_lbl_ = nullptr;
lv_obj_t* TouchDiagnostics::cal_btn_ = nullptr;

TouchDebugState TouchDiagnostics::state_ = {0,0,0,0,false,0,0,0};
bool TouchDiagnostics::wasPressed_ = false;
BtnDebugEvent TouchDiagnostics::btnEvt_ = {nullptr, false, 0};

// Calibration
CalState TouchDiagnostics::calState_ = CAL_IDLE;
int16_t TouchDiagnostics::calTlRawX = 0, TouchDiagnostics::calTlRawY = 0;
int16_t TouchDiagnostics::calBrRawX = 0, TouchDiagnostics::calBrRawY = 0;

// ─── Init: create overlay widgets ────────────────────────────────────────────
void TouchDiagnostics::init() {
    lv_obj_t* scr = lv_scr_act();
    if (!scr) return;

    // Container — NOT clickable (no LV_OBJ_FLAG_CLICKABLE set)
    overlay_ = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay_);
    lv_obj_set_size(overlay_, 320, 240);
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_set_style_pad_all(overlay_, 0, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(overlay_, lv_obj_get_child_cnt(scr) - 1);

    // Dot (8px circle) — hidden by default
    dot_ = lv_obj_create(overlay_);
    lv_obj_remove_style_all(dot_);
    lv_obj_set_size(dot_, 8, 8);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot_, 1, 0);
    lv_obj_set_style_border_color(dot_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);

    // Touch counter label (top-left)
    counter_lbl_ = lv_label_create(overlay_);
    lv_obj_set_pos(counter_lbl_, 2, 2);
    lv_obj_set_style_text_font(counter_lbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(counter_lbl_, lv_color_hex(0x00FF00), 0);
    lv_label_set_text(counter_lbl_, "T:0");

    // Coords label (below counter)
    coords_lbl_ = lv_label_create(overlay_);
    lv_obj_set_pos(coords_lbl_, 2, 16);
    lv_obj_set_style_text_font(coords_lbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(coords_lbl_, lv_color_hex(0xFFFF00), 0);
    lv_label_set_text(coords_lbl_, "");

    // Button event label (bottom-center)
    btn_lbl_ = lv_label_create(overlay_);
    lv_obj_set_width(btn_lbl_, 280);
    lv_obj_set_pos(btn_lbl_, 10, 222);
    lv_obj_set_style_text_font(btn_lbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_lbl_, lv_color_hex(0xFF4444), 0);
    lv_label_set_text(btn_lbl_, "");

    // CALIBRATE button (top-right corner) — this IS clickable
    cal_btn_ = lv_obj_create(overlay_);
    lv_obj_set_size(cal_btn_, 50, 24);
    lv_obj_set_pos(cal_btn_, 264, 2);
    lv_obj_set_style_bg_color(cal_btn_, lv_color_hex(0x4444FF), 0);
    lv_obj_set_style_bg_opa(cal_btn_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cal_btn_, 4, 0);
    lv_obj_set_style_border_width(cal_btn_, 0, 0);
    lv_obj_add_flag(cal_btn_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cal_btn_, [](lv_event_t* e) {
        (void)e;
        // Start calibration
        TouchDiagnostics::calState_ = CAL_WAIT_TL;
        Serial.println("[CAL] Touch TOP-LEFT corner");
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cal_text = lv_label_create(cal_btn_);
    lv_obj_set_pos(cal_text, 8, 3);
    lv_obj_set_style_text_font(cal_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cal_text, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(cal_text, "CAL");
}

// ─── Update: call every frame ───────────────────────────────────────────────
void TouchDiagnostics::update() {
    if (!overlay_) return;

    // Reparent if screen changed
    lv_obj_t* active = lv_scr_act();
    if (active && lv_obj_get_parent(overlay_) != active) {
        lv_obj_set_parent(overlay_, active);
        lv_obj_move_to_index(overlay_, lv_obj_get_child_cnt(active) - 1);
    }

    uint32_t now = lv_tick_get();

    // Dot visibility
    if (state_.pressed) {
        int16_t dx = state_.x - 4;
        int16_t dy = state_.y - 4;
        if (dx < 0) dx = 0;
        if (dy < 0) dy = 0;
        lv_obj_set_pos(dot_, dx, dy);
        lv_obj_clear_flag(dot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(dot_, LV_OPA_COVER, 0);

        char buf[48];
        snprintf(buf, sizeof(buf), "R %d,%d M %d,%d",
                 state_.rawX, state_.rawY, state_.x, state_.y);
        lv_label_set_text(coords_lbl_, buf);
    } else if (state_.lastPressMs > 0) {
        uint32_t elapsed = now - state_.lastReleaseMs;
        if (elapsed < 200) {
            int16_t dx = state_.x - 4;
            int16_t dy = state_.y - 4;
            if (dx < 0) dx = 0;
            if (dy < 0) dy = 0;
            lv_obj_set_pos(dot_, dx, dy);
            lv_obj_clear_flag(dot_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(dot_, LV_OPA_50, 0);
        } else {
            lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(dot_, LV_OPA_COVER, 0);
        }
    }

    // Button label: show for 400ms
    if (btnEvt_.name) {
        if (now - btnEvt_.ms >= 400) {
            lv_label_set_text(btn_lbl_, "");
        }
    }

    // Calibration state display
    if (calState_ == CAL_WAIT_TL) {
        lv_label_set_text(btn_lbl_, ">>> TOUCH TOP-LEFT CORNER <<<");
    } else if (calState_ == CAL_WAIT_BR) {
        lv_label_set_text(btn_lbl_, ">>> TOUCH BOTTOM-RIGHT CORNER <<<");
    } else if (calState_ == CAL_DONE) {
        if (now - state_.lastPressMs > 2000) {
            calState_ = CAL_IDLE;
            lv_label_set_text(btn_lbl_, "");
        }
    }
}

// ─── Called from touch_read_cb ─────────────────────────────────────────────
void TouchDiagnostics::onTouch(int16_t rawX, int16_t rawY, int16_t x, int16_t y, bool pressed) {
    state_.rawX = rawX;
    state_.rawY = rawY;
    state_.x = x;
    state_.y = y;
    state_.pressed = pressed;

    if (pressed && !wasPressed_) {
        state_.sequence++;
        state_.lastPressMs = lv_tick_get();
        char buf[16];
        snprintf(buf, sizeof(buf), "T:%u", state_.sequence);
        if (counter_lbl_) lv_label_set_text(counter_lbl_, buf);
        logTransition(true, rawX, rawY, x, y);

        // ── Calibration state machine ──
        if (calState_ == CAL_WAIT_TL) {
            calTlRawX = rawX;
            calTlRawY = rawY;
            calState_ = CAL_WAIT_BR;
            Serial.printf("[CAL] TL: rawX=%d rawY=%d\n", rawX, rawY);
            Serial.println("[CAL] Now touch BOTTOM-RIGHT corner");
        } else if (calState_ == CAL_WAIT_BR) {
            calBrRawX = rawX;
            calBrRawY = rawY;
            calState_ = CAL_DONE;

            // Compute calibration
            // display_x = (rawY - xMin) * 319 / (xMax - xMin)
            // display_y = (yMax - rawX) * 239 / (yMax - yMin)
            // TL: rawX=high, rawY=low  → BR: rawX=low, rawY=high
            TouchCal cal;
            cal.xMin = calTlRawY; // low rawY = left
            cal.xMax = calBrRawY; // high rawY = right
            cal.yMin = calBrRawX; // low rawX = bottom
            cal.yMax = calTlRawX; // high rawX = top

            Touch::setCalibration(cal);
            Touch::saveCalibration();
            Serial.printf("[CAL] BR: rawX=%d rawY=%d\n", rawX, rawY);
            Serial.printf("[CAL] SAVED: xMin=%d xMax=%d yMin=%d yMax=%d\n",
                          cal.xMin, cal.xMax, cal.yMin, cal.yMax);
        }
    }
    if (!pressed && wasPressed_) {
        state_.lastReleaseMs = lv_tick_get();
        logTransition(false, rawX, rawY, x, y);
    }
    wasPressed_ = pressed;
}

// ─── Button event notification ───────────────────────────────────────────────
void TouchDiagnostics::notifyButton(const char* name, bool isPressed) {
    if (!btn_lbl_) return;
    char buf[48];
    if (isPressed) {
        snprintf(buf, sizeof(buf), "BTN: %s PRESSED", name);
    } else {
        snprintf(buf, sizeof(buf), "BTN: %s CLICK", name);
    }
    lv_label_set_text(btn_lbl_, buf);
    btnEvt_.name = name;
    btnEvt_.pressed = isPressed;
    btnEvt_.ms = lv_tick_get();
}

// ─── Serial log ──────────────────────────────────────────────────────────────
void TouchDiagnostics::logTransition(bool pressed, int16_t rawX, int16_t rawY, int16_t x, int16_t y) {
    if (pressed) {
        Serial.printf("[TOUCH] DOWN raw=%d,%d mapped=%d,%d seq=%u\n",
                      rawX, rawY, x, y, state_.sequence);
    } else {
        Serial.printf("[TOUCH] UP seq=%u\n", state_.sequence);
    }
}

#endif // TOUCH_DIAGNOSTICS
