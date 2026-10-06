#pragma once
#include "lvgl.h"
#include <stdint.h>

// Persistent image ownership; one bounded SD buffer per displayed sprite.
class CatalogSprite {
public:
    void create(lv_obj_t* parent, int x, int y, bool large, lv_color_t background);
    bool show(uint16_t id);
    void hide();
private:
    lv_obj_t* image_ = nullptr;
    uint8_t* buffer_ = nullptr;
    lv_img_dsc_t descriptor_ = {};
    bool large_ = true;
    lv_color_t background_ = {};
};
