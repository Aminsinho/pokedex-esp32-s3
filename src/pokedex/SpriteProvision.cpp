#include "SpriteProvision.h"
#include "storage_manager.h"
#include "lvgl.h"
#include <Arduino.h>

static uint32_t checksum(const uint8_t* data, size_t size) {
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; ++i) value = (value ^ data[i]) * 16777619u;
    return value;
}

// Explicit USB provisioning only. Fixed sprite path, bounded payload, readback.
void provisionSprite(const char* command) {
    unsigned id = 0, length = 0;
    char kind = 0;
    if (sscanf(command, "@SPRITE %u %c %u", &id, &kind, &length) != 3 ||
        id < 1 || id > 1025 || (kind != 'S' && kind != 'L')) {
        Serial.println("SPRITE ERROR command"); return;
    }
    const unsigned side = kind == 'L' ? 96 : 48;
    if (length != 8 + side * side * 2) { Serial.println("SPRITE ERROR size"); return; }
    auto* data = static_cast<uint8_t*>(lv_mem_alloc(length));
    if (!data) { Serial.println("SPRITE ERROR memory"); return; }
    Serial.println("SPRITE READY");
    const auto timeout = Serial.getTimeout();
    Serial.setTimeout(2000);
    size_t received = 0;
    while (received < length) {
        const size_t chunk = min(static_cast<size_t>(128), length - received);
        const size_t count = Serial.readBytes(data + received, chunk);
        received += count;
        if (count != chunk) break;
        Serial.println("SPRITE CHUNK");
    }
    Serial.setTimeout(timeout);
    uint32_t w = 0, h = 0;
    if (received == length) { memcpy(&w, data, 4); memcpy(&h, data + 4, 4); }
    if (received != length || w != side || h != side) {
        Serial.println("SPRITE ERROR payload"); lv_mem_free(data); return;
    }
    char path[72];
    snprintf(path, sizeof(path), "/pokedex/pokemon/sprites/%s/%04u.r565", kind == 'L' ? "large" : "small", id);
    const uint32_t expected = checksum(data, length);
    bool ok = storageMgr.writeFile(path, data, length);
    size_t actual = 0;
    ok = ok && storageMgr.readFile(path, data, length, &actual) && actual == length && checksum(data, length) == expected;
    if (ok) Serial.printf("SPRITE OK %08lx\n", static_cast<unsigned long>(expected));
    else Serial.println("SPRITE ERROR verify");
    lv_mem_free(data);
}
