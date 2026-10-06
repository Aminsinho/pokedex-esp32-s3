#pragma once
#include "lvgl.h"
#include <stdint.h>

struct Pokemon;

namespace PokemonDetailScreen {
    lv_obj_t* create();
    void update(uint16_t id);   // local SD detail + catalogue sprite; explicit error if missing
    void render(const Pokemon& p);
    void showError();
    void selectTab(int tab);    // QA/diagnóstico: cambia de pestaña (0..3)
    void selectGen(int index);  // QA/diagnóstico: selección de generación (ATAQUES)
    void scrollMoves(int dy);   // QA/diagnóstico: desplaza la lista de movimientos (ATAQUES)
    void scrollEvo(int dx);     // QA/diagnóstico: desplaza ramas evolutivas
    void selectMove(int index); // QA/diagnóstico: abre la ficha completa de un ataque
}
