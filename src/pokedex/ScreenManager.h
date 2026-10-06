#pragma once
#include "lvgl.h"
#include <functional>

enum class ScreenId {
    BOOT,
    HOME,
    POKEDX_LIST,
    POKEMON_DETAIL,
    SCAN
};

class ScreenManager {
public:
    static ScreenManager& getInstance();

    void init();
    void show(ScreenId id);
    ScreenId current() const { return current_; }

    // Callbacks
    using NavCallback = std::function<void()>;
    void setNavCallback(NavCallback cb) { navCb_ = cb; }

    // Detail param
    void showDetail(uint16_t pokemonId);

private:
    ScreenManager() = default;
    static void onShown(ScreenId id);

    ScreenId current_ = (ScreenId)-1;
    NavCallback navCb_;

    lv_obj_t* screens_[5] = {nullptr};
    uint16_t detailId_ = 0;
};
