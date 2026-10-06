#include "PokemonDetailScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "PokemonService.h"
#include "FavoritesManager.h"
#include "CatalogSprite.h"
#include "asset_manager.h"
#include "pokemon_mechanics_db.h"
#include "storage_manager.h"
#include <stdio.h>
#include <string.h>

extern const lv_font_t lv_font_spanish_14;

// Elementos compartidos
static lv_obj_t *scr, *state_lbl, *name_lbl, *id_lbl;
static lv_obj_t *info, *evo, *stats, *moves;                       // 4 tarjetas de contenido
static lv_obj_t *type_badges[2], *type_labels[2], *height_lbl, *weight_lbl, *desc_lbl;
static lv_obj_t *fav_btn, *fav_lbl, *missing_lbl;
static lv_obj_t *info_btn, *evo_btn, *stats_btn, *moves_btn;       // 4 botones de pestaña
static lv_obj_t *bars[6], *values[6];
static lv_obj_t *gen_prev, *gen_next, *gen_lbl, *moves_list;       // selector de generación (ATAQUES)
static lv_obj_t *move_detail;                                       // panel derecho (detalle movimiento)
static lv_obj_t *move_sheet = nullptr;                              // ficha completa del ataque
static CatalogSprite sprite;
static uint16_t currentId = 0;
static bool valid = false;

// Datos de mecánicas (PKME) en PSRAM + estado del selector de generación
static MechanicsData* _mech = nullptr;
static bool _mechOk = false;
static int _mechGenSel = 0;
static int _selMoveIdx = -1;  // índice de movimiento seleccionado

static lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y,
                       const lv_font_t* font = &lv_font_montserrat_14) {
    auto* obj = Theme::makeLabel(parent, text, font, Theme::ink);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static const char* genName(uint8_t g) {
    static const char* n[] = {"I","II","III","IV","V","VI","VII","VIII","IX"};
    return (g >= 1 && g <= 9) ? n[g - 1] : "?";
}

// Copia limitada a char[] (evita -Wstringop-truncation)
static void copyName(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    while (n + 1 < cap && src[n]) { dst[n] = src[n]; n++; }
    dst[n] = 0;
}

static void clearChildren(lv_obj_t* p) {
    while (lv_obj_get_child_cnt(p) > 0) lv_obj_del(lv_obj_get_child(p, 0));
}

// ---- TIPOS DE MOVIMIENTO (colores + abreviatura) ----
static const uint32_t TYPE_COLORS[19] = {
    0,                       // 0 = desconocido
    0xA8A77A,  // 1 Normal
    0xEE8130,  // 2 Fuego
    0x6390F0,  // 3 Agua
    0xF7D02C,  // 4 Electricidad
    0x7AC74C,  // 5 Planta
    0x96D9D6,  // 6 Hielo
    0xC22E28,  // 7 Lucha
    0xA33EA1,  // 8 Veneno
    0xE2BF65,  // 9 Tierra
    0xA98FF3,  // 10 Volador
    0xF95587,  // 11 Psíquico
    0xA6B91A,  // 12 Bicho
    0xB6A136,  // 13 Roca
    0x735797,  // 14 Fantasma
    0x6F35FC,  // 15 Dragón
    0x705746,  // 16 Siniestro
    0xB7B7CE,  // 17 Acero
    0xD685AD,  // 18 Hada
};
static const char* TYPE_NAMES[19] = {
    "", "NORMAL", "FUEGO", "AGUA", "ELÉCTRICO", "PLANTA", "HIELO", "LUCHA",
    "VENENO", "TIERRA", "VOLADOR", "PSÍQUICO", "BICHO", "ROCA", "FANTASMA",
    "DRAGÓN", "SINIESTRO", "ACERO", "HADA"
};

// ---- EVOLUCIONES (horizontal cards) ----
static constexpr int EVO_MAX_SPRITES = 16;
static lv_obj_t* evo_imgs[EVO_MAX_SPRITES] = {};
static uint8_t*  evo_bufs[EVO_MAX_SPRITES] = {};
static lv_img_dsc_t evo_dscs[EVO_MAX_SPRITES];
static uint8_t* stone_bufs[15] = {};
static lv_img_dsc_t stone_dscs[15];

static bool loadEvoSprite(int idx, uint16_t species_id, lv_obj_t* parent, int x, int y) {
    if (idx >= EVO_MAX_SPRITES) return false;
    if (evo_bufs[idx]) { lv_mem_free(evo_bufs[idx]); evo_bufs[idx] = nullptr; }
    const size_t side = 48;
    const size_t capacity = 8 + side * side * 2;
    auto* buf = (uint8_t*)lv_mem_alloc(capacity);
    if (!buf) return false;
    evo_bufs[idx] = buf;
    uint16_t w = 0, h = 0;
    if (!assetMgr.loadSprite(species_id, false, buf, capacity, &w, &h)) {
        lv_mem_free(buf); evo_bufs[idx] = nullptr;
        return false;
    }
    // Recolorear fondo negro → transparent-ish (white)
    auto* pixels = reinterpret_cast<uint16_t*>(buf + 8);
    auto* queue = (uint16_t*)lv_mem_alloc(w * h * sizeof(uint16_t));
    if (queue) {
        uint16_t bgc = 0xF7F7F7; // white bg
        size_t head = 0, tail = 0;
        auto visit = [&](int i) {
            if (i >= 0 && i < w * h && pixels[i] == 0) { pixels[i] = bgc; queue[tail++] = i; }
        };
        for (int xx = 0; xx < w; ++xx) { visit(xx); visit((h - 1) * w + xx); }
        for (int yy = 0; yy < h; ++yy) { visit(yy * w); visit(yy * w + w - 1); }
        while (head < tail) {
            const int i = queue[head++], xx = i % w, yy = i / w;
            if (xx > 0) visit(i - 1);
            if (xx + 1 < w) visit(i + 1);
            if (yy > 0) visit(i - w);
            if (yy + 1 < h) visit(i + w);
        }
        lv_mem_free(queue);
    }
    memset(&evo_dscs[idx], 0, sizeof(lv_img_dsc_t));
    evo_dscs[idx].header.cf = LV_IMG_CF_TRUE_COLOR;
    evo_dscs[idx].header.w = w;
    evo_dscs[idx].header.h = h;
    evo_dscs[idx].data = buf + 8;
    evo_dscs[idx].data_size = w * h * 2;
    lv_img_cache_invalidate_src(&evo_dscs[idx]);
    if (!evo_imgs[idx]) {
        evo_imgs[idx] = lv_img_create(parent);
        lv_obj_clear_flag(evo_imgs[idx], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_set_parent(evo_imgs[idx], parent);
    lv_img_set_src(evo_imgs[idx], &evo_dscs[idx]);
    lv_obj_set_pos(evo_imgs[idx], x, y);
    lv_obj_clear_flag(evo_imgs[idx], LV_OBJ_FLAG_HIDDEN);
    return true;
}

static void freeEvoSprites() {
    for (int i = 0; i < EVO_MAX_SPRITES; ++i) {
        if (evo_bufs[i]) { lv_mem_free(evo_bufs[i]); evo_bufs[i] = nullptr; }
        if (evo_imgs[i]) { lv_obj_del(evo_imgs[i]); evo_imgs[i] = nullptr; }
    }
    for (int i = 0; i < 15; ++i) {
        if (stone_bufs[i]) {
            lv_img_cache_invalidate_src(&stone_dscs[i]);
            lv_mem_free(stone_bufs[i]); stone_bufs[i] = nullptr;
        }
    }
}

// Build evolution chain: find node matching current id, walk back to root, reverse
struct EvoChain {
    uint16_t ids[16];
    int count;
    uint8_t cond_kinds[15];   // condition kind between chain[i] and chain[i+1]
    uint16_t cond_values[15];
    char cond_texts[15][40]; // condition text
    bool edge_active[15];     // false separa dos ramas independientes
};

static void buildEvoChain(uint16_t cur_id, EvoChain& out) {
    memset(&out, 0, sizeof(out));
    if (!_mechOk || _mech->node_count == 0) return;

    int cur = -1, children[MECH_MAX_NODES], childCount = 0;
    for (int i = 0; i < _mech->node_count; ++i) {
        if (_mech->nodes[i].species_id == cur_id) cur = i;
    }
    if (cur < 0) return;
    for (int i = 0; i < _mech->node_count; ++i)
        if (_mech->nodes[i].parent_index == cur) children[childCount++] = i;

    auto appendEdge = [&](int from, int to) {
        if (out.count + 2 > 16) return;
        if (out.count > 0) out.edge_active[out.count - 1] = false;
        out.ids[out.count++] = _mech->nodes[from].species_id;
        const int edge = out.count - 1;
        out.ids[out.count++] = _mech->nodes[to].species_id;
        out.edge_active[edge] = true;
        const MechNode& dest = _mech->nodes[to];
        if (dest.condition_count) {
            out.cond_kinds[edge] = dest.conditions[0].kind;
            out.cond_values[edge] = dest.conditions[0].value;
            copyName(out.cond_texts[edge], sizeof(out.cond_texts[edge]), dest.conditions[0].text);
        }
    };

    if (childCount > 1) {
        // Una pareja completa por rama: Eevee→Vaporeon | Eevee→Jolteon...
        for (int i = 0; i < childCount && out.count + 2 <= 16; ++i) appendEdge(cur, children[i]);
        return;
    }

    // Cadena lineal alrededor de la especie: ancestros + especie + descendiente directo.
    int path[16], pn = 0, walk = cur;
    while (walk >= 0 && pn < 16) { path[pn++] = walk; walk = _mech->nodes[walk].parent_index; }
    for (int i = pn - 1; i >= 0; --i) out.ids[out.count++] = _mech->nodes[path[i]].species_id;
    // Continúa hasta el final de una línea no ramificada (Bulbasaur→Ivysaur→Venusaur).
    int descendant = cur;
    while (out.count < 16) {
        int onlyChild = -1, descendants = 0;
        for (int n = 0; n < _mech->node_count; ++n) {
            if (_mech->nodes[n].parent_index == descendant) { onlyChild = n; descendants++; }
        }
        if (descendants != 1) break;
        out.ids[out.count++] = _mech->nodes[onlyChild].species_id;
        descendant = onlyChild;
    }
    for (int i = 0; i + 1 < out.count; ++i) {
        int dest = -1;
        for (int n = 0; n < _mech->node_count; ++n) if (_mech->nodes[n].species_id == out.ids[i + 1]) dest = n;
        if (dest < 0) continue;
        out.edge_active[i] = true;
        const MechNode& node = _mech->nodes[dest];
        if (node.condition_count) {
            out.cond_kinds[i] = node.conditions[0].kind;
            out.cond_values[i] = node.conditions[0].value;
            copyName(out.cond_texts[i], sizeof(out.cond_texts[i]), node.conditions[0].text);
        }
    }
}

static uint32_t condColor(uint8_t kind) {
    if (kind == 2) return 0xE8C547;  // item → yellow
    if (kind == 3) return 0x2E9B91;  // trade → teal
    if (kind == 4 || kind == 14 || kind == 15 || kind == 16) return 0xF06292;
    return 0x4A90D9;                 // level → blue
}

static lv_obj_t* iconPart(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius = 4) {
    auto* p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h); lv_obj_set_pos(p, x, y);
    lv_obj_set_style_bg_color(p, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0); lv_obj_set_style_radius(p, radius, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return p;
}

// Iconos vectoriales ligeros: no dependen de glifos Unicode que falten en la fuente.
static uint8_t stoneAssetId(const char* condition) {
    if (!condition) return 0;
    if (strstr(condition, "Agua")) return 1;
    if (strstr(condition, "Alba")) return 2;
    if (strstr(condition, "Brillante") || strstr(condition, "Día")) return 3;
    if (strstr(condition, "Fuego")) return 4;
    if (strstr(condition, "Hielo")) return 5;
    if (strstr(condition, "Planta") || strstr(condition, "Hoja")) return 6;
    if (strstr(condition, "Luna")) return 7;
    if (strstr(condition, "Crepúsculo") || strstr(condition, "Noche")) return 8;
    if (strstr(condition, "Sol") || strstr(condition, "Solar")) return 9;
    if (strstr(condition, "Trueno")) return 10;
    return 0;
}

static bool loadStoneAsset(lv_obj_t* parent, int slot, uint8_t id) {
    if (slot < 0 || slot >= 15 || id == 0) return false;
    constexpr size_t capacity = 8 + 26 * 26 * 2;
    auto* buf = (uint8_t*)lv_mem_alloc(capacity);
    if (!buf) return false;
    char path[48]; snprintf(path, sizeof(path), "/pokedex/ui/stones/%02u.r565", id);
    size_t length = 0;
    if (!storageMgr.readFile(path, buf, capacity, &length) || length != capacity) {
        lv_mem_free(buf); return false;
    }
    stone_bufs[slot] = buf;
    memset(&stone_dscs[slot], 0, sizeof(lv_img_dsc_t));
    stone_dscs[slot].header.cf = LV_IMG_CF_TRUE_COLOR;
    stone_dscs[slot].header.w = 26; stone_dscs[slot].header.h = 26;
    stone_dscs[slot].data = buf + 8; stone_dscs[slot].data_size = 26 * 26 * 2;
    auto* image = lv_img_create(parent);
    lv_img_set_src(image, &stone_dscs[slot]); lv_obj_set_pos(image, 0, 0);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return true;
}

static void makeEvolutionAsset(lv_obj_t* parent, int slot, uint8_t kind, const char* condition) {
    if (kind == 1) {
        // El texto con el nivel real se añade en el llamador.
    } else if (kind == 15 || kind == 16) {
        if (loadStoneAsset(parent, slot, kind == 15 ? 11 : 12)) return;
    } else if (kind == 5) {
        const uint8_t timeAsset = (condition && strstr(condition, "día")) ? 13 :
                                  (condition && strstr(condition, "noche")) ? 14 : 0;
        if (loadStoneAsset(parent, slot, timeAsset)) return;
    } else if (kind == 4 || kind == 14) { // corazón: amistad/afecto
        iconPart(parent, 3, 4, 8, 6, 0xED4F78, 5);
        iconPart(parent, 15, 4, 8, 6, 0xED4F78, 5);
        iconPart(parent, 2, 7, 22, 6, 0xED4F78, 2);
        iconPart(parent, 5, 13, 16, 4, 0xED4F78, 2);
        iconPart(parent, 8, 17, 10, 3, 0xED4F78, 1);
        iconPart(parent, 11, 20, 4, 2, 0xED4F78, 1);
    } else if (kind == 2) {
        if (loadStoneAsset(parent, slot, stoneAssetId(condition))) return;
        // Piedra evolutiva: carcasa mineral y núcleo del color del elemento.
        uint32_t core = 0xE8C547;
        if (condition && strstr(condition, "Agua")) core = 0x6390F0;
        else if (condition && strstr(condition, "Fuego")) core = 0xEE8130;
        else if (condition && strstr(condition, "Planta")) core = 0x7AC74C;
        else if (condition && strstr(condition, "Hielo")) core = 0x96D9D6;
        else if (condition && strstr(condition, "Luna")) core = 0xB7B7CE;
        iconPart(parent, 3, 3, 20, 19, 0x758593, 8);
        if (condition && strstr(condition, "Trueno")) {
            iconPart(parent, 11, 5, 5, 8, core, 1);
            iconPart(parent, 8, 11, 8, 5, core, 1);
            iconPart(parent, 8, 15, 4, 5, core, 1);
        } else {
            iconPart(parent, 8, 8, 10, 10, core, 5);
        }
    } else if (kind == 3) { // intercambio: dos flechas opuestas
        auto* a = Theme::makeLabel(parent, ">", &lv_font_spanish_14, lv_color_hex(0x2E9B91));
        lv_obj_set_pos(a, 4, 1);
        auto* b = Theme::makeLabel(parent, "<", &lv_font_spanish_14, lv_color_hex(0x2E9B91));
        lv_obj_set_pos(b, 12, 10);
        iconPart(parent, 6, 8, 14, 2, 0x2E9B91, 1);
        iconPart(parent, 6, 16, 14, 2, 0x2E9B91, 1);
    } else {
        // El resto conserva una marca visual sin ocupar espacio con texto.
        iconPart(parent, 6, 6, 14, 14, condColor(kind), 7);
    }
}

// Click handler for evo card: navigate to that species
static void evoCardCb(lv_event_t* e) {
    int target = (int)(intptr_t)lv_event_get_user_data(e);
    if (target > 0 && target != currentId) {
        ScreenManager::getInstance().showDetail(target);
    }
}

static void renderEvo() {
    // Las imágenes son descendientes de las tarjetas. Libera sus buffers/objetos
    // antes de borrar el árbol para no volver a eliminar punteros LVGL ya destruidos.
    freeEvoSprites();
    clearChildren(evo);
    if (!_mechOk || _mech->node_count == 0) {
        auto* l = Theme::makeLabel(evo, "SIN CADENA EVOLUTIVA", Theme::font_small, Theme::ink);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    EvoChain ch;
    buildEvoChain(currentId, ch);
    if (ch.count == 0) {
        auto* l = Theme::makeLabel(evo, "SIN CADENA EVOLUTIVA", Theme::font_small, Theme::ink);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    // Las ramas se conservan completas y el panel permite desplazamiento horizontal.
    int N = ch.count;
    int visStart = 0, visEnd = N - 1;
    int visN = visEnd - visStart + 1;

    // Reference layout adapted to 320x240: three tall portrait cards with
    // sprite above the number/name/type, and compact condition connectors.
    const int cardW = visN == 1 ? 100 : (visN == 2 ? 116 : 76);
    const int connW = 38;
    const int cardH = 118;
    const int totalW = visN * cardW + (visN - 1) * connW;
    const int offX = totalW < 308 ? (308 - totalW) / 2 : 4;
    const int cardY = 8;

    lv_obj_t* connectorAssets[15] = {};
    for (int vi = 0; vi < visN; ++vi) {
        int di = visStart + vi;
        int cx = offX + vi * (cardW + connW);
        bool isCurrent = (ch.ids[di] == currentId);

        auto* card = Theme::makeCard(evo, cx, cardY, cardW, cardH);
        lv_obj_set_style_border_width(card, isCurrent ? 3 : 1, 0);
        lv_obj_set_style_border_color(card, isCurrent ? lv_color_hex(0x37DDEA) : Theme::steel, 0);
        lv_obj_set_style_bg_color(card, isCurrent ? lv_color_hex(0xCDEBED) : lv_color_hex(0xEFF4F7), 0);
        if (isCurrent) {
            lv_obj_set_style_shadow_color(card, lv_color_hex(0x37DDEA), 0);
            lv_obj_set_style_shadow_width(card, 8, 0);
            lv_obj_set_style_shadow_opa(card, LV_OPA_50, 0);
        }
        lv_obj_add_event_cb(card, evoCardCb, LV_EVENT_CLICKED, (void*)(intptr_t)ch.ids[di]);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

        // Sprite portrait.
        const int spX = (cardW - 48) / 2;
        const int spY = 4;
        if (!loadEvoSprite(vi, ch.ids[di], card, spX, spY)) {
            auto* q = Theme::makeLabel(card, "?", &lv_font_montserrat_18, Theme::gray);
            lv_obj_set_pos(q, cardW / 2 - 5, 20);
        }

        char numTxt[8];
        snprintf(numTxt, sizeof(numTxt), "#%03u", ch.ids[di]);
        auto* numL = Theme::makeLabel(card, numTxt, &lv_font_spanish_14, Theme::steel);
        lv_obj_set_width(numL, cardW);
        lv_obj_set_pos(numL, 0, 54);
        lv_obj_set_style_text_align(numL, LV_TEXT_ALIGN_CENTER, 0);

        Pokemon np;
        bool npOk = PokemonService::getDetail(ch.ids[di], np);
        char nm[12];
        if (npOk) {
            size_t len = 0;
            while (len < 10 && np.name[len]) len++;
            memcpy(nm, np.name.c_str(), len);
            nm[len] = 0;
        } else {
            strncpy(nm, "???", 11);
        }
        auto* nameL = Theme::makeLabel(card, nm, &lv_font_spanish_14, isCurrent ? Theme::ink : Theme::gray);
        lv_obj_set_width(nameL, cardW - 4);
        lv_obj_set_pos(nameL, 2, 70);
        lv_label_set_long_mode(nameL, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(nameL, LV_TEXT_ALIGN_CENTER, 0);

        if (npOk && np.type_count > 0) {
            uint32_t tc = np.types[0].color;
            char tname[10];
            size_t tlen = 0;
            while (tlen < 8 && np.types[0].name[tlen]) tlen++;
            memcpy(tname, np.types[0].name.c_str(), tlen);
            tname[tlen] = 0;
            for (size_t k = 0; tname[k]; ++k) if (tname[k] >= 'a' && tname[k] <= 'z') tname[k] -= 32;
            auto* tpill = lv_obj_create(card);
            lv_obj_set_size(tpill, cardW - 12, 19);
            lv_obj_set_pos(tpill, 6, 92);
            lv_obj_set_style_bg_color(tpill, lv_color_hex(tc), 0);
            lv_obj_set_style_bg_opa(tpill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(tpill, 6, 0);
            lv_obj_set_style_border_width(tpill, 0, 0);
            lv_obj_clear_flag(tpill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            auto* tl = lv_label_create(tpill);
            lv_label_set_text(tl, tname);
            lv_obj_set_style_text_font(tl, &lv_font_spanish_14, 0);
            uint32_t r = (tc >> 16) & 0xFF, g2 = (tc >> 8) & 0xFF, b2 = tc & 0xFF;
            uint32_t lum = (r * 299 + g2 * 587 + b2 * 114) / 1000;
            lv_obj_set_style_text_color(tl, lum > 160 ? lv_color_hex(0x1A1A2E) : lv_color_white(), 0);
            lv_obj_center(tl);
        }

        // Connector: arrow + condition
        if (vi < visN - 1 && ch.edge_active[di]) {
            const int connX = cx + cardW;
            auto* arrow = Theme::makeLabel(evo, ">", &lv_font_montserrat_18, lv_color_hex(0x4E7891));
            lv_obj_set_width(arrow, connW);
            lv_obj_set_pos(arrow, connX, 36);
            lv_obj_set_style_text_align(arrow, LV_TEXT_ALIGN_CENTER, 0);
            if (ch.cond_texts[di][0]) {
                auto* asset = lv_obj_create(evo);
                lv_obj_set_size(asset, connW, 26);
                lv_obj_set_pos(asset, connX, 61);
                lv_obj_set_style_bg_opa(asset, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(asset, 0, 0);
                lv_obj_set_style_pad_all(asset, 0, 0);
                lv_obj_clear_flag(asset, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
                if (ch.cond_kinds[di] == 1) {
                    char level[12]; snprintf(level, sizeof(level), "Nv%u", ch.cond_values[di]);
                    auto* ll = Theme::makeLabel(asset, level, &lv_font_spanish_14, lv_color_hex(condColor(1)));
                    lv_obj_set_width(ll, connW); lv_obj_set_pos(ll, 0, 5);
                    lv_obj_set_style_text_align(ll, LV_TEXT_ALIGN_CENTER, 0);
                } else {
                    auto* iconBox = lv_obj_create(asset);
                    lv_obj_set_size(iconBox, 26, 26); lv_obj_set_pos(iconBox, (connW - 26) / 2, 0);
                    lv_obj_set_style_bg_opa(iconBox, LV_OPA_TRANSP, 0);
                    lv_obj_set_style_border_width(iconBox, 0, 0); lv_obj_set_style_pad_all(iconBox, 0, 0);
                    lv_obj_clear_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
                    makeEvolutionAsset(iconBox, vi, ch.cond_kinds[di], ch.cond_texts[di]);
                }
                connectorAssets[vi] = asset;
            }
        } else if (vi < visN - 1) {
            auto* separator = iconPart(evo, cx + cardW + connW / 2, 20, 1, 96, 0x708796, 0);
            lv_obj_set_style_bg_opa(separator, LV_OPA_30, 0);
        }
    }
    // Las tarjetas posteriores se crean después; eleva los assets al final para
    // que el corazón/la piedra queden completos en el hueco entre especies.
    for (int i = 0; i < 15; ++i) if (connectorAssets[i]) lv_obj_move_foreground(connectorAssets[i]);
}

// ---- ATAQUES (two-column layout) ----
static void renderMoves();  // forward declaration

static void closeMoveSheet(lv_event_t*) {
    if (move_sheet) { lv_obj_del(move_sheet); move_sheet = nullptr; }
}

static const char* categoryName(uint8_t category) {
    if (category == 1) return "FÍSICO";
    if (category == 2) return "ESPECIAL";
    if (category == 3) return "ESTADO";
    return "--";
}

static void showMoveSheet(int idx) {
    if (!_mechOk || idx < 0 || idx >= (int)_mech->sections[_mechGenSel].move_count) return;
    const MechMove& m = _mech->sections[_mechGenSel].moves[idx];
    if (move_sheet) lv_obj_del(move_sheet);
    move_sheet = Theme::makeCard(moves, 4, 4, 300, 128);
    lv_obj_set_style_bg_color(move_sheet, lv_color_hex(0xEDF3F6), 0);
    lv_obj_set_style_border_color(move_sheet, lv_color_hex(0x37DDEA), 0);
    lv_obj_set_style_border_width(move_sheet, 2, 0);

    auto* title = Theme::makeLabel(move_sheet, m.name, &lv_font_spanish_14, Theme::ink);
    lv_obj_set_pos(title, 10, 7); lv_obj_set_width(title, 230);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

    auto* close = Theme::makeCard(move_sheet, 252, 4, 40, 30);
    Theme::button(close, Theme::red);
    lv_obj_add_event_cb(close, closeMoveSheet, LV_EVENT_CLICKED, nullptr);
    lv_obj_center(Theme::makeLabel(close, "X", Theme::font_small, Theme::white));

    char combat[96];
    char power[8], accuracy[8], pp[8];
    if (m.power == 255) copyName(power, sizeof(power), "--"); else snprintf(power, sizeof(power), "%u", m.power);
    if (m.accuracy == 255) copyName(accuracy, sizeof(accuracy), "--"); else snprintf(accuracy, sizeof(accuracy), "%u", m.accuracy);
    if (m.pp == 255) copyName(pp, sizeof(pp), "--"); else snprintf(pp, sizeof(pp), "%u", m.pp);
    snprintf(combat, sizeof(combat), "POT. %s   PREC. %s   PP %s   %s", power, accuracy, pp, categoryName(m.category));
    auto* statsL = Theme::makeLabel(move_sheet, combat, &lv_font_spanish_14, Theme::steel);
    lv_obj_set_pos(statsL, 10, 35); lv_obj_set_width(statsL, 280);

    auto* divider = iconPart(move_sheet, 10, 56, 280, 1, 0x9EB4C4, 0);
    (void)divider;
    const char* description = m.description[0] ? m.description : "Descripción no instalada. Actualiza los datos de la tarjeta SD.";
    auto* desc = Theme::makeLabel(move_sheet, description, &lv_font_spanish_14, Theme::ink);
    lv_obj_set_pos(desc, 10, 63); lv_obj_set_width(desc, 280);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(desc, 58);
}

static void renderMoveDetail(int idx) {
    clearChildren(move_detail);
    if (idx < 0 || !_mechOk || _mech->section_count == 0) {
        auto* l = Theme::makeLabel(move_detail, "Selecciona un movimiento", Theme::font_small, Theme::gray);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    const MechSection* sec = &_mech->sections[_mechGenSel];
    if (idx >= (int)sec->move_count) {
        auto* l = Theme::makeLabel(move_detail, "Selecciona un movimiento", Theme::font_small, Theme::gray);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    const MechMove& m = sec->moves[idx];

    auto* accent = lv_obj_create(move_detail);
    lv_obj_set_size(accent, 4, 120);
    lv_obj_set_pos(accent, 0, 4);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x39CAD8), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Resumen: solo datos de combate. La ficha completa se abre al tocar una fila.
    char mn[40];
    copyName(mn, sizeof(mn), m.name);
    auto* nameL = Theme::makeLabel(move_detail, mn, &lv_font_spanish_14, Theme::ink);
    lv_obj_set_pos(nameL, 10, 7);
    lv_obj_set_width(nameL, 122);
    lv_label_set_long_mode(nameL, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(nameL, 32);

    auto* heading = Theme::makeLabel(move_detail, "COMBATE", &lv_font_spanish_14, Theme::steel);
    lv_obj_set_pos(heading, 10, 43); lv_obj_set_width(heading, 116);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
    char line[32];
    snprintf(line, sizeof(line), "POTENCIA      %s", m.power == 255 ? "--" : "");
    if (m.power != 255) snprintf(line, sizeof(line), "POTENCIA      %u", m.power);
    auto* l1 = Theme::makeLabel(move_detail, line, &lv_font_spanish_14, Theme::ink); lv_obj_set_pos(l1, 10, 64);
    snprintf(line, sizeof(line), "PRECISIÓN     %s", m.accuracy == 255 ? "--" : "");
    if (m.accuracy != 255) snprintf(line, sizeof(line), "PRECISIÓN     %u", m.accuracy);
    auto* l2 = Theme::makeLabel(move_detail, line, &lv_font_spanish_14, Theme::ink); lv_obj_set_pos(l2, 10, 84);
    snprintf(line, sizeof(line), "PP            %s", m.pp == 255 ? "--" : "");
    if (m.pp != 255) snprintf(line, sizeof(line), "PP            %u", m.pp);
    auto* l3 = Theme::makeLabel(move_detail, line, &lv_font_spanish_14, Theme::ink); lv_obj_set_pos(l3, 10, 104);
}

// Click handler for move row
static void moveRowCb(lv_event_t* e) {
    (void)e;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0) return;
    _selMoveIdx = idx;
    // Re-render list (highlight) y abrir la ficha descriptiva del ataque.
    renderMoves();
    showMoveSheet(idx);
}

static void renderMoves() {
    const int oldScroll = moves_list ? lv_obj_get_scroll_top(moves_list) : 0;
    clearChildren(moves_list);
    if (!_mechOk || _mech->section_count == 0) {
        lv_obj_add_flag(gen_prev, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(gen_next, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(gen_lbl, "SIN MOVIMIENTOS");
        clearChildren(move_detail);
        auto* l = Theme::makeLabel(move_detail, "Sin datos", Theme::font_small, Theme::gray);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    lv_obj_clear_flag(gen_prev, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gen_next, LV_OBJ_FLAG_HIDDEN);
    char t[16];
    snprintf(t, sizeof(t), "GEN %s", genName(_mech->sections[_mechGenSel].generation));
    lv_label_set_text(gen_lbl, t);

    // Clamp selection
    const MechSection* sec = &_mech->sections[_mechGenSel];
    if (_selMoveIdx >= (int)sec->move_count) _selMoveIdx = 0;
    if (_selMoveIdx < 0) _selMoveIdx = 0;

    // Render move list like the reference: bordered rows, level column,
    // Spanish move name and a colour marker for the type.
    int y = 3;
    lv_obj_t* selectedRow = nullptr;
    for (uint16_t i = 0; i < sec->move_count; ++i) {
        const MechMove& m = sec->moves[i];
        char mn[24];
        copyName(mn, sizeof(mn), m.name);
        // Truncate name to fit the compact left column.
        size_t nlen = 0;
        while (nlen < 13 && mn[nlen]) nlen++;
        mn[nlen] = 0;

        bool selected = (i == _selMoveIdx);
        auto* row = Theme::makeCard(moves_list, 2, y, 148, 23);
        lv_obj_set_style_bg_color(row, selected ? lv_color_hex(0xD3F3F5) : lv_color_hex(0xF4F7F9), 0);
        lv_obj_set_style_border_width(row, selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(row, selected ? lv_color_hex(0x2ED4E5) : lv_color_hex(0xAFC0CC), 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, moveRowCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        if (selected) selectedRow = row;

        char level[8];
        if (m.level == 0) copyName(level, sizeof(level), "INI");
        else snprintf(level, sizeof(level), "%u", m.level);
        auto* levelL = Theme::makeLabel(row, level, Theme::font_small, selected ? Theme::red : Theme::gray);
        lv_obj_set_width(levelL, 28);
        lv_obj_set_pos(levelL, 2, 3);
        lv_obj_set_style_text_align(levelL, LV_TEXT_ALIGN_CENTER, 0);

        auto* name = Theme::makeLabel(row, mn, &lv_font_spanish_14, selected ? Theme::red_dark : Theme::ink);
        lv_obj_set_width(name, 103);
        lv_obj_set_pos(name, 32, 3);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        // Type strip mirrors the coloured TYPE column of the reference.
        if (m.type_code >= 1 && m.type_code <= 18) {
            auto* strip = lv_obj_create(row);
            lv_obj_set_size(strip, 8, 17);
            lv_obj_set_pos(strip, 137, 2);
            lv_obj_set_style_bg_color(strip, lv_color_hex(TYPE_COLORS[m.type_code]), 0);
            lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(strip, 3, 0);
            lv_obj_set_style_border_width(strip, 0, 0);
            lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        }
        y += 25;
    }
    lv_obj_scroll_to_y(moves_list, oldScroll, LV_ANIM_OFF);
    if (selectedRow) lv_obj_scroll_to_view(selectedRow, LV_ANIM_OFF);

    // Render detail panel (right column)
    renderMoveDetail(_selMoveIdx);
}

static void genPrevCb(lv_event_t*) {
    if (_mechOk && _mech->section_count > 0) {
        _mechGenSel = (int)(_mech->section_count + _mechGenSel - 1) % _mech->section_count;
        _selMoveIdx = 0;
        renderMoves();
    }
}
static void genNextCb(lv_event_t*) {
    if (_mechOk && _mech->section_count > 0) {
        _mechGenSel = (_mechGenSel + 1) % _mech->section_count;
        _selMoveIdx = 0;
        renderMoves();
    }
}

// ---- Pestañas (4) ----
static void setTab(int tab) {
    lv_obj_t* cards[] = {info, evo, stats, moves};
    lv_obj_t* btns[] = {info_btn, evo_btn, stats_btn, moves_btn};
    for (int i = 0; i < 4; ++i) {
        if (i == tab) lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cards[i], LV_OBJ_FLAG_HIDDEN);
        Theme::button(btns[i], i == tab ? Theme::red : Theme::blue);
    }
    if (tab == 3) renderMoves();   // refresca ATAQUES
    if (!valid) {
        lv_obj_add_flag(info, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(evo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(stats, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(moves, LV_OBJ_FLAG_HIDDEN);
    }
}
static void tab_cb(lv_event_t* e) {
    lv_obj_t* t = lv_event_get_target(e);
    int tab = (t == info_btn) ? 0 : (t == evo_btn) ? 1 : (t == stats_btn) ? 2 : 3;
    setTab(tab);
}

static void back_cb(lv_event_t*) { ScreenManager::getInstance().show(ScreenId::POKEDX_LIST); }
static void refreshFavorite() {
    const bool favorite = valid && FavoritesMgr::isFavorite(currentId);
    lv_label_set_text(fav_lbl, favorite ? LV_SYMBOL_OK " FAV" : "+ FAV");
    Theme::button(fav_btn, favorite ? Theme::blue : Theme::red_dark);
}
static void fav_cb(lv_event_t*) {
    if (!valid) return;
    FavoritesMgr::toggle(currentId);
    refreshFavorite();
}

static void loadMechFor(uint16_t id) {
    if (!_mech) _mech = (MechanicsData*)ps_malloc(sizeof(MechanicsData));
    _mechOk = false;
    _selMoveIdx = 0;
    if (_mech && PokemonService::loadMech(id, *_mech)) {
        _mechOk = true;
        _mechGenSel = 0;
    }
}

namespace PokemonDetailScreen {

lv_obj_t* create() {
    scr = Theme::shell();
    Theme::header(scr, "FICHA");
    auto* back = Theme::makeCard(scr, 4, 0, 100, 56);
    Theme::button(back, Theme::red);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_center(Theme::makeLabel(back, LV_SYMBOL_LEFT " VOLVER", Theme::font_small, Theme::white));
    fav_btn = Theme::makeCard(scr, 236, 0, 80, 56);
    Theme::button(fav_btn, Theme::red_dark);
    lv_obj_add_event_cb(fav_btn, fav_cb, LV_EVENT_CLICKED, nullptr);
    fav_lbl = Theme::makeLabel(fav_btn, "+ FAV", Theme::font_small, Theme::white);
    lv_obj_center(fav_lbl);

    // ---- FICHA (info) ----
    info = Theme::makeCard(scr, 6, 60, 308, 136);
    Theme::panel(info, Theme::card);
    lv_obj_add_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(info, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(info, LV_SCROLLBAR_MODE_AUTO);
    auto* well = Theme::makeCard(info, 4, 4, 106, 122);
    Theme::panel(well, Theme::teal);
    lv_obj_clear_flag(well, LV_OBJ_FLAG_CLICKABLE);
    sprite.create(well, 3, 3, true, Theme::teal);
    missing_lbl = Theme::makeLabel(well, "SIN\nIMAGEN", Theme::font_small, Theme::ink);
    lv_obj_center(missing_lbl);
    id_lbl = label(well, "No. ----", 8, 100);
    name_lbl = label(info, "", 118, 6, Theme::font_name);
    lv_obj_set_width(name_lbl, 180);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(name_lbl, 44);
    for (int i = 0; i < 2; ++i) {
        type_badges[i] = Theme::makeCard(info, 118 + i * 90, 86, 86, 24);
        Theme::panel(type_badges[i], Theme::blue);
        lv_obj_clear_flag(type_badges[i], LV_OBJ_FLAG_CLICKABLE);
        type_labels[i] = Theme::makeLabel(type_badges[i], "", Theme::font_small, Theme::white);
        lv_obj_center(type_labels[i]);
    }
    height_lbl = label(info, "", 118, 118);
    weight_lbl = label(info, "", 118, 140);
    label(info, "DESCRIPCION", 8, 168);
    desc_lbl = label(info, "", 8, 186);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_spanish_14, 0);
    lv_label_set_long_mode(desc_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc_lbl, 290);

    // ---- EVOLUCIONES (evo) - horizontal scrollable ----
    evo = Theme::makeCard(scr, 6, 60, 308, 136);
    Theme::panel(evo, lv_color_hex(0x9EB4C4));
    lv_obj_add_flag(evo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(evo, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(evo, LV_SCROLLBAR_MODE_AUTO);

    // ---- ESTADISTICAS (stats) ----
    stats = Theme::makeCard(scr, 6, 60, 308, 136);
    Theme::panel(stats, Theme::card);
    const char* names[] = {"HP", "ATK", "DEF", "SP.ATK", "SP.DEF", "SPD"};
    const uint32_t colors[] = {0x42AD70, 0xD34862, 0xE8B944, 0x38A5CB, 0x9565BA, 0xD66AA4};
    for (int i = 0; i < 6; ++i) {
        label(stats, names[i], 8, 6 + i * 21);
        values[i] = label(stats, "0", 72, 6 + i * 21);
        auto* track = Theme::makeCard(stats, 112, 8 + i * 21, 180, 12);
        lv_obj_set_style_bg_color(track, Theme::bg, 0);
        lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
        bars[i] = Theme::makeCard(track, 0, 0, 0, 12);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(colors[i]), 0);
        lv_obj_clear_flag(bars[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // ---- ATAQUES (moves) - two-column layout ----
    moves = Theme::makeCard(scr, 6, 60, 308, 136);
    Theme::panel(moves, lv_color_hex(0xC9D6DF));

    // Left column: gen selector + move list
    gen_prev = Theme::makeCard(moves, 4, 4, 36, 25);
    Theme::button(gen_prev, Theme::blue);
    lv_obj_add_event_cb(gen_prev, genPrevCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_center(Theme::makeLabel(gen_prev, LV_SYMBOL_LEFT, Theme::font_small, Theme::white));
    gen_lbl = Theme::makeLabel(moves, "GEN ?", Theme::font_small, Theme::ink);
    lv_obj_set_pos(gen_lbl, 44, 7);
    lv_obj_set_width(gen_lbl, 76);
    lv_obj_set_style_text_align(gen_lbl, LV_TEXT_ALIGN_CENTER, 0);
    gen_next = Theme::makeCard(moves, 124, 4, 36, 25);
    Theme::button(gen_next, Theme::blue);
    lv_obj_add_event_cb(gen_next, genNextCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_center(Theme::makeLabel(gen_next, LV_SYMBOL_RIGHT, Theme::font_small, Theme::white));

    moves_list = Theme::makeCard(moves, 4, 32, 156, 100);
    Theme::panel(moves_list, Theme::card);
    lv_obj_add_flag(moves_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(moves_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(moves_list, LV_SCROLLBAR_MODE_AUTO);

    // Right column: move detail panel
    move_detail = Theme::makeCard(moves, 164, 4, 140, 128);
    lv_obj_set_style_bg_color(move_detail, lv_color_hex(0xD9E3EA), 0);
    lv_obj_set_style_bg_opa(move_detail, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(move_detail, Theme::steel, 0);
    lv_obj_set_style_border_width(move_detail, 1, 0);
    lv_obj_clear_flag(move_detail, LV_OBJ_FLAG_CLICKABLE);

    // ---- 4 botones de pestaña ----
    info_btn  = Theme::makeCard(scr, 4, 200, 75, 36);
    evo_btn   = Theme::makeCard(scr, 83, 200, 75, 36);
    stats_btn = Theme::makeCard(scr, 162, 200, 75, 36);
    moves_btn = Theme::makeCard(scr, 241, 200, 75, 36);
    lv_obj_t* tabs[] = {info_btn, evo_btn, stats_btn, moves_btn};
    const char* captions[] = {"FICHA", "EVOL.", "STATS", "ATAQUES"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_add_event_cb(tabs[i], tab_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_center(Theme::makeLabel(tabs[i], captions[i], Theme::font_small, Theme::white));
    }

    state_lbl = Theme::makeLabel(scr, "", Theme::font_normal, Theme::ink);
    lv_obj_set_width(state_lbl, 276);
    lv_label_set_long_mode(state_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(state_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(state_lbl);
    lv_obj_add_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
    setTab(0);
    return scr;
}

void render(const Pokemon& p) {
    const bool changed = !valid || currentId != p.id;
    currentId = p.id;
    valid = true;
    lv_obj_add_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(name_lbl, p.name.c_str());
    char text[48];
    snprintf(text, sizeof(text), "No. %04u", p.id);
    lv_label_set_text(id_lbl, text);
    for (int i = 0; i < 2; ++i) {
        if (i < p.type_count) {
            lv_obj_clear_flag(type_badges[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(type_badges[i], lv_color_hex(p.types[i].color), 0);
            lv_label_set_text(type_labels[i], p.types[i].name.c_str());
            const uint32_t rgb = p.types[i].color;
            const uint32_t luminance = ((rgb >> 16) & 255) * 299 + ((rgb >> 8) & 255) * 587 + (rgb & 255) * 114;
            lv_obj_set_style_text_color(type_labels[i], luminance > 150000 ? Theme::ink : Theme::white, 0);
        } else lv_obj_add_flag(type_badges[i], LV_OBJ_FLAG_HIDDEN);
    }
    snprintf(text, sizeof(text), "ALT.  %.1f m", p.height_m); lv_label_set_text(height_lbl, text);
    snprintf(text, sizeof(text), "PESO  %.1f kg", p.weight_kg); lv_label_set_text(weight_lbl, text);
    lv_label_set_text(desc_lbl, p.description.c_str());
    const int numbers[] = {p.stats.hp, p.stats.attack, p.stats.defense,
                           p.stats.special_attack, p.stats.special_defense, p.stats.speed};
    for (int i = 0; i < 6; ++i) {
        const int n = numbers[i] < 0 ? 0 : (numbers[i] > 255 ? 255 : numbers[i]);
        lv_obj_set_width(bars[i], n * 180 / 255);
        snprintf(text, sizeof(text), "%d", numbers[i]); lv_label_set_text(values[i], text);
    }
    // Mecánicas (PKME): cadena evolutiva + ataques por generación
    loadMechFor(p.id);
    renderEvo();
    renderMoves();
    refreshFavorite();
    if (changed) {
        setTab(0);
        lv_obj_scroll_to_y(info, 0, LV_ANIM_OFF);
        if (sprite.show(p.id)) lv_obj_add_flag(missing_lbl, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(missing_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void update(uint16_t id) {
    Pokemon p;
    if (PokemonService::getDetail(id, p)) render(p);
    else showError();
}

void showError() {
    valid = false;
    currentId = 0;
    _mechOk = false;
    _selMoveIdx = -1;
    sprite.hide();
    setTab(0);
    refreshFavorite();
    lv_label_set_text(state_lbl, "FICHA NO DISPONIBLE EN SD\nPulsa VOLVER");
    lv_obj_clear_flag(state_lbl, LV_OBJ_FLAG_HIDDEN);
}

void selectTab(int tab) {
    if (tab < 0 || tab > 3) tab = 0;
    setTab(tab);
}

void selectGen(int index) {
    if (!_mechOk || _mech->section_count == 0) return;
    if (index < 0) index = 0;
    if (index >= (int)_mech->section_count) index = (int)_mech->section_count - 1;
    _mechGenSel = index;
    _selMoveIdx = 0;
    renderMoves();
}

void scrollMoves(int dy) {
    if (!moves_list) return;
    int top = lv_obj_get_scroll_top(moves_list);
    int next = top + dy;
    if (next < 0) next = 0;
    lv_obj_scroll_to_y(moves_list, next, LV_ANIM_OFF);
}

void scrollEvo(int dx) {
    if (!evo) return;
    int left = lv_obj_get_scroll_left(evo) + dx;
    if (left < 0) left = 0;
    lv_obj_scroll_to_x(evo, left, LV_ANIM_OFF);
}

void selectMove(int index) {
    if (!_mechOk || _mech->section_count == 0) return;
    const int count = _mech->sections[_mechGenSel].move_count;
    if (index < 0 || index >= count) return;
    _selMoveIdx = index;
    renderMoves();
    showMoveSheet(index);
}

} // namespace PokemonDetailScreen
