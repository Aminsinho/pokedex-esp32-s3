#pragma once
#include <Arduino.h>

/* ============================================================
 * FavoritesManager — NVS-backed favorites (up to 10 Pokémon).
 *
 * Usage:
 *   FavoritesMgr::begin();
 *   FavoritesMgr::toggle(25);       // add/remove
 *   FavoritesMgr::isFavorite(25);   // check
 *   FavoritesMgr::count();          // how many
 *   FavoritesMgr::get(2);           // 3rd favorite (0-indexed)
 *   FavoritesMgr::clear();          // remove all
 * ============================================================ */

class FavoritesMgr {
public:
    static const int MAX_FAVS = 10;

    static void begin();
    static void toggle(uint16_t id);
    static bool isFavorite(uint16_t id);
    static int count();
    static uint16_t get(int index);   // 0-indexed, returns 0 if OOB
    static void clear();

private:
    static uint16_t favs_[MAX_FAVS];
    static int n_;
    static void save();
};
