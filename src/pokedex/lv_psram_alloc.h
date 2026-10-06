/**
 * lv_psram_alloc.h — PSRAM-backed allocator for LVGL
 *
 * ESP32-S3 has 8MB OPI PSRAM. We use it for LVGL's dynamic objects
 * so we can have thousands of UI objects without exhausting internal RAM.
 *
 * Falls back to internal RAM if PSRAM allocation fails.
 */
#ifndef LV_PSRAM_ALLOC_H
#define LV_PSRAM_ALLOC_H

#include <esp_heap_caps.h>
#include <string.h>
#include <stddef.h>

static inline void* lv_psram_malloc(size_t size) {
    // Try PSRAM first (8-bit, SPIRAM)
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) return p;
    // Fallback: internal RAM
    p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return p;
}

static inline void lv_psram_free(void* ptr) {
    if (ptr) heap_caps_free(ptr);
}

static inline void* lv_psram_realloc(void* ptr, size_t new_size) {
    if (!ptr) return lv_psram_malloc(new_size);
    if (new_size == 0) { lv_psram_free(ptr); return (void*)0; }

    void* new_p = lv_psram_malloc(new_size);
    if (!new_p) return (void*)0;

    // Copy old data (we don't know the old size, so just copy new_size or less)
    // In practice LVGL always grows, so copy the minimum of old and new.
    // For safety, we just memcpy the new_size bytes (old buffer is >= new_size for shrink,
    // or we lose data for grow — but LVGL always memsets new area).
    // Safer: use heap_caps_get_allocated_size
    size_t old_size = heap_caps_get_allocated_size(ptr);
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_p, ptr, copy_size);
    lv_psram_free(ptr);
    return new_p;
}

#endif // LV_PSRAM_ALLOC_H
