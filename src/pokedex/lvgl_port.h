#pragma once
#include "lvgl.h"
#include <TFT_eSPI.h>

// Display buffer: 320x24x2 = 15360 bytes
#define DISP_BUF_LINES 24
extern TFT_eSPI tft;

void lvgl_port_init();
void lvgl_port_task();
void lvgl_port_capture(); // explicit USB diagnostic; no buffer/cost while idle

// Touch
void lvgl_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
