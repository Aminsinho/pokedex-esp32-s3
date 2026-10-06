#pragma once
#include "lvgl.h"
#include <stdint.h>

namespace ScanScreen {
    lv_obj_t* create();
    void onShow();
    void setState(const char* text, uint32_t color);   // main status line
    void setBackend(bool online);                       // backend indicator dot
    void setCamera(bool online);                        // camera indicator dot

    void showMatch(const char* name, float confidence);
    void reset();                                       // back to idle

    void startScanAnim();                              // activate scan-line animation
    void stopScanAnim();                               // stop scan-line animation
}
