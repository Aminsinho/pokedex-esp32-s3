#pragma once
#include <Arduino.h>

/* Pokemon data model (M2: String-based, backend-sourced).
 * Data is fetched over HTTP by the NetworkTask and cached by
 * PokemonService — there is no longer any compile-time mock data.
 *
 * NOTE: fields are `String` (heap-backed). Copying a Pokemon is cheap
 * (COW headers), but avoid unnecessary copies in tight loops.
 */
struct TypeBadge {
    String name;
    uint32_t color;
};

struct Pokemon {
    uint16_t id;
    String name;
    TypeBadge types[3];
    uint8_t type_count;
    float height_m;
    float weight_kg;
    uint8_t base_experience;
    uint8_t generation;
    struct {
        uint8_t hp;
        uint8_t attack;
        uint8_t defense;
        uint8_t special_attack;
        uint8_t special_defense;
        uint8_t speed;
    } stats;
    String description;
};

/* Official-ish type colors for the badge UI */
inline uint32_t typeColor(const String& t) {
    if (t == "fire")     return 0xF08030;
    if (t == "water")    return 0x3A6FD8;
    if (t == "grass")    return 0x4CAF50;
    if (t == "poison")   return 0x9C59B6;
    if (t == "electric") return 0xF2C744;
    if (t == "flying")   return 0x7F8FC9;
    if (t == "rock")     return 0xB5A642;
    if (t == "bug")      return 0xA8B545;
    if (t == "normal")   return 0xA8A878;
    if (t == "fighting") return 0xC0504D;
    if (t == "ghost")    return 0x6A4C93;
    if (t == "dragon")   return 0x6A5ACD;
    if (t == "psychic")  return 0xEF55B2;
    if (t == "ice")      return 0x6ACCD9;
    if (t == "dark")     return 0x5A5A5A;
    if (t == "steel")    return 0x8C8C9C;
    if (t == "fairy")    return 0xEE9AC4;
    return 0x8C8C8C;
}
