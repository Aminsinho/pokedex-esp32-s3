#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>
#include <FT6336U.h>
#include <Preferences.h>

struct TouchPoint {
    int16_t x;
    int16_t y;
    bool touched;
    uint16_t rawX;
    uint16_t rawY;
};

// Calibration: 2-point (TL and BR corners)
// display_x = (rawY - calXmin) * 319 / (calXmax - calXmin)
// display_y = (calYmax - rawX) * 239 / (calYmax - calYmin)
struct TouchCal {
    int16_t xMin; // rawY at top-left
    int16_t xMax; // rawY at bottom-right
    int16_t yMin; // rawX at bottom-right
    int16_t yMax; // rawX at top-left
};

class Touch {
public:
    static bool init();
    static TouchPoint read();
    static void mapToDisplay(uint16_t rawX, uint16_t rawY, int16_t &x, int16_t &y);
    static void setCalibration(TouchCal cal);
    static TouchCal getCalibration();
    static void saveCalibration();
    static void loadCalibration();
};
