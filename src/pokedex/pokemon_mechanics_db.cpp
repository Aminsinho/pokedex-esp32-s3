#include "pokemon_mechanics_db.h"
#include <SD_MMC.h>

// CRC32 (IEEE 802.3) — compatible con Python zlib.crc32
static uint32_t crc32_calc(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int)(crc & 1));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// Lectores little-endian sobre buffer (con límites)
struct RD {
    const uint8_t* p;
    size_t n;
    size_t off;
    bool bad;
    RD(const uint8_t* p, size_t n) : p(p), n(n), off(0), bad(false) {}
    bool need(size_t k) {
        if (bad) return false;
        if (off + k > n) { bad = true; return false; }
        return true;
    }
    uint8_t  u8()  { if (!need(1)) return 0;  return p[off++]; }
    uint16_t u16() { if (!need(2)) return 0;  uint16_t v = p[off] | (p[off+1] << 8); off += 2; return v; }
    int8_t   i8()  { if (!need(1)) return 0;  return (int8_t)p[off++]; }
    uint32_t u32() { if (!need(4)) return 0;
                     uint32_t v = (uint32_t)p[off] | ((uint32_t)p[off+1] << 8) |
                                  ((uint32_t)p[off+2] << 16) | ((uint32_t)p[off+3] << 24);
                     off += 4; return v; }
    void copy(char* dst, size_t cap) {   // copiate text_utf8 (max cap-1)
        if (cap == 0) return;
        size_t k = u8();
        if (k > cap - 1) k = cap - 1;
        if (off + k > n) { bad = true; dst[0] = 0; return; }
        memcpy(dst, p + off, k); off += k;
        dst[k] = '\0';
    }
};

PokemonMechanicsDatabase::PokemonMechanicsDatabase() { _data_path[0] = '\0'; }

void PokemonMechanicsDatabase::begin(const char* base_path) {
    snprintf(_data_path, sizeof(_data_path), "%smechanics/data/", base_path);
}

bool PokemonMechanicsDatabase::has(uint16_t id) const {
    if (_data_path[0] == '\0') return false;
    char path[80];
    snprintf(path, sizeof(path), "%s%04d.bin", _data_path, id);
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) return false;
    f.close();
    return true;
}

bool PokemonMechanicsDatabase::load(uint16_t id, MechanicsData* out) const {
    if (!out) return false;
    memset(out, 0, sizeof(MechanicsData));
    if (_data_path[0] == '\0') return false;

    char path[80];
    snprintf(path, sizeof(path), "%s%04d.bin", _data_path, id);
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) { Serial.printf("[MechDB] ERROR: no file %s\n", path); return false; }

    size_t file_size = f.size();
    if (file_size < 20) { Serial.printf("[MechDB] ERROR: too small %zu\n", file_size); f.close(); return false; }

    uint8_t* buf = (uint8_t*)ps_malloc(file_size + 1);
    if (!buf) { Serial.println("[MechDB] ERROR: PSRAM alloc fail"); f.close(); return false; }
    int rd = f.read(buf, file_size);
    f.close();
    if (rd != (int)file_size) { free(buf); return false; }

    // ---- Cabecera ----
    if (memcmp(buf, "PKME", 4) != 0) { Serial.println("[MechDB] ERROR: bad magic"); free(buf); return false; }
    const uint16_t version      = (uint16_t)(buf[4] | (buf[5] << 8));
    const uint16_t pid          = (uint16_t)(buf[6] | (buf[7] << 8));
    const uint32_t payload_size = (uint32_t)buf[8]  | ((uint32_t)buf[9] << 8) |
                                 ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
    const uint32_t stored_crc   = (uint32_t)buf[12] | ((uint32_t)buf[13] << 8) |
                                 ((uint32_t)buf[14] << 16) | ((uint32_t)buf[15] << 24);
    const uint16_t gen_mask     = (uint16_t)(buf[16] | (buf[17] << 8));
    const uint8_t  gen_count    = buf[18];

    if (version != 1 && version != 2) { Serial.printf("[MechDB] ERROR: version %u\n", version); free(buf); return false; }
    if (payload_size != file_size - 20) {
        Serial.printf("[MechDB] ERROR: payload %lu != %zu\n", (unsigned long)payload_size, file_size - 20);
        free(buf); return false;
    }
    if (gen_count > MECH_MAX_SECTIONS) { Serial.println("[MechDB] ERROR: gen_count > max"); free(buf); return false; }

    // ---- CRC del payload ----
    const uint8_t* payload = buf + 20;
    const uint32_t computed = crc32_calc(payload, payload_size);
    if (computed != stored_crc) {
        Serial.printf("[MechDB] ERROR: CRC %08lX != %08lX\n",
                      (unsigned long)computed, (unsigned long)stored_crc);
        free(buf); return false;
    }

    // ---- Parseo ----
    RD r(payload, payload_size);

    out->id = pid;
    out->generation_mask = gen_mask;
    out->generation_count = gen_count;

    // Nodos evolutivos
    out->node_count = r.u8();
    if (out->node_count > MECH_MAX_NODES) { r.bad = true; }
    for (uint8_t i = 0; i < out->node_count && !r.bad; i++) {
        MechNode& nd = out->nodes[i];
        nd.species_id = r.u16();
        nd.introduced_generation = r.u8();
        nd.parent_index = r.i8();
        nd.condition_count = r.u8();
        if (nd.condition_count > MECH_MAX_CONDITIONS) { r.bad = true; break; }
        for (uint8_t c = 0; c < nd.condition_count && !r.bad; c++) {
            MechCondition& cd = nd.conditions[c];
            cd.kind = r.u8();
            cd.introduced_generation = r.u8();
            cd.value = r.u16();
            r.copy(cd.text, sizeof(cd.text));
        }
    }

    // Secciones por generación
    out->section_count = gen_count;
    for (uint8_t s = 0; s < out->section_count && !r.bad; s++) {
        MechSection& sec = out->sections[s];
        sec.generation = r.u8();
        sec.version_group_code = r.u8();
        sec.move_count = r.u16();
        if (sec.move_count > MECH_MAX_MOVES_PER_GEN) { r.bad = true; break; }
        for (uint16_t m = 0; m < sec.move_count && !r.bad; m++) {
            MechMove& mv = sec.moves[m];
            mv.level = r.u8();
            mv.order = r.u16();
            mv.move_id = r.u16();
            mv.type_code = r.u8();
            mv.flags = r.u8();
            r.copy(mv.name, sizeof(mv.name));
            if (version >= 2) {
                mv.power = r.u8();
                mv.accuracy = r.u8();
                mv.pp = r.u8();
                mv.category = r.u8();
                r.copy(mv.description, sizeof(mv.description));
            } else {
                mv.power = mv.accuracy = mv.pp = 255;
                mv.category = 0;
                mv.description[0] = 0;
            }
        }
    }

    free(buf);
    if (r.bad) {
        Serial.println("[MechDB] ERROR: parse overflow");
        return false;
    }
    out->valid = true;
    Serial.printf("[MechDB] OK: #%-4u nodes=%u gens=%u mask=0x%04X\n",
                  pid, out->node_count, out->section_count, gen_mask);
    return true;
}

bool PokemonMechanicsDatabase::hasGeneration(const MechanicsData* d, uint8_t g) const {
    if (!d || !d->valid || g < 1 || g > MECH_MAX_GEN) return false;
    return (d->generation_mask & (1u << (g - 1))) != 0;
}

int PokemonMechanicsDatabase::sectionIndex(const MechanicsData* d, uint8_t g) const {
    if (!d || !d->valid) return -1;
    for (int i = 0; i < d->section_count; i++) {
        if (d->sections[i].generation == g) return i;
    }
    return -1;
}

bool PokemonMechanicsDatabase::isAncestor(const MechanicsData* d, uint16_t target_id, uint16_t ancestor_id) const {
    // Recorre padres desde target; si llega a ancestor_id, es ancestro.
    if (!d || !d->valid) return false;
    for (int step = 0; step < d->node_count; step++) {
        int cur = -1;
        for (int i = 0; i < d->node_count; i++) {
            if (d->nodes[i].species_id == target_id) { cur = i; break; }
        }
        if (cur < 0) return false;
        int8_t p = d->nodes[cur].parent_index;
        if (p < 0) return false;
        if (p >= d->node_count) return false;
        if (d->nodes[p].species_id == ancestor_id) return true;
        target_id = d->nodes[p].species_id;
    }
    return false;
}
