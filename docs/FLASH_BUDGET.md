# FLASH BUDGET — AUDIT + FIX (M3)

**Fecha:** 2026-09-04  
**Firmware:** 1,264,031 bytes (1.206 MB)  
**Partición APP actual:** 6.25 MB (default_16MB.csv)  
**Uso:** **19.3%** ✅

---

## ✅ RESUELTO

Partición 16MB aplicada exitosamente (2026-09-04).

**Método:** `partitions.csv` (= default_16MB.csv) en `src/pokedex/` + `PartitionScheme=custom` en FQBN.

**Resultado:** APP = 6.25 MB, uso = 19.3%, headroom = 5.29 MB.
Validado: compilación + flash + smoke test (LCD/TOUCH/LVGL/WiFi/Backend).

---

## PARTITION TABLES COMPARADAS

| Partición | default_4MB (ACTUAL) | default_16MB (PROYECTO) |
|-----------|---------------------|------------------------|
| nvs | 24 KB | 24 KB |
| otadata | 8 KB | 8 KB |
| **app (factory)** | **1,310,720 (1.25 MB)** | **6,553,600 (6.25 MB)** |
| spiffs | 1.30 MB | 8.37 MB |
| **Total** | **4 MB** | **16 MB** |

### Impacto del cambio:

| Métrica | 4MB (actual) | 16MB (propuesto) |
|---------|-------------|-----------------|
| APP size | 1.25 MB | 6.25 MB |
| Firmware | 1.206 MB | 1.206 MB |
| **Uso APP** | **96.4%** | **19.0%** |
| Headroom | 46,576 bytes | 5,349,456 bytes |
| SPiffs | 1.30 MB | 8.37 MB |

---

## CONSUMO DE FLASH POR COMPONENTE

### Top 15 archivos (.a / sketch) — secciones de salida verificadas

| # | Archivo | Flash | % |
|---|---------|-------|---|
| 1 | `[lib:lvgl]` | 252,404 B (246.5 KB) | 22.5% |
| 2 | `libnet80211.a` | 141,801 B (138.5 KB) | 12.6% |
| 3 | `liblwip.a` | 123,562 B (120.7 KB) | 11.0% |
| 4 | `[sketch]` | 102,353 B (100.0 KB) | 9.1% |
| 5 | `libmbedcrypto.a` | 66,791 B (65.2 KB) | 6.0% |
| 6 | `libwpa_supplicant.a` | 58,572 B (57.2 KB) | 5.2% |
| 7 | `libpp.a` | 51,219 B (50.0 KB) | 4.6% |
| 8 | `libc.a` | ~50,000 B (~49 KB) | ~4.4% |
| 9 | `core.a` | 40,925 B (40.0 KB) | 3.6% |
| 10 | `libphy.a` | 29,938 B (29.2 KB) | 2.7% |
| 11 | `libsoc.a` | 29,638 B (28.9 KB) | 2.6% |
| 12 | `libesp_wifi.a` | 12,325 B (12.0 KB) | 1.1% |
| 13 | `libesp_timer.a` | 12,319 B (12.0 KB) | 1.1% |
| 14 | `libnvs_flash.a` | 11,730 B (11.5 KB) | 1.0% |
| 15 | `libesp_psram.a` | 7,047 B (6.9 KB) | 0.6% |

**Total archivos .a:** 1,088,979 B (1063.5 KB) = 95.0% del flash

### Categorías de consumo

| Categoría | Flash | % | Componentes |
|-----------|-------|---|-------------|
| **WiFi / Red** | ~388 KB | ~34% | net80211 + lwip + wpa_supplicant + phy + pp + esp_wifi |
| **LVGL (UI)** | 246.5 KB | ~22% | render, draw, widgets, fonts |
| **Sketch (nuestro código)** | 100 KB | ~9% | pokedex_main, pokedex_*, main |
| **mbedcrypto** | 65.2 KB | ~6% | crypto primitives (WiFi WPA2/3) |
| **libc + core** | ~90 KB | ~8% | runtime, alloc, printf |
| **Sociales (soc/timer/psram/nvs)** | ~60 KB | ~5% | drivers básicos |
| **Otros** | ~113 KB | ~10% | resto |

---

## DETALLE LVGL (252,404 B)

### Fuentes tipográficas (LVMONTESANT — ~95 KB)

| Fuente | .text | .rodata | Total |
|--------|-------|---------|-------|
| montserrat_28 | 6,508 | 30,592 | 37,100 (36.2 KB) |
| montserrat_22 | 4,680 | 20,512 | 25,192 (24.6 KB) |
| montserrat_18 | 3,332 | 15,776 | 19,108 (18.7 KB) |
| montserrat_14 | 2,592 | 11,456 | 14,048 (13.7 KB) |
| **Total fuentes** | **17,112** | **78,336** | **95,448 (93.2 KB)** |

### Módulos LVGL (top 10 por objeto)

| Objeto | .text | .rodata | Total |
|--------|-------|---------|-------|
| lv_draw.c | 3,212 | 6,956 | 10,168 |
| lv_label.c | 5,396 | 1,940 | 7,336 |
| lv_refr.c | 5,844 | 1,032 | 6,876 |
| lv_arc.c | 6,300 | 144 | 6,444 |
| lv_image.c | 5,788 | 168 | 5,956 |
| lv_draw_sdl.c | 2,188 | 3,520 | 5,708 |
| lv_event.c | 2,316 | 3,004 | 5,320 |
| lv_theme_default.c | 4,144 | 904 | 5,048 |
| lv_draw_arc.c | 3,528 | 1,456 | 4,984 |
| lv_timer.c | 2,908 | 2,020 | 4,928 |

### Observaciones LVGL

- **lv_draw_sdl.c** (5,708 B) está presente — es el SDL backend, NO el native ESP32.
  Esto es anómalo: en firmware ESP32-S3 debería usarse `lvgl/bsp/esp32` o
  `lvgl/driver/esp32`. Posible mal-configuración de build.
- **lv_freetype** NO está linked (bueno — usa fuentes estáticas).
- **lv_demos / lv_examples / lv_themes** NO están linked (bueno).
- 4 tamaños de fuente = ~95 KB. Si solo se usan 2 (18 + 28), se ahorran ~48 KB.

---

## MBEDTLS / CRYPTO

| Archivo | Flash | Rol |
|---------|-------|-----|
| libmbedcrypto.a | 65.2 KB | Crypto primitives (AES, SHA, HMAC) — **requerido por WPA2/WPA3** |
| libmbedtls.a (TLS) | **0 B** | **NO está linked** |
| libmbedtls_2.a | **0 B** | **NO está linked** |

**Conclusión:** El firmware NO incluye mbedTLS TLS completo. Solo los
primitivos criptográficos que WiFi necesita para WPA2/WPA3.
Si se implementa HTTPS (BackendClient con TLS), se añadirían ~80-120 KB más.

---

## WIFI STACK (total ~388 KB)

| Componente | Flash | Función |
|------------|-------|---------|
| libnet80211.a | 138.5 KB | 802.11 MAC/PHY |
| liblwip.a | 120.7 KB | TCP/IP stack |
| libwpa_supplicant.a | 57.2 KB | WPA2/WPA3 auth |
| libphy.a | 29.2 KB | PHY driver |
| libpp.a | 50.0 KB | Power profile |
| libesp_wifi.a | 12.0 KB | WiFi API |
| **Total WiFi** | **~407 KB** | **Irreducible** |

> WiFi es un requisito del proyecto (BackendClient). No se puede reducir
> sin perder conectividad.

---

## SKECH (NUESTRO CÓDIGO) — 100 KB

| Objeto | Flash |
|--------|-------|
| pokedex_main.cpp.o | 24,284 |
| pokedex_app.cpp.o | 13,092 |
| pokedex_ui.cpp.o | 10,616 |
| pokedex_mock.cpp.o | 9,396 |
| pokedex_backend.cpp.o | 7,488 |
| main.cpp.o | 6,664 |
| pokedex_ota.cpp.o | 5,876 |
| pokedex_prefs.cpp.o | 3,192 |
| pokedex_audio.cpp.o | 3,072 |
| **Total sketch** | **~100 KB** |

> Nuestro código es 9% del flash. Es razonable para un app completo.

---

## RAM (bonus)

| Sección | Size |
|---------|------|
| .dram0.bss | 70,800 |
| .dram0.data | 14,756 |
| .dram0.heap | 104,976 |
| .iram0.text | 26,652 |
| **Total DRAM (static)** | **~92 KB** |
| **Total IRAM (static)** | **~28 KB** |

Con 8 MB PSRAM habilitada, la RAM interna es suficiente para LVGL + WiFi.

---

## RIESGOS

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|-------------|---------|-----------|
| Agotamiento de flash al crecer | ALTA (con 4MB) | BLOQUEANTE | **Cambiar a 16MB** |
| HTTPS/TLS añade ~100 KB | ALTA | Moderado | Con 16MB: no es problema |
| Nuevas features LVGL | MEDIA | Bajo | Con 16MB: no es problema |
| OTA (dual image) | ALTA | BLOQUEANTE con 4MB | 16MB permite 2×6MB |

---

## RECOMENDACIÓN

### ACCIÓN INMEDIATA (0 riesgo, 0 código)

Cambiar la partition table de `default_4MB.csv` a `default_16MB.csv`.

**Cómo:**
1. En `platformio.ini` o `platformio` config: `board_build.partitions = default_16MB.csv`
2. O en Arduino IDE: `tools/build/partitions/default_16MB.csv` ya existe
3. Rebuild → flash → verificar con `esptool.py partition_table`

**Resultado:**
- APP: 1.25 MB → 6.25 MB (96.4% → 19.0%)
- SPiffs: 1.30 MB → 8.37 MB
- Headroom: 46 KB → 5.3 MB

### ACCIONES OPCIONALES (si se quiere ahorrar aún más)

| Acción | Ahorro | Riesgo |
|--------|--------|--------|
| Reducir fuentes a 2 tamaños (18+28) | ~48 KB | Bajo (verificar UI) |
| Desactivar lv_draw_sdl (no usado en ESP32) | ~5 KB | Bajo |
| Usar `LV_USE_` selectives (desactivar widgets no usados) | 10-30 KB | Medio |
| Compilar con `-Os` en vez de `-Og` | 5-10% | Medio (desperf) |

### NO RECOMENDADO

| Acción | Razón |
|--------|-------|
| Desactivar WiFi | Proyecto requiere BackendClient |
| Desactivar mbedcrypto | WiFi WPA2/WPA3 lo requiere |
| Reducir PSRAM | Necesaria para LVGL buffers |

---

## METODOLOGÍA

1. **Parser del .map:** `tools/analyze_map_final.py` — parsea secciones de salida
   y valida input sections sum vs output section size.
2. **Validación ELF:** `xtensa-esp32s3-elf-size` confirma sizes de secciones.
3. **Partición:** `tools/build_analysis/partitions.csv` = default_4MB (confirmado).
4. **Firmware:** `tools/build_analysis/pokedex.ino.bin` = 1,264,144 bytes.
5. **ELF total:** 2,422,272 bytes (incluye RAM, no va a flash).

**Nota:** La suma de input sections (1,314,728 B) excede el output (1,145,680 B)
por ~15% debido a: (a) padding/alignment entre secciones, (b) sections que el
linker reordena, (c) el parser puede tener doble-conteo en algunas secciones.
El tamaño real de firmware es el .bin: **1,264,144 bytes**.
