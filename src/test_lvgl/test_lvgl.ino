#include <TFT_eSPI.h>
#include "lvgl.h"

TFT_eSPI tft;

static lv_color_t buf1[320 * 24];
static lv_color_t buf2[320 * 24];
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t total = w * h;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, total);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("=== LVGL MIN TEST ===");

    tft.init();
    tft.setRotation(1); // 320x240
    Serial.println("[OK] TFT");

    // First: plain TFT red screen to confirm display works
    tft.fillScreen(TFT_RED);
    delay(1000);
    Serial.println("[OK] RED SCREEN SHOWN");

    // Now LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 24);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = disp_flush;
    lv_disp_drv_register(&disp_drv);
    Serial.println("[OK] LVGL INIT");

    // Create a screen with BLUE background
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Add a label
    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "LVGL WORKS!");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scr);
    Serial.println("[OK] SCREEN LOADED");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
