#include "ScreenManager.h"
#include "BootScreen.h"
#include "HomeScreen.h"
#include "PokedexScreen.h"
#include "PokemonDetailScreen.h"
#include "ScanScreen.h"
#include "WifiManager.h"
#include "PokemonService.h"
#include "NetworkTask.h"

ScreenManager& ScreenManager::getInstance() {
    static ScreenManager inst;
    return inst;
}

void ScreenManager::init() {
    screens_[0] = BootScreen::create();
    screens_[1] = HomeScreen::create();
    screens_[2] = PokedexScreen::create();
    screens_[3] = PokemonDetailScreen::create();
    screens_[4] = ScanScreen::create();
}

void ScreenManager::show(ScreenId id) {
    int idx = (int)id;
    if (idx < 0 || idx > 4) return;
    if (screens_[idx] == nullptr) return;
    if (current_ == id) return;

    uint32_t started = millis();
    if (lv_indev_get_act()) lv_indev_wait_release(lv_indev_get_act());
    lv_scr_load(screens_[idx]);
    current_ = id;
    onShown(id);

    if (navCb_) navCb_();
    Serial.printf("[NAV] screen=%d show=%lu ms\n", idx, millis() - started);
}

void ScreenManager::showDetail(uint16_t pokemonId) {
    detailId_ = pokemonId;
    PokemonDetailScreen::update(pokemonId);
    show(ScreenId::POKEMON_DETAIL);
}

void ScreenManager::onShown(ScreenId id) {
    switch (id) {
        case ScreenId::HOME: {
            int s;
            if (!WifiManager::connected()) s = 1;                       // connecting
            else if (PokemonService::backend() == BackendState::ONLINE) s = 2;  // online
            else if (PokemonService::backend() == BackendState::UNKNOWN) s = 1; // connecting
            else s = 0;                                                  // offline
            HomeScreen::setBackend(s);
            break;
        }
        case ScreenId::POKEDX_LIST:
            PokedexScreen::onShow();
            break;
        case ScreenId::SCAN:
            ScanScreen::onShow();
            NetworkTask::post(NetCmd::CHECK_CAMERA);
            break;
        default:
            break;
    }
}
