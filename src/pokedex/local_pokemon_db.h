#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstddef>

// ============================================================================
// LocalPokemonDatabase — lee el dataset binario desde SD (sin HTTP)
//
// Formato pokemon_index.bin (cargado en PSRAM):
//   Header (12 bytes): magic[4]="PKDX", version u16, count u16, crc32 u32
//   Entry (26 bytes):  id u16, name[16], type1 u8, type2 u8, gen u8,
//                      height_cm u16, weight_dkg u16, base_exp u8
//
// Formato data/NNNN.bin (leído bajo demanda):
//   Header (8): magic[4]="PKDP", version u16, id u16
//   Body: name[20], name_es[20], type1 u8, type2 u8, gen u8,
//         height_cm u16, weight_dkg u16, base_exp u16,
//         hp u8, atk u8, def u8, spa u8, spd u8, spe u8,
//         desc_en_len u8, desc_en[var], desc_es_len u8, desc_es[var]
// ============================================================================

// Entry en PSRAM (26 bytes, fixed-size)
struct __attribute__((packed)) LocalIndexEntry {
    uint16_t id;
    char     name[16];   // display name, null-terminated
    uint8_t  type1;      // 1-18 or 0
    uint8_t  type2;      // 1-18 or 0
    uint8_t  generation; // 1-9
    uint16_t height_cm;  // cm
    uint16_t weight_dkg; // 0.1 kg
    uint8_t  base_exp;   // 0-255
};
static_assert(sizeof(LocalIndexEntry) == 26, "LocalIndexEntry must be 26 bytes");

// Detalle completo (leído desde SD bajo demanda)
struct LocalPokemonDetail {
    uint16_t id;
    char     name[21];    // max 20 chars + null
    char     name_es[21];
    uint8_t  type1;
    uint8_t  type2;
    uint8_t  generation;
    uint16_t height_cm;
    uint16_t weight_dkg;
    uint16_t base_exp;
    uint8_t  hp, attack, defense, sp_attack, sp_defense, speed;
    String   desc_en;
    String   desc_es;
    bool     valid;       // false si no se pudo leer
};

class LocalPokemonDatabase {
public:
    LocalPokemonDatabase();

    // Carga el índice desde SD en PSRAM.
    // paths: "/pokedex/pokemon/"
    // Returns true si el índice se cargó correctamente.
    bool begin(const char* base_path);

    // ¿El índice está cargado?
    bool isReady() const { return _ready; }

    // Número total de especies
    uint16_t count() const { return _count; }

    // ¿Existe un ID?
    bool exists(uint16_t id) const;

    // Obtiene el detalle completo desde data/NNNN.bin
    // Si out es nullptr, solo verifica existencia.
    bool getById(uint16_t id, LocalPokemonDetail* out) const;

    // Busca por nombre (case-insensitive, substring match)
    // Devuelve el número de resultados y llena results[] (max MAX_SEARCH)
    static constexpr int MAX_SEARCH = 20;
    int search(const char* query, LocalIndexEntry* results, int max_results) const;

    // Obtiene una página de resultados (offset, limit)
    int getPage(uint16_t offset, uint16_t limit, LocalIndexEntry* results, int max_results) const;

    // Obtiene un entry del índice directamente
    const LocalIndexEntry* getEntry(uint16_t index) const;

    // Total bytes del índice en PSRAM
    size_t memoryUsage() const { return _index_bytes; }

private:
    LocalIndexEntry* _entries;   // array en PSRAM
    uint16_t         _count;
    bool             _ready;
    size_t           _index_bytes;
    char             _data_path[64];  // "/pokedex/pokemon/data/"

    bool _loadIndex(const char* base_path);
    bool _readDetailFile(uint16_t id, LocalPokemonDetail* out) const;
    static bool _nameStartsWith(const char* haystack, const char* needle);
};
