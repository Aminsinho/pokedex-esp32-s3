#pragma once
#include "lvgl.h"
#include <stdint.h>

namespace PokedexScreen {
    lv_obj_t* create();
    void onShow();             // called by ScreenManager when shown
    void renderList();         // rebuild rows from cache (called by bridge)
    void showOffline();        // show error state
    void setSearching(bool s); // show/hide "SEARCHING..."
    void updateSearchResult(const char* name, bool found); // search result
}
