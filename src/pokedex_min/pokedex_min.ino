#include <Arduino.h>
#include <TFT_eSPI.h>
#include <FT6336U.h>
#include "../config/hardware_config.h"

TFT_eSPI  tft   = TFT_eSPI();
FT6336U   ctp(CTP_SDA, CTP_SCL, CTP_RST, CTP_INT);

// Touch→Display transform (90° CW)
#define TOUCH_MAX_X  239
int transformX(int tx, int ty) { return ty; }              // display_x = touch_y
int transformY(int tx, int ty) { return TOUCH_MAX_X - tx; } // display_y = 239 - touch_x

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 2000) {}

  tft.init();
  tft.setRotation(1);  // landscape 320x240
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  ctp.begin();
  uint8_t fw = ctp.read_firmware_id();

  Serial.println();
  Serial.println("=== TOUCH TEST (with transform) ===");
  Serial.printf("[OK] FT6336U fw=0x%02X\n", fw);
  Serial.println("[INFO] Touch ANYWHERE on screen");
  Serial.println("=== READY ===");

  // Static screen
  tft.fillScreen(tft.color565(30, 30, 30));
  tft.setTextColor(TFT_GREEN, tft.color565(30, 30, 30));
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("TOUCH ANYWHERE", 160, 100);
  tft.setTextSize(1);
  tft.drawString("dots show transformed position", 160, 140);
  tft.drawString("green cross = expected, red dot = actual", 160, 155);
}

void loop() {
  FT6336U_TouchPointType tp = ctp.scan();
  if (tp.touch_count > 0 && tp.tp[0].status == 0) {
    int tx = tp.tp[0].x;
    int ty = tp.tp[0].y;
    int dx = transformX(tx, ty);
    int dy = transformY(tx, ty);

    Serial.printf("RAW(%d,%d) -> DISP(%d,%d)\n", tx, ty, dx, dy);

    // Draw dot at transformed position
    tft.fillCircle(dx, dy, 5, tft.color565(255, 50, 50));
  }
  delay(10);
}
