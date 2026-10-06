#pragma once

// === LCD ILI9341 (2.8" 240x320) ===
#define LCD_MISO  13
#define LCD_MOSI  11
#define LCD_SCLK  12
#define LCD_CS    10
#define LCD_DC    46
#define LCD_BL    45
#define LCD_RST   -1

// === Touch FT6336U (I2C) ===
#define CTP_SDA   16
#define CTP_SCL   15
#define CTP_RST   18
#define CTP_INT   17

// === Touch→Display transform (90° CW, validated) ===
#define TOUCH_MAX_X  239
// display_x = touch_y
// display_y = TOUCH_MAX_X - touch_x

// === Audio I2S (ES8311) — documented, NOT used yet ===
#define I2S_MCK   4
#define I2S_BCK   5
#define I2S_DI    6
#define I2S_DO    8
#define I2S_WS    7
#define I2S_PA    7  // PA_EN (amplifier enable)
