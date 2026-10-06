#include "lvgl_port.h"
#include "Touch.h"
#include "TouchDiagnostics.h"
#include "AudioManager.h"

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * 48];
static lv_color_t buf2[320 * 48];
static lv_color_t* captureBuffer = nullptr;

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t total = w * h;
    if (captureBuffer) {
        for (uint32_t y = 0; y < h; ++y)
            memcpy(captureBuffer + (area->y1 + y) * 320 + area->x1, color_p + y * w, w * sizeof(lv_color_t));
    }

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, total);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

void lvgl_port_capture() {
    captureBuffer = static_cast<lv_color_t*>(lv_mem_alloc(320 * 240 * sizeof(lv_color_t)));
    if (!captureBuffer) { Serial.println("UIFRAME ERROR"); return; }
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(lv_disp_get_default());
    Serial.println("\nUIFRAME 320 240 153600");
    Serial.write(reinterpret_cast<uint8_t*>(captureBuffer), 153600);
    Serial.println("\nUIEND");
    lv_mem_free(captureBuffer);
    captureBuffer = nullptr;
}

static void touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    TouchPoint p = Touch::read();
    static bool wasPressed = false;
    if (p.touched && !wasPressed) AudioManager::playClick();
    wasPressed = p.touched;
    if (p.touched) {
        data->point.x = p.x;
        data->point.y = p.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
#if TOUCH_DIAGNOSTICS
    TouchDiagnostics::onTouch(p.rawX, p.rawY, p.x, p.y, p.touched);
#endif
}

void lvgl_port_init() {
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 48);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = disp_flush;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_t* input = lv_indev_drv_register(&indev_drv);
    lv_timer_set_period(input->driver->read_timer, 10);
}

static uint32_t last_ms_ = 0;
static uint32_t last_heartbeat_ = 0;

void lvgl_port_task() {
    // Advance the LVGL tick (this build uses LV_TICK_CUSTOM=0, so lv_tick_get()
    // only moves if we call lv_tick_inc()). Without this, timers/animations/touch
    // never progress.
    uint32_t now = millis();
    uint32_t delta = now - last_ms_;
    last_ms_ = now;
    lv_tick_inc(delta);

    uint32_t t0 = millis();
    lv_timer_handler();
    uint32_t t1 = millis();

    // Log if lv_timer_handler took > 200ms OR if there was a big gap between calls
    if (t1 - t0 > 200 || delta > 500) {
        Serial.printf("[LVGL] handler=%lu ms, gap=%lu ms\n", t1 - t0, delta);
    }

    // Heartbeat: log every 2s to see loop frequency
    if (t1 - last_heartbeat_ >= 10000) {
        last_heartbeat_ = t1;
        Serial.printf("[HB] loop ok\n");
    }

#if TOUCH_DIAGNOSTICS
    TouchDiagnostics::update();
#endif
}
