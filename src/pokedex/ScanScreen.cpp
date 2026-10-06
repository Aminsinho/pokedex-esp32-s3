#include "ScanScreen.h"
#include "Theme.h"
#include "ScreenManager.h"
#include "ScanService.h"
#include "PokemonService.h"
#include "CatalogSprite.h"
#include <cstring>
#include <cstdlib>
#include <stdio.h>

static lv_obj_t *scr, *status_lbl, *btn, *btn_lbl, *backend_dot, *camera_dot, *target;
static lv_obj_t *scan_line = nullptr;
static lv_anim_t scan_anim;
static bool scan_anim_running = false;
static CatalogSprite matchedSprite;

static void scan_anim_cb(void* obj, int32_t v) {
    lv_obj_set_y((lv_obj_t*)obj, v);
}


static void back_cb(lv_event_t*) {
    ScanService::cancel();
    ScreenManager::getInstance().show(ScreenId::HOME);
}
static void scan_btn_cb(lv_event_t*) {
    if (ScanService::isActive()) return;
    ScanScreen::reset();
    if (PokemonService::backend() != BackendState::ONLINE) {
        ScanScreen::setState("BACKEND OFFLINE", 0xE8B93E);
        return;
    }
    ScanService::start();
    ScanScreen::setState("WAITING FOR CAMERA...", 0xE8B93E);
}
lv_obj_t* ScanScreen::create() {
    scr = Theme::shell();
    Theme::header(scr, "SCAN");
    auto* back = Theme::makeCard(scr, 4, 0, 100, 56);
    Theme::button(back, Theme::red);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_center(Theme::makeLabel(back, LV_SYMBOL_LEFT " VOLVER", Theme::font_small, Theme::white));
    camera_dot = Theme::makeCard(scr, 290, 14, 10, 10);
    backend_dot = Theme::makeCard(scr, 290, 34, 10, 10);
    for (auto* dot : {camera_dot, backend_dot}) {
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    }
    auto* panel = Theme::makeCard(scr, 6, 60, 308, 116);
    Theme::panel(panel, Theme::card);
    auto* visor = Theme::makeCard(panel, 4, 4, 110, 104);
    Theme::panel(visor, Theme::teal);
    lv_obj_clear_flag(visor, LV_OBJ_FLAG_CLICKABLE);
    target = Theme::makeCard(visor, 19, 17, 68, 68);
    lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(target, 2, 0);
    lv_obj_set_style_border_color(target, Theme::white, 0);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_CLICKABLE);
    auto* cross = Theme::makeLabel(target, "+", Theme::font_title, Theme::white);
    lv_obj_center(cross);
    matchedSprite.create(visor, 5, 2, true, Theme::teal);
    /* scan-line animation: created LAST so it draws on top of everything */
    scan_line = lv_obj_create(visor);
    lv_obj_set_size(scan_line, 104, 8);
    lv_obj_set_pos(scan_line, 3, 2);
    lv_obj_set_style_radius(scan_line, 0, 0);
    lv_obj_set_style_border_width(scan_line, 0, 0);
    lv_obj_set_style_pad_top(scan_line, 0, 0);
    lv_obj_set_style_pad_bottom(scan_line, 0, 0);
    lv_obj_set_style_pad_left(scan_line, 0, 0);
    lv_obj_set_style_pad_right(scan_line, 0, 0);
    lv_obj_set_style_bg_color(scan_line, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scan_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    auto* title = Theme::makeLabel(panel, "CAMARA PC", Theme::font_name, Theme::ink);
    lv_obj_set_pos(title, 122, 8);
    auto* subtitle = Theme::makeLabel(panel, "OLLAMA / LOCAL", Theme::font_small, Theme::gray);
    lv_obj_set_pos(subtitle, 122, 34);
    status_lbl = Theme::makeLabel(panel, "", Theme::font_small, Theme::blue);
    lv_obj_set_width(status_lbl, 174);
    lv_obj_set_height(status_lbl, 54);
    lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(status_lbl, 122, 56);
    btn = Theme::makeCard(scr, 6, 180, 308, 56);
    Theme::button(btn, Theme::red);
    lv_obj_add_event_cb(btn, scan_btn_cb, LV_EVENT_CLICKED, nullptr);
    btn_lbl = Theme::makeLabel(btn, LV_SYMBOL_EYE_OPEN " ESCANEAR", Theme::font_name, Theme::white);
    lv_obj_center(btn_lbl);
    reset();
    return scr;
}
void ScanScreen::setState(const char* text, uint32_t color) {
    (void)color;
    if (!status_lbl) return;
    const char* shown = text;
    bool working = false;
    if (!strcmp(text, "WAITING FOR CAMERA...")) { shown = "Esperando camara..."; working = true; }
    else if (!strcmp(text, "CAPTURING...")) { shown = "Capturando..."; working = true; }
    else if (!strcmp(text, "UPLOADING...")) { shown = "Enviando imagen..."; working = true; }
    else if (!strcmp(text, "ANALYZING...")) { shown = "Identificando..."; working = true; }
    else if (!strcmp(text, "BACKEND OFFLINE")) shown = "Sin conexion al PC";
    else if (!strcmp(text, "NOT RECOGNIZED - RETRY")) shown = "No reconocido. Reintenta.";
    else if (strstr(text, "FAILED") || strstr(text, "TIMEOUT")) shown = "Error. Vuelve a escanear.";
    lv_label_set_text(status_lbl, shown);
    lv_obj_set_style_text_color(status_lbl, Theme::blue, 0);
    if (working) startScanAnim();
    else stopScanAnim();
}
void ScanScreen::setBackend(bool online) {
    if (backend_dot) lv_obj_set_style_bg_color(backend_dot, online ? lv_color_hex(0x25AF69) : Theme::gray, 0);
}
void ScanScreen::setCamera(bool online) {
    if (camera_dot) lv_obj_set_style_bg_color(camera_dot, online ? lv_color_hex(0x25B4DF) : Theme::gray, 0);
}
void ScanScreen::showMatch(const char* name, float confidence) {
    (void)confidence;
    stopScanAnim();
    char text[80];
    snprintf(text, sizeof(text), "Encontrado:\n%s", name);
    setState(text, 0);
    if (matchedSprite.show(ScanService::matchedId())) lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
    Theme::button(btn, Theme::blue);
    lv_label_set_text(btn_lbl, LV_SYMBOL_OK " IDENTIFICADO");
}
void ScanScreen::onShow() {
    reset();
    setBackend(PokemonService::backend() == BackendState::ONLINE);
    setCamera(NetworkTask::cameraOnline());
}

void ScanScreen::startScanAnim() {
    if (!scan_line) { Serial.println("[ANIM] scan_line is NULL!"); return; }
    if (scan_anim_running) return;
    scan_anim_running = true;
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[ANIM] startScanAnim");
    lv_anim_init(&scan_anim);
    lv_anim_set_var(&scan_anim, scan_line);
    lv_anim_set_exec_cb(&scan_anim, scan_anim_cb);
    lv_anim_set_values(&scan_anim, 2, 94);
    lv_anim_set_time(&scan_anim, 700);
    lv_anim_set_playback_time(&scan_anim, 700);
    lv_anim_set_repeat_count(&scan_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&scan_anim, lv_anim_path_ease_in_out);
    lv_anim_start(&scan_anim);
}

void ScanScreen::stopScanAnim() {
    if (!scan_anim_running) return;
    scan_anim_running = false;
    lv_anim_del(scan_line, scan_anim_cb);
    lv_obj_set_y(scan_line, 2);
    lv_obj_add_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[ANIM] stopScanAnim");
}
void ScanScreen::reset() {
    stopScanAnim();
    setState("Enfoca un Pokemon y pulsa ESCANEAR.", 0);
    matchedSprite.hide();
    if (target) lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
    if (btn) Theme::button(btn, Theme::red);
    if (btn_lbl) lv_label_set_text(btn_lbl, LV_SYMBOL_EYE_OPEN " ESCANEAR");
}
