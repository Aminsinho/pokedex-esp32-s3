#include "local_pokemon_db.h"
#include <SD_MMC.h>

// CRC32 (IEEE 802.3) — compatible with Python zlib.crc32
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (uint32_t)-(int)(crc & 1));
        }
    }
    return crc;
}
static uint32_t crc32_calc(const uint8_t* data, size_t len) {
    return crc32_update(0xFFFFFFFF, data, len) ^ 0xFFFFFFFF;
}

// ============================================================================
// LocalPokemonDatabase Implementation
// ============================================================================

LocalPokemonDatabase::LocalPokemonDatabase()
    : _entries(nullptr)
    , _count(0)
    , _ready(false)
    , _index_bytes(0)
{
    _data_path[0] = '\0';
}

bool LocalPokemonDatabase::begin(const char* base_path) {
    return _loadIndex(base_path);
}

bool LocalPokemonDatabase::_loadIndex(const char* base_path) {
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%spokemon_index.bin", base_path);

    File f = SD_MMC.open(index_path, FILE_READ);
    if (!f) {
        Serial.printf("[LocalDB] ERROR: cannot open %s\n", index_path);
        return false;
    }

    size_t file_size = f.size();
    if (file_size < 12) {
        Serial.printf("[LocalDB] ERROR: file too small (%zu bytes)\n", file_size);
        f.close();
        return false;
    }

    // Read header
    uint8_t header[12];
    if (f.read(header, 12) != 12) {
        Serial.println("[LocalDB] ERROR: failed to read header");
        f.close();
        return false;
    }

    // Validate magic
    if (memcmp(header, "PKDX", 4) != 0) {
        Serial.println("[LocalDB] ERROR: bad magic (expected PKDX)");
        f.close();
        return false;
    }

    uint16_t version = header[4] | (header[5] << 8);
    if (version != 1) {
        Serial.printf("[LocalDB] ERROR: unsupported version %u\n", version);
        f.close();
        return false;
    }

    uint16_t count = header[6] | (header[7] << 8);
    uint32_t stored_crc = header[8] | (header[9] << 8) | (header[10] << 16) | (header[11] << 24);

    size_t expected_size = 12 + (size_t)count * sizeof(LocalIndexEntry);
    if (file_size != expected_size) {
        Serial.printf("[LocalDB] ERROR: size mismatch (expected %zu, got %zu)\n",
                      expected_size, file_size);
        f.close();
        return false;
    }

    // Allocate in PSRAM
    _entries = (LocalIndexEntry*)ps_malloc(expected_size - 12);
    if (!_entries) {
        Serial.printf("[LocalDB] ERROR: PSRAM alloc failed (%zu bytes)\n", expected_size - 12);
        f.close();
        return false;
    }

    // Read entries
    int bytes_read = f.read((uint8_t*)_entries, expected_size - 12);
    f.close();

    if (bytes_read != (int)(expected_size - 12)) {
        Serial.printf("[LocalDB] ERROR: read failed (%d/%zu)\n", bytes_read, expected_size - 12);
        free(_entries);
        _entries = nullptr;
        return false;
    }

    // Verify CRC32
    uint32_t computed_crc = crc32_calc((const uint8_t*)_entries, expected_size - 12);
    if (computed_crc != stored_crc) {
        Serial.printf("[LocalDB] ERROR: CRC mismatch (stored=%08lX, computed=%08lX)\n",
                      stored_crc, computed_crc);
        free(_entries);
        _entries = nullptr;
        return false;
    }

    _count = count;
    _ready = true;
    _index_bytes = expected_size;

    // Build data path
    snprintf(_data_path, sizeof(_data_path), "%sdata/", base_path);

    Serial.printf("[LocalDB] OK: %u species loaded, %zu bytes in PSRAM\n", _count, _index_bytes);
    return true;
}

bool LocalPokemonDatabase::exists(uint16_t id) const {
    if (!_ready) return false;
    // Linear search (fast enough for <2000 entries)
    for (uint16_t i = 0; i < _count; i++) {
        if (_entries[i].id == id) return true;
    }
    return false;
}

const LocalIndexEntry* LocalPokemonDatabase::getEntry(uint16_t index) const {
    if (!_ready || index >= _count) return nullptr;
    return &_entries[index];
}

int LocalPokemonDatabase::getPage(uint16_t offset, uint16_t limit,
                                   LocalIndexEntry* results, int max_results) const {
    if (!_ready) return 0;
    if (offset >= _count) return 0;

    int n = 0;
    for (uint16_t i = offset; i < _count && n < max_results; i++) {
        if (limit > 0 && n >= limit) break;
        results[n] = _entries[i];
        n++;
    }
    return n;
}

int LocalPokemonDatabase::search(const char* query, LocalIndexEntry* results, int max_results) const {
    if (!_ready || !query || query[0] == '\0') return 0;

    // Convert to lowercase for comparison
    char q[16];
    int qlen = 0;
    for (int i = 0; query[i] && qlen < 15; i++) {
        q[qlen++] = (query[i] >= 'A' && query[i] <= 'Z') ? query[i] + 32 : query[i];
    }
    q[qlen] = '\0';

    int n = 0;
    for (uint16_t i = 0; i < _count && n < max_results; i++) {
        // Case-insensitive substring match
        const char* name = _entries[i].name;
        bool found = false;
        for (int j = 0; name[j]; j++) {
            bool match = true;
            for (int k = 0; q[k]; k++) {
                char c = name[j + k];
                if (c >= 'A' && c <= 'Z') c += 32;
                if (c != q[k]) { match = false; break; }
            }
            if (match) { found = true; break; }
        }
        if (found) {
            results[n++] = _entries[i];
        }
    }
    return n;
}

bool LocalPokemonDatabase::getById(uint16_t id, LocalPokemonDetail* out) const {
    if (!_ready) return false;
    if (!out) return exists(id);
    return _readDetailFile(id, out);
}

bool LocalPokemonDatabase::_readDetailFile(uint16_t id, LocalPokemonDetail* out) const {
    char path[64];
    snprintf(path, sizeof(path), "%s%04d.bin", _data_path, id);
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) return false;

    memset(out, 0, sizeof(LocalPokemonDetail));

    // Header: magic(4) + version(2) + id(2) = 8 bytes
    uint8_t hdr[8];
    if (f.read(hdr, 8) != 8) { f.close(); return false; }
    if (memcmp(hdr, "PKDP", 4) != 0) { f.close(); return false; }

    out->id = hdr[6] | (hdr[7] << 8);

    // Body: name[20] + name_es[20] + type1 + type2 + gen + height u16 + weight u16 + base_exp u16
    uint8_t body[56];
    if (f.read(body, 56) != 56) { f.close(); return false; }

    int off = 0;
    memcpy(out->name, body + off, 20); off += 20;
    out->name[20] = '\0';
    // Ensure null-terminated within the 20 bytes
    for (int i = 0; i < 20; i++) if (out->name[i] == '\0') break;

    memcpy(out->name_es, body + off, 20); off += 20;
    out->name_es[20] = '\0';
    for (int i = 0; i < 20; i++) if (out->name_es[i] == '\0') break;

    out->type1 = body[off++];
    out->type2 = body[off++];
    out->generation = body[off++];
    out->height_cm = body[off] | (body[off+1] << 8); off += 2;
    out->weight_dkg = body[off] | (body[off+1] << 8); off += 2;
    out->base_exp = body[off] | (body[off+1] << 8); off += 2;

    out->hp = body[off++];
    out->attack = body[off++];
    out->defense = body[off++];
    out->sp_attack = body[off++];
    out->sp_defense = body[off++];
    out->speed = body[off++];

    // Descriptions (variable length)
    uint8_t desc_en_len = body[off++];
    if (desc_en_len > 0) {
        char* buf = (char*)malloc(desc_en_len + 1);
        if (buf) {
            int rd = f.read((uint8_t*)buf, desc_en_len);
            if (rd == (int)desc_en_len) {
                buf[desc_en_len] = '\0';
                out->desc_en = String(buf);
            }
            free(buf);
        }
    }

    uint8_t desc_es_len;
    if (f.read(&desc_es_len, 1) == 1 && desc_es_len > 0) {
        char* buf = (char*)malloc(desc_es_len + 1);
        if (buf) {
            int rd = f.read((uint8_t*)buf, desc_es_len);
            if (rd == (int)desc_es_len) {
                buf[desc_es_len] = '\0';
                out->desc_es = String(buf);
            }
            free(buf);
        }
    }

    f.close();
    out->valid = true;
    return true;
}

bool LocalPokemonDatabase::_nameStartsWith(const char* haystack, const char* needle) {
    while (*needle) {
        char h = *haystack;
        if (h >= 'A' && h <= 'Z') h += 32;
        char n = *needle;
        if (n >= 'A' && n <= 'Z') n += 32;
        if (h != n) return false;
        haystack++;
        needle++;
    }
    return true;
}
