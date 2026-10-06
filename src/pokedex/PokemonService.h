#pragma once
#include <Arduino.h>
#include "Pokemon.h"
#include "local_pokemon_db.h"
#include "pokemon_mechanics_db.h"

/* ============================================================
 * PokemonService — UI-side data access layer (core 0).
 *
 * SD-FIRST architecture:
 *   - List, Search, Detail → LocalPokemonDatabase (SD, no HTTP)
 *   - Scan/IA → still uses NetworkTask (backend)
 *
 * The local DB index is loaded into PSRAM at boot.
 * Detail files are read on-demand from SD.
 * ============================================================ */

enum class BackendState { UNKNOWN, ONLINE, OFFLINE };

class PokemonService {
public:
    static const int MAX_LIST = 200;  // max search results buffer

    /* --- local DB binding --- */
    static LocalPokemonDatabase* localDb() { return _db; }
    static void setLocalDb(LocalPokemonDatabase* db) { _db = db; }

    /* --- mechanics DB binding (PKME: evolucion + ataques por generación) --- */
    static PokemonMechanicsDatabase* mechDb() { return _mech; }
    static void setMechDb(PokemonMechanicsDatabase* db) { _mech = db; }
    static bool hasMech(uint16_t id) { return _mech && _mech->has(id); }
    static bool loadMech(uint16_t id, MechanicsData& out) { return _mech && _mech->load(id, &out); }

    /* --- backend state (for scan/IA) --- */
    static BackendState backend() { return backend_; }
    static void setBackend(BackendState s) { backend_ = s; }

    /* --- list (from local DB index) --- */
    static bool hasList() { return _db && _db->isReady(); }
    static int listCount() { return _db ? _db->count() : 0; }

    // Backward-compatible: returns a const ref (uses internal static buffer)
    static const Pokemon& listItem(int i);

    // Paginated access (for scrollable lists)
    static int getPage(int offset, int limit, Pokemon* out, int max_out);

    // Backward-compatible setters (no-op in local mode)
    static void setList(const Pokemon* items, int n) { (void)items; (void)n; }
    static void setDetail(const Pokemon& p) { (void)p; }

    /* --- detail (from local DB data files) --- */
    static bool getDetail(uint16_t id, Pokemon& out);

    /* --- search (local, synchronous) --- */
    static void requestSearch(const String& query) {
        // Synchronous: searches local DB immediately
        String q = query;
        searchResults_ = _db ? _db->search(q.c_str(), _searchBuf, MAX_LIST) : 0;
        searchOk_ = searchResults_ > 0;
    }
    static bool searchFound() { return searchOk_; }
    static int searchCount() { return searchResults_; }
    static const Pokemon& searchResult(int i = 0);

    /* --- compatibility: old request methods (now no-ops) --- */
    static void requestList() { /* no-op: list is always available from local DB */ }
    static void requestDetail(uint16_t id) { /* no-op: detail is read synchronously */ }
    static void invalidateList() { /* no-op */ }

private:
    static LocalPokemonDatabase* _db;
    static PokemonMechanicsDatabase* _mech;
    static BackendState backend_;
    static int searchResults_;
    static bool searchOk_;
    static LocalIndexEntry _searchBuf[MAX_LIST];

    static void _indexToPokemon(const LocalIndexEntry& entry, Pokemon& out);
    static const char* _typeCodeToName(uint8_t code);
};
