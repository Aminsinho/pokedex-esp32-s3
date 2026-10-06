# HARDWARE_PINOUT — FNK0104B (2.8" ILI9341 Touch, ESP32-S3)

Solo GPIO ESP32-S3 ↔ función. Todos los valores con evidencia directa en código oficial
Freenove (`official/Freenove_ESP32_S3_Display/`).

| GPIO | Función        | Evidencia (archivo oficial) |
|------|----------------|------------------------------|
| 13   | LCD_MISO       | `TFT_eSPI_Setups/FNK0104AB_2.8_240x320_ILI9341.h` (TFT_MISO) |
| 11   | LCD_MOSI       | idem (TFT_MOSI) |
| 12   | LCD_SCLK       | idem (TFT_SCLK) |
| 10   | LCD_CS         | idem (TFT_CS) |
| 46   | LCD_DC / RS    | idem (TFT_DC) |
| 45   | LCD_BL         | idem (TFT_BL, ON=HIGH) |
| 16   | CTP_SDA        | `Sketch_11.1_Touch.ino` (I2C_SDA) |
| 15   | CTP_SCL        | idem (I2C_SCL) |
| 18   | CTP_RST        | idem (RST_N_PIN) |
| 17   | CTP_INT        | idem (INT_N_PIN) |
| 4    | I2S_MCK        | `Sketch_07.1_Music.ino` (I2S_MCK) |
| 5    | I2S_SCK / BCK  | idem (I2S_BCK) |
| 6    | I2S_DI / DINT  | idem (I2S_DINT) |
| 8    | I2S_DO / DOUT  | idem (I2S_DOUT) |
| 7    | I2S_LRC / WS   | idem (I2S_WS) |
| 15   | ES8311 I2C_SCL | `Sketch_07.1_Music.ino` (I2C_SCL) — COMPARTIDO con CTP_SCL |
| 16   | ES8311 I2C_SDA | idem (I2C_SDA) — COMPARTIDO con CTP_SDA |
| 38   | SD_CLK         | `Sketch_06.1_SDMMC_Test.ino` (SD_MMC_CLK) |
| 40   | SD_CMD         | idem (SD_MMC_CMD) |
| 39   | SD_D0          | idem (SD_MMC_D0) |
| 41   | SD_D1          | idem (SD_MMC_D1) |
| 48   | SD_D2          | idem (SD_MMC_D2) |
| 47   | SD_D3          | idem (SD_MMC_D3) |
| 42   | RGB_LED (WS2812) | `Sketch_02.1_LedPixel.ino` (LEDS_PIN) |
| 0    | KEY (botón)    | `Sketch_03.1_Button_RGB.ino` (KEY_PIN) |
| 9    | BAT_ADC        | `Sketch_05.1_Battery_Voltage.ino` (BAT_ADC_PIN) |

## Notas (hechos, sin hipótesis)
- Touch = FT6336U (I2C). LCD = ILI9341, 240×320, BGR, inversion ON, SPI 40 MHz, HSPI.
- Los pines 15/16 son la única bus I2C del board: la comparten el touch FT6336U y el
  codec ES8311 (mismo bus I2C, distintas direcciones de dispositivo).
- SD usa SDMMC (1-bit), no SPI.
- Pantalla sin pin RST dedicado (TFT_RST = -1, RST atado al reset del board).

## Sin confirmar
- Ninguno de los GPIO solicitados quedó sin evidencia oficial.
- Pines de cámara: no presentes en la documentación oficial FNK0104B (no se lista).
