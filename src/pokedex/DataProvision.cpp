#include "DataProvision.h"
#include "storage_manager.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

static uint32_t fnv1a(const uint8_t* data, size_t size) {
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; ++i) value = (value ^ data[i]) * 16777619u;
    return value;
}
static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
static bool validNarration(const uint8_t* p, size_t n) {
    return n >= 44 && n <= 1048576 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WAVEfmt ", 8)
        && le32(p + 4) == n - 8 && le32(p + 16) == 16 && p[20] == 1 && p[21] == 0
        && p[22] == 1 && p[23] == 0 && le32(p + 24) == 44100 && le32(p + 28) == 88200
        && p[32] == 2 && p[33] == 0 && p[34] == 16 && p[35] == 0
        && !memcmp(p + 36, "data", 4) && le32(p + 40) == n - 44;
}
void provisionDataFile(const char* command) {
    unsigned id = 0, length = 0; char kind[8] = {};
    if (sscanf(command, "@%7s %u %u", kind, &id, &length) != 3 || id < 1 || id > 1025) {
        Serial.println("DATA ERROR command"); return;
    }
    const bool detail = !strcmp(kind, "DATA"), narration = !strcmp(kind, "NARR"), mech = !strcmp(kind, "MECH");
    const bool stone = !strcmp(kind, "STONE");
    if ((!detail && !narration && !mech && !stone) || (detail && (length < 64 || length > 768)) ||
        (narration && (length < 44 || length > 2 * 1024 * 1024)) || (mech && (length < 20 || length > 32768))) {
        Serial.println("DATA ERROR size"); return;
    }
    auto* data = static_cast<uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) { Serial.println("DATA ERROR memory"); return; }
    Serial.println("DATA READY");
    const auto previousTimeout = Serial.getTimeout(); Serial.setTimeout(2500);
    size_t received = 0;
    while (received < length) {
        const size_t chunk = min(size_t(128), size_t(length) - received);
        const size_t count = Serial.readBytes(data + received, chunk); received += count;
        if (count != chunk) break;
        Serial.println("DATA CHUNK");
    }
    Serial.setTimeout(previousTimeout);
    const bool payloadOk = received == length &&
        (detail    ? (!memcmp(data, "PKDP", 4) && (data[6] | unsigned(data[7]) << 8) == id)
         : mech    ? (!memcmp(data, "PKME", 4) && (data[6] | unsigned(data[7]) << 8) == id)
         : stone   ? (length == 1360 && le32(data) == 26 && le32(data + 4) == 26)
         :          validNarration(data, length));
    if (!payloadOk) { Serial.println("DATA ERROR payload"); heap_caps_free(data); return; }
    char path[64];
    snprintf(path, sizeof(path), detail ? "/pokedex/pokemon/data/%04u.bin"
                                 : mech   ? "/pokedex/pokemon/mechanics/data/%04u.bin"
                                 : stone  ? "/pokedex/ui/stones/%02u.r565"
                                          : "/pokedex/audio/narration/%04u.wav", id);
    if (narration) storageMgr.mkdir("/pokedex/audio/narration");
    if (mech) { storageMgr.mkdir("/pokedex/pokemon/mechanics"); storageMgr.mkdir("/pokedex/pokemon/mechanics/data"); }
    if (stone) { storageMgr.mkdir("/pokedex/ui"); storageMgr.mkdir("/pokedex/ui/stones"); }
    const uint32_t expected = fnv1a(data, length);
    bool ok = storageMgr.writeFile(path, data, length); size_t actual = 0;
    ok = ok && storageMgr.readFile(path, data, length, &actual) && actual == length && fnv1a(data, length) == expected;
    if (ok) Serial.printf("DATA OK %08lx\n", static_cast<unsigned long>(expected));
    else Serial.println("DATA ERROR verify");
    heap_caps_free(data);
}
