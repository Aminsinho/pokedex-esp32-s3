#include "FavoritesManager.h"
#include <Preferences.h>

Preferences prefs;

uint16_t FavoritesMgr::favs_[MAX_FAVS] = {0};
int FavoritesMgr::n_ = 0;

void FavoritesMgr::begin() {
    prefs.begin("favs", false);  // read-only namespace
    n_ = prefs.getInt("count", 0);
    if (n_ > MAX_FAVS) n_ = MAX_FAVS;
    for (int i = 0; i < n_; i++) {
        char key[4];
        snprintf(key, sizeof(key), "f%d", i);
        favs_[i] = (uint16_t)prefs.getUShort(key);
    }
    prefs.end();
    Serial.printf("[FAVS] loaded %d favorites\n", n_);
}

void FavoritesMgr::save() {
    Preferences w;
    w.begin("favs", true);  // read-write
    w.putInt("count", n_);
    for (int i = 0; i < n_; i++) {
        char key[4];
        snprintf(key, sizeof(key), "f%d", i);
        w.putUShort(key, favs_[i]);
    }
    w.end();
}

void FavoritesMgr::toggle(uint16_t id) {
    for (int i = 0; i < n_; i++) {
        if (favs_[i] == id) {
            // Remove: shift left
            for (int j = i; j < n_ - 1; j++) favs_[j] = favs_[j + 1];
            n_--;
            save();
            Serial.printf("[FAVS] removed #%d (now %d)\n", id, n_);
            return;
        }
    }
    // Add
    if (n_ >= MAX_FAVS) {
        Serial.println("[FAVS] max reached, cannot add");
        return;
    }
    favs_[n_++] = id;
    save();
    Serial.printf("[FAVS] added #%d (now %d)\n", id, n_);
}

bool FavoritesMgr::isFavorite(uint16_t id) {
    for (int i = 0; i < n_; i++)
        if (favs_[i] == id) return true;
    return false;
}

int FavoritesMgr::count() { return n_; }

uint16_t FavoritesMgr::get(int index) {
    if (index < 0 || index >= n_) return 0;
    return favs_[index];
}

void FavoritesMgr::clear() {
    n_ = 0;
    save();
    Serial.println("[FAVS] cleared all");
}
