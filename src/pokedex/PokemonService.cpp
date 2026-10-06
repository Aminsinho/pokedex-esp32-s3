#include "PokemonService.h"
#include <cstring>

// Static members
LocalPokemonDatabase* PokemonService::_db = nullptr;
PokemonMechanicsDatabase* PokemonService::_mech = nullptr;
BackendState PokemonService::backend_ = BackendState::UNKNOWN;
int PokemonService::searchResults_ = 0;
bool PokemonService::searchOk_ = false;
LocalIndexEntry PokemonService::_searchBuf[MAX_LIST];

// Type code → name mapping (1-18)
static const char* TYPE_NAMES[] = {
    "",            // 0 = none
    "normal",      // 1
    "fire",        // 2
    "water",       // 3
    "electric",    // 4 (matches tools/generate_local_db.py TYPE_MAP)
    "grass",       // 5
    "ice",         // 6
    "fighting",    // 7
    "poison",      // 8
    "ground",      // 9
    "flying",      // 10
    "psychic",     // 11
    "bug",         // 12
    "rock",        // 13
    "ghost",       // 14
    "dragon",      // 15
    "dark",        // 16
    "steel",       // 17
    "fairy"        // 18
};

const char* PokemonService::_typeCodeToName(uint8_t code) {
    if (code >= 1 && code <= 18) return TYPE_NAMES[code];
    return "";
}

void PokemonService::_indexToPokemon(const LocalIndexEntry& entry, Pokemon& out) {
    out.id = entry.id;
    out.name = String(entry.name);
    out.type_count = 0;

    if (entry.type1 > 0) {
        out.types[out.type_count].name = _typeCodeToName(entry.type1);
        out.types[out.type_count].color = typeColor(out.types[out.type_count].name);
        out.type_count++;
    }
    if (entry.type2 > 0) {
        out.types[out.type_count].name = _typeCodeToName(entry.type2);
        out.types[out.type_count].color = typeColor(out.types[out.type_count].name);
        out.type_count++;
    }

    out.height_m = entry.height_cm / 100.0f;
    out.weight_kg = entry.weight_dkg / 10.0f;
    out.base_experience = entry.base_exp;
    out.generation = entry.generation;

    // Stats not in index — zero-filled (use getDetail for full stats)
    out.stats.hp = 0;
    out.stats.attack = 0;
    out.stats.defense = 0;
    out.stats.special_attack = 0;
    out.stats.special_defense = 0;
    out.stats.speed = 0;
    out.description = "";
}

const Pokemon& PokemonService::listItem(int i) {
    static Pokemon tmp;
    if (!_db || !_db->isReady()) return tmp;
    if (i < 0 || i >= _db->count()) return tmp;
    const LocalIndexEntry* entry = _db->getEntry(i);
    if (!entry) return tmp;
    _indexToPokemon(*entry, tmp);
    return tmp;
}

int PokemonService::getPage(int offset, int limit, Pokemon* out, int max_out) {
    if (!_db || !_db->isReady()) return 0;

    // Get entries from local DB
    LocalIndexEntry buf[max_out > MAX_LIST ? MAX_LIST : max_out];
    int n = _db->getPage(offset, limit, buf, max_out);

    for (int i = 0; i < n; i++) {
        _indexToPokemon(buf[i], out[i]);
    }
    return n;
}

bool PokemonService::getDetail(uint16_t id, Pokemon& out) {
    if (!_db || !_db->isReady()) return false;

    LocalPokemonDetail detail;
    if (!_db->getById(id, &detail)) return false;

    // Convert to Pokemon struct
    out.id = detail.id;
    out.name = String(detail.name);
    out.type_count = 0;

    if (detail.type1 > 0) {
        out.types[out.type_count].name = _typeCodeToName(detail.type1);
        out.types[out.type_count].color = typeColor(out.types[out.type_count].name);
        out.type_count++;
    }
    if (detail.type2 > 0) {
        out.types[out.type_count].name = _typeCodeToName(detail.type2);
        out.types[out.type_count].color = typeColor(out.types[out.type_count].name);
        out.type_count++;
    }

    out.height_m = detail.height_cm / 100.0f;
    out.weight_kg = detail.weight_dkg / 10.0f;
    out.base_experience = detail.base_exp > 255 ? 255 : detail.base_exp;
    out.generation = detail.generation;

    out.stats.hp = detail.hp;
    out.stats.attack = detail.attack;
    out.stats.defense = detail.defense;
    out.stats.special_attack = detail.sp_attack;
    out.stats.special_defense = detail.sp_defense;
    out.stats.speed = detail.speed;

    // Prefer Spanish description if available
    if (detail.desc_es.length() > 0) {
        out.description = detail.desc_es;
    } else {
        out.description = detail.desc_en;
    }

    return true;
}

const Pokemon& PokemonService::searchResult(int i) {
    static Pokemon dummy;  // fallback
    if (i < 0 || i >= searchResults_) return dummy;
    // Convert on-the-fly (cached in a static to return reference)
    static LocalIndexEntry tmp_entry;
    tmp_entry = _searchBuf[i];
    static Pokemon tmp_pokemon;
    _indexToPokemon(tmp_entry, tmp_pokemon);
    return tmp_pokemon;
}
