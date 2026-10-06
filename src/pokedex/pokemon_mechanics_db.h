#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstddef>

// ============================================================================
// PokemonMechanicsDatabase — lee archivos PKME v1/v2 desde SD (sin HTTP)
//
// Ubicación: <base>/mechanics/data/NNNN.bin
//   base = "/pokedex/pokemon/"
//
// Cabecera (20 bytes, little-endian):
//   magic[4]="PKME" | version u16 | pokemon_id u16 | payload_size u32 |
//   crc32 u32 | generation_mask u16 (bits 0..8 = gen I..IX) |
//   generation_count u8 | reserved u8
//
// Payload:
//   node_count u8
//   nodo: species_id u16 | introduced_generation u8 | parent_index i8 (-1=root)
//         | condition_count u8
//         condición: kind u8 | introduced_generation u8 | value u16 |
//                    text_len u8 | text_utf8
//   sección (generation_count de ellas, ascendentes):
//         generation u8 | version_group_code u8 | move_count u16
//         movimiento: level u8 (0=INICIO) | order u16 | move_id u16 |
//                     type_code u8 | flags u8 (bit0=nombre EN fallback) |
//                     name_len u8 | name_utf8
//   v2 agrega: power/accuracy/pp/category u8 | description_len u8 | description_utf8
//
// El firmware NO tiene un índice global: la existencia se comprueba por
// apertura del archivo y la validación de cabecera + CRC.
// ============================================================================

// Caps (la generación completa respeta estos límites en la muestra; si un
// binario los excede, se rechaza en vez de desbordar).
static constexpr uint8_t  MECH_MAX_NODES        = 16;
static constexpr uint8_t  MECH_MAX_SECTIONS     = 9;
static constexpr uint16_t MECH_MAX_MOVES_PER_GEN = 64;
static constexpr uint8_t  MECH_MAX_CONDITIONS   = 6;
static constexpr uint8_t  MECH_MAX_COND_TEXT    = 40;
static constexpr uint8_t  MECH_MAX_MOVE_NAME    = 40;
static constexpr uint8_t  MECH_MAX_MOVE_DESCRIPTION = 180;
static constexpr uint8_t  MECH_MAX_GEN          = 9;

struct MechCondition {
    uint8_t  kind;                 // 15=amistad+día, 16=amistad+noche
    uint8_t  introduced_generation;
    uint16_t value;                // nivel / id item / ...
    char     text[MECH_MAX_COND_TEXT + 1];
};

struct MechNode {
    uint16_t species_id;
    uint8_t  introduced_generation;
    int8_t   parent_index;         // -1 = raíz (preevolución más antigua)
    uint8_t  condition_count;
    MechCondition conditions[MECH_MAX_CONDITIONS];
};

struct MechMove {
    uint8_t  level;                // 0 = INICIO
    uint16_t order;
    uint16_t move_id;
    uint8_t  type_code;
    uint8_t  flags;                // bit0 = nombre EN (fallback)
    char     name[MECH_MAX_MOVE_NAME + 1];
    uint8_t  power;                // 255 = no aplica/desconocido
    uint8_t  accuracy;             // 255 = no aplica/desconocido
    uint8_t  pp;                   // 255 = desconocido
    uint8_t  category;             // 1=físico, 2=especial, 3=estado
    char     description[MECH_MAX_MOVE_DESCRIPTION + 1];
};

struct MechSection {
    uint8_t  generation;           // 1..9
    uint8_t  version_group_code;
    uint16_t move_count;
    MechMove moves[MECH_MAX_MOVES_PER_GEN];
};

struct MechanicsData {
    uint16_t id;
    uint16_t generation_mask;
    uint8_t  generation_count;
    uint8_t  node_count;
    MechNode nodes[MECH_MAX_NODES];
    uint8_t  section_count;
    MechSection sections[MECH_MAX_SECTIONS];
    bool     valid;
};

class PokemonMechanicsDatabase {
public:
    PokemonMechanicsDatabase();

    // Define la ruta base (p. ej. "/pokedex/pokemon/"). No carga nada.
    void begin(const char* base_path);

    // ¿Existe el archivo PKME de esta especie? (apertura + cabecera)
    bool has(uint16_t id) const;

    // Lee y valida el PKME de una especie bajo demanda.
    bool load(uint16_t id, MechanicsData* out) const;

    // ¿La especie tiene datos para la generación g (1..9)?
    bool hasGeneration(const MechanicsData* d, uint8_t g) const;

    // Índice (0..section_count-1) de la sección de la generación g, o -1.
    int sectionIndex(const MechanicsData* d, uint8_t g) const;

    // ¿El nodo species_id es un ancestro de target? (para destacar preevolución)
    bool isAncestor(const MechanicsData* d, uint16_t target_id, uint16_t ancestor_id) const;

private:
    char _data_path[64];   // "<base>mechanics/data/"
};
