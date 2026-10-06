#ifndef LV_CONF_H
#define LV_CONF_H 1

/* Color settings */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Display */
#define LV_DPI_DEF 130

/* Memory */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48 * 1024)

/* Logging */
#define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR
#define LV_LOG_PRINTF 1

/* Features */
#define LV_USE_ANTIALIAS 1
#define LV_USE_SHADOW 0
#define LV_USE_OUTLINE 0
#define LV_USE_DRAW_SW 1
#define LV_USE_FONT_MONTSERRAT 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1
#define LV_USE_LIST 1
#define LV_USE_LINE 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_OBJ_PROPERTY 0

/* Others */
#define LV_USE_REFR_DEBUG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_OS 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_INDEV_STACK 0
#define LV_USE_ASSERT_OBJ 0

/* Indev */
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_INDEV_DRAG_THROW 0

/* Themes */
#define LV_THEME_DEFAULT_COLOR_PRIMARY lv_color_hex(0xD62828)
#define LV_THEME_DEFAULT_COLOR_SECONDARY lv_color_hex(0x29B6F6)
#define LV_THEME_DEFAULT_FLAG LV_THEME_FLAG_SIMPLE

/* Misc */
#define LV_CACHE_SIZE 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_STRESS_TEST 0

#endif
