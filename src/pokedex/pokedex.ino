/* ============================================================
 * POKEDEX — M2: DISPLAY ↔ BACKEND
 *
 * Architecture (already implemented in src/pokedex/):
 *   UI (LVGL, core 0)  <->  PokemonService / ScanService
 *   PokemonService      ->  NetworkTask::post(NetCmd)   [cmd queue]
 *   NetworkTask (core 1) -> BackendClient -> HTTP
 *   NetworkTask         ->  res queue -> bridge below
 *   bridge (core 0)     ->  services + screens
 *
 * This file only wires everything together:
 *   setup(): hardware -> LVGL -> touch -> Wi-Fi -> NetworkTask -> screens -> BOOT
 *   loop():  LVGL tick + WifiManager::loop() + result bridge + ScanService::tick()
 *            + periodic backend health check.
 * ============================================================ */

#include <TFT_eSPI.h>
#include <Wire.h>
#include "lvgl.h"
#include "lvgl_port.h"
#include "Touch.h"
#include "ScreenManager.h"
#include "storage_manager.h"
#include "local_pokemon_db.h"
#include "pokemon_mechanics_db.h"
#include "WifiManager.h"
#include "NetworkTask.h"
#include "PokemonService.h"
#include "ScanService.h"
#include "HomeScreen.h"
#include "FavoritesManager.h"
#include "TouchDiagnostics.h"
#include "PokedexScreen.h"
#include "PokemonDetailScreen.h"
#include "ScanScreen.h"
#include "SpriteProvision.h"
#include "app_config.h"
#include "AudioManager.h"
#include "DataProvision.h"

TFT_eSPI tft;

static uint32_t lastHealthCheck = 0;
static bool wasBackendOnline = false;

/* ---------- helpers (core 0 / LVGL context) ---------- */

static ScreenId cur() {
    return ScreenManager::getInstance().current();
}

static void scanSetState(const char* text, uint32_t color) {
    if (cur() == ScreenId::SCAN) ScanScreen::setState(text, color);
}

static void homeSetBackendDot() {
    if (cur() != ScreenId::HOME) return;
    int s;
    if (!WifiManager::connected()) s = 1;                              // connecting
    else if (PokemonService::backend() == BackendState::ONLINE) s = 2; // online
    else if (PokemonService::backend() == BackendState::UNKNOWN) s = 1;// connecting
    else s = 0;                                                        // offline
    HomeScreen::setBackend(s);
}

/* ---------- bridge: NetworkTask results -> services/UI ---------- */

static void handleNetworkResult(const NetResult& r) {
    switch (r.evt) {

        case NetEvt::STATUS: {
            bool online = r.ok;
            PokemonService::setBackend(online ? BackendState::ONLINE : BackendState::OFFLINE);
            homeSetBackendDot();
            if (online && !wasBackendOnline) {
                Serial.println("[BRIDGE] backend ONLINE — registering display + camera check");
                NetworkTask::post(NetCmd::REGISTER);
                NetworkTask::post(NetCmd::CHECK_CAMERA);
            } else if (!online && wasBackendOnline) {
                Serial.println("[BRIDGE] backend OFFLINE");
            }
            wasBackendOnline = online;
            break;
        }

        case NetEvt::LIST: {
            if (r.ok) {
                static Pokemon buf[NetworkTask::MAX_LIST];
                int n = 0;
                if (NetworkTask::copyList(buf, &n) && n > 0) {
                    PokemonService::setList(buf, n);
                    if (cur() == ScreenId::POKEDX_LIST) PokedexScreen::renderList();
                }
            } else {
                PokemonService::invalidateList();
                if (cur() == ScreenId::POKEDX_LIST) PokedexScreen::showOffline();
            }
            break;
        }

        case NetEvt::SEARCH: {
            if (cur() == ScreenId::POKEDX_LIST) {
                if (r.ok) {
                    PokedexScreen::updateSearchResult(r.name.c_str(), true);
                } else {
                    PokedexScreen::updateSearchResult(nullptr, false);
                }
            }
            break;
        }

        case NetEvt::DETAIL: {
            if (r.ok) {
                Pokemon p;
                if (NetworkTask::copyDetail(p)) {
                    PokemonService::setDetail(p);
                    if (cur() == ScreenId::POKEMON_DETAIL) {
                        PokemonDetailScreen::render(p);
                    }
                }
            } else if (cur() == ScreenId::POKEMON_DETAIL) {
                PokemonDetailScreen::showError();
            }
            break;
        }

        case NetEvt::CAMERA:
            if (cur() == ScreenId::SCAN) ScanScreen::setCamera(r.ok);
            break;

        case NetEvt::REGISTERED:
            Serial.printf("[BRIDGE] display registered ok=%d\n", (int)r.ok);
            break;

        case NetEvt::SCAN_CREATED:
            if (cur() != ScreenId::SCAN || ScanService::phase() != ScanPhase::STARTING) break;
            if (r.ok) {
                ScanService::onCreated(r.data);
                scanSetState("WAITING FOR CAMERA...", 0xE8B93E);
            } else {
                ScanService::onError(r.data);
                scanSetState("SCAN FAILED", 0xE8B93E);
            }
            break;

        case NetEvt::SCAN_STATUS: {
            if (cur() != ScreenId::SCAN || !ScanService::isActive() ||
                r.scanId != ScanService::scanId()) break;
            if (!r.ok) {
                ScanService::onError(r.data.length() ? r.data : "network error");
            } else {
                ScanService::onStatus(r.data, r.pokemonId, r.confidence);
            }
            if (cur() != ScreenId::SCAN) break;

            /* safe defaults — never setState() with uninitialised pointers */
            const char* text = nullptr;
            uint32_t color = 0;
            switch (ScanService::phase()) {
                case ScanPhase::WAITING_CAMERA:
                    text = "WAITING FOR CAMERA..."; color = 0xE8B93E; break;
                case ScanPhase::CAPTURING:
                    text = "CAPTURING..."; color = 0xE8B93E; break;
                case ScanPhase::UPLOADING:
                    text = "UPLOADING..."; color = 0xE8B93E; break;
                case ScanPhase::PROCESSING:
                    text = "ANALYZING..."; color = 0xE8B93E; break;
                case ScanPhase::MATCH:
                    /* direct showMatch — do NOT fall through to setState */
                    if (r.name.length() > 0)
                        ScanScreen::showMatch(r.name.c_str(), r.confidence);
                    break;
                case ScanPhase::ERROR:
                    text = (r.name == "pokemon_not_recognized" || r.name == "pokemon_identity_mismatch")
                        ? "NOT RECOGNIZED - RETRY" : "SCAN FAILED - RETRY";
                    color = 0xE8B93E; break;
                default:
                    break;
            }
            if (text) ScanScreen::setState(text, color);
            break;
        }

        case NetEvt::ERROR:
            if (ScanService::isActive()) {
                ScanService::onError(r.data);
                scanSetState("SCAN FAILED", 0xE8B93E);
            }
            break;
    }
}


/* ---------- setup / loop ---------- */

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("=== POKEDEX UI ===");

    // Display
    tft.init();
    tft.setRotation(1); // landscape 320x240
    tft.fillScreen(TFT_BLACK);
    Serial.println("[OK] LCD");

    // Touch
    if (Touch::init()) {
        Serial.println("[OK] TOUCH");
    } else {
        Serial.println("[ERR] TOUCH");
    }

    // Audio (ES8311 + I2S) — must be after Touch (shared I2C bus)
    if (AudioManager::begin()) {
        AudioManager::startTask(0);
        Serial.println(AudioManager::isReady() ? "[OK] AUDIO" : "[ERR] AUDIO task");
    } else {
        Serial.println("[ERR] AUDIO init (I2S, codec or queue)");
    }

    // LVGL
    lvgl_port_init();
    Serial.println("[OK] LVGL");

    // Screens
    ScreenManager::getInstance().init();
    Serial.println("[OK] SCREENS");

#if TOUCH_DIAGNOSTICS
    TouchDiagnostics::init();
    Serial.println("[OK] TOUCH DIAGNOSTICS");
#endif

    // SD card (SDMMC)
    storageMgr.begin();

    // Local Pokemon Database (SD-first, no HTTP for list/search/detail)
    static LocalPokemonDatabase localDb;
    if (localDb.begin("/pokedex/pokemon/")) {
        PokemonService::setLocalDb(&localDb);
        Serial.printf("[OK] LOCAL DB: %u species\n", localDb.count());
    } else {
        Serial.println("[ERR] LOCAL DB failed — list/search/detail unavailable");
    }

    // Mechanics Database (PKME: evolucion + ataques por generación) — bajo demanda
    static PokemonMechanicsDatabase mechDb;
    mechDb.begin("/pokedex/pokemon/");
    PokemonService::setMechDb(&mechDb);
    Serial.println("[OK] MECH DB: ready (PKME, on-demand)");

    // Favorites
    FavoritesMgr::begin();
    uint16_t fav = FavoritesMgr::get(0);
    if (fav == 0) fav = 25;  // default Pikachu
    HomeScreen::setFavorite(fav);

    // Wi-Fi (STA, non-blocking — WifiManager::loop() tracks state)
    WifiManager::begin();

    // Network task (core 1) — only for scan/IA, not for list/detail
    NetworkTask::begin();
    NetworkTask::post(NetCmd::CHECK_STATUS);

    // Short asynchronous hardware test; never stall UI startup.
    if (AudioManager::isReady()) {
        Serial.println("[AUDIO] BOOT BEEP TEST: 1kHz, 200ms");
        AudioManager::bootBeepTest();
    }

    // Boot screen (auto -> HOME after 1.5 s)
    ScreenManager::getInstance().show(ScreenId::BOOT);
    Serial.println("=== BOOT ===");
}

void loop() {
    uint32_t loop_start = millis();

    // Explicit local USB visual QA. No camera capture or network operation.
    static char uiCommand[32];
    static uint8_t uiLength = 0;
    for (int n = 0; n < 32 && Serial.available(); ++n) {
        const char c = Serial.read();
        if (c == '\n') {
            uiCommand[uiLength] = 0;
            if (!strcmp(uiCommand, "@SNAP")) lvgl_port_capture();
            else if (!strcmp(uiCommand, "@BEEP")) {
                Serial.printf("[AUDIO] test ready=%u\n", AudioManager::isReady());
                AudioManager::bootBeepTest();
            }
            else if (!strncmp(uiCommand, "@CRY ", 5)) AudioManager::playCry(atoi(uiCommand + 5));
            else if (!strncmp(uiCommand, "@NARRATE ", 9)) AudioManager::playNarration(atoi(uiCommand + 9));
            else if (!strncmp(uiCommand, "@DATA ", 6) || !strncmp(uiCommand, "@NARR ", 6) ||
                     !strncmp(uiCommand, "@MECH ", 6) || !strncmp(uiCommand, "@STONE ", 7)) provisionDataFile(uiCommand);
            else if (!strcmp(uiCommand, "@HOME")) {
                HomeScreen::setFavorite(HomeScreen::getFavorite());
                ScreenManager::getInstance().show(ScreenId::HOME);
            }
            else if (!strncmp(uiCommand, "@SPRITE ", 8)) provisionSprite(uiCommand);
            else if (!strcmp(uiCommand, "@LIST")) ScreenManager::getInstance().show(ScreenId::POKEDX_LIST);
            else if (!strcmp(uiCommand, "@SCAN")) ScreenManager::getInstance().show(ScreenId::SCAN);
            else if (!strncmp(uiCommand, "@DETAIL ", 8)) {
                const int id = atoi(uiCommand + 8);
                if (id > 0 && id <= 1025) ScreenManager::getInstance().showDetail(id);
            }
            else if (!strncmp(uiCommand, "@TAB ", 5)) {
                PokemonDetailScreen::selectTab(atoi(uiCommand + 5));   // QA: 0..3
            }
            else if (!strncmp(uiCommand, "@GEN ", 5)) {
                PokemonDetailScreen::selectGen(atoi(uiCommand + 5));   // QA: índice de generación
            }
            else if (!strncmp(uiCommand, "@SCROLL ", 8)) {
                PokemonDetailScreen::scrollMoves(atoi(uiCommand + 8)); // QA: desplaza lista ATAQUES
            }
            else if (!strncmp(uiCommand, "@MOVE ", 6)) {
                PokemonDetailScreen::selectMove(atoi(uiCommand + 6));  // QA: abre ficha de ataque
            }
            else if (!strncmp(uiCommand, "@EVOX ", 6)) {
                PokemonDetailScreen::scrollEvo(atoi(uiCommand + 6));   // QA: scroll de ramas
            }
            uiLength = 0;
        } else if (c != '\r' && uiLength < sizeof(uiCommand) - 1) uiCommand[uiLength++] = c;
    }

    lvgl_port_task();   // LVGL tick (non-blocking)
    uint32_t t_lvgl = millis();

    // Wi-Fi state tracking (1 Hz)
    WifiManager::loop();
    uint32_t t_wifi = millis();

    // Bridge: drain network results in UI context
    NetResult r;
    for (int processed = 0; processed < 4 && NetworkTask::pop(r); ++processed) {
        handleNetworkResult(r);
    }
    uint32_t t_net = millis();

    // Scan state machine (polling, MATCH delay, SHOWING handover)
    const ScanPhase beforeTick = ScanService::phase();
    ScanService::tick();
    if (beforeTick != ScanPhase::ERROR && ScanService::phase() == ScanPhase::ERROR)
        scanSetState("SCAN TIMEOUT - RETRY", 0xE8B93E);
    if (cur() == ScreenId::SCAN && ScanService::phase() == ScanPhase::SHOWING) {
        const uint16_t id = ScanService::matchedId();
        ScanService::cancel();
        ScreenManager::getInstance().showDetail(id);
        AudioManager::playNarration(id);
    }
    uint32_t t_scan = millis();

    // Home screen animation + sprite load
    if (cur() == ScreenId::HOME) HomeScreen::update();
    uint32_t t_home = millis();

    // Periodic backend health check
    uint32_t now = millis();
    uint32_t period = (PokemonService::backend() == BackendState::ONLINE)
                        ? BACKEND_CHECK_ONLINE_MS
                        : BACKEND_CHECK_MS;
    if (now - lastHealthCheck >= period) {
        lastHealthCheck = now;
        NetworkTask::post(NetCmd::CHECK_STATUS);
    }

    uint32_t total = millis() - loop_start;
    if (total > 500) {
        Serial.printf("[LOOP] SLOW: total=%lu ms (lvgl=%lu wifi=%lu net=%lu scan=%lu home=%lu)\n",
            total, t_lvgl - loop_start, t_wifi - t_lvgl, t_net - t_wifi, t_scan - t_net, t_home - t_scan);
    }

    delay(1);
}
