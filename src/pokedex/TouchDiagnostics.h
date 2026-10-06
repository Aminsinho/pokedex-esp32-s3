#pragma once
#include <stdint.h>
#include "lvgl.h"

// ============================================================
// TOUCH DIAGNOSTICS — compile-time enabled
// Set to 0 to remove all diagnostics without touching architecture.
// ============================================================
#ifndef TOUCH_DIAGNOSTICS
#define TOUCH_DIAGNOSTICS 0
#endif

#if TOUCH_DIAGNOSTICS

// Light state written by touch callback (no LVGL calls here)
struct TouchDebugState {
    int16_t rawX;
    int16_t rawY;
    int16_t x;         // mapped
    int16_t y;         // mapped
    bool    pressed;
    uint32_t sequence; // increments on each PRESS transition
    uint32_t lastPressMs;
    uint32_t lastReleaseMs;
};

// Button event notification
struct BtnDebugEvent {
    const char* name;
    bool pressed;  // true=PRESSED, false=CLICKED
    uint32_t ms;   // timestamp when shown
};

// Calibration states
enum CalState { CAL_IDLE, CAL_WAIT_TL, CAL_WAIT_BR, CAL_DONE };

class TouchDiagnostics {
public:
    static void init();           // create overlay widgets
    static void update();         // call each frame from LVGL context
    static void onTouch(int16_t rawX, int16_t rawY, int16_t x, int16_t y, bool pressed);
    static void notifyButton(const char* name, bool isPressed);

    // For serial (one line per transition, not spam)
    static void logTransition(bool pressed, int16_t rawX, int16_t rawY, int16_t x, int16_t y);

private:
    static lv_obj_t* overlay_;
    static lv_obj_t* dot_;
    static lv_obj_t* counter_lbl_;
    static lv_obj_t* coords_lbl_;
    static lv_obj_t* btn_lbl_;
    static lv_obj_t* cal_btn_;

    static TouchDebugState state_;
    static bool wasPressed_;
    static BtnDebugEvent btnEvt_;

    // Calibration
    static CalState calState_;
    static int16_t calTlRawX, calTlRawY;
    static int16_t calBrRawX, calBrRawY;
};

#endif // TOUCH_DIAGNOSTICS
