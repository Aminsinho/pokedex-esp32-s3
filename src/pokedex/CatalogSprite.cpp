#include "CatalogSprite.h"
#include "asset_manager.h"
#include <cstring>

void CatalogSprite::create(lv_obj_t* parent, int x, int y, bool large, lv_color_t background) {
    large_ = large;
    background_ = background;
    image_ = lv_img_create(parent);
    lv_obj_set_pos(image_, x, y);
    lv_obj_clear_flag(image_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    hide();
}

void CatalogSprite::hide() {
    if (image_) lv_obj_add_flag(image_, LV_OBJ_FLAG_HIDDEN);
}

bool CatalogSprite::show(uint16_t id) {
    hide(); // never display a stale species when SD load fails
    const size_t side = large_ ? 96 : 48;
    const size_t capacity = 8 + side * side * 2;
    if (!buffer_) buffer_ = static_cast<uint8_t*>(lv_mem_alloc(capacity));
    if (!buffer_ || !image_) return false;
    lv_img_cache_invalidate_src(&descriptor_);
    uint16_t w = 0, h = 0;
    if (!assetMgr.loadSprite(id, large_, buffer_, capacity, &w, &h)) return false;

    // Legacy R565 flattened transparent pixels to black. Recolour only the
    // border-connected black background, preserving enclosed dark details.
    auto* pixels = reinterpret_cast<uint16_t*>(buffer_ + 8);
    auto* queue = static_cast<uint16_t*>(lv_mem_alloc(w * h * sizeof(uint16_t)));
    if (queue && background_.full != 0) {
        size_t head = 0, tail = 0;
        auto visit = [&](int i) {
            if (pixels[i] == 0) { pixels[i] = background_.full; queue[tail++] = i; }
        };
        for (int x = 0; x < w; ++x) { visit(x); visit((h - 1) * w + x); }
        for (int y = 0; y < h; ++y) { visit(y * w); visit(y * w + w - 1); }
        while (head < tail) {
            const int i = queue[head++], x = i % w, y = i / w;
            if (x) visit(i - 1);
            if (x + 1 < w) visit(i + 1);
            if (y) visit(i - w);
            if (y + 1 < h) visit(i + w);
        }
    }
    if (queue) lv_mem_free(queue);
    memset(&descriptor_, 0, sizeof(descriptor_));
    descriptor_.header.cf = LV_IMG_CF_TRUE_COLOR;
    descriptor_.header.w = w;
    descriptor_.header.h = h;
    descriptor_.data = buffer_ + 8;
    descriptor_.data_size = w * h * 2;
    lv_img_set_src(image_, &descriptor_);
    lv_obj_clear_flag(image_, LV_OBJ_FLAG_HIDDEN);
    return true;
}
