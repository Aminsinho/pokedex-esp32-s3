#pragma once
#include "lvgl.h"

namespace HomeScreen {
    lv_obj_t* create();
    void setBackend(int state);  // 0=offline 1=connecting 2=online
    // Set the favorite Pokemon ID (triggers sprite load)
    void setFavorite(uint16_t id);
    // Animation state
    enum AnimState { ANIM_IDLE, ANIM_BOUNCE, ANIM_TOUCHED };
    void triggerBounce();
    // Update sprite if not yet loaded (call from loop)
    void update();
    // Get current favorite ID
    uint16_t getFavorite();
}
