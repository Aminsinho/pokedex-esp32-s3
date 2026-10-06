#include "Touch.h"

static FT6336U ft6336u(16, 15, 18, 17);
static TouchCal cal_ = {0, 319, 0, 239}; // default: no calibration

static Preferences prefs;

bool Touch::init() {
    Wire.begin(16, 15);
    Wire.setClock(400000);

    // Reset sequence
    pinMode(18, OUTPUT);
    digitalWrite(18, LOW);
    delay(50);
    digitalWrite(18, HIGH);
    delay(100);

    ft6336u.begin();
    Wire.setTimeOut(20);

    // Erase corrupted calibration (factory reset)
    prefs.begin("touchcal", false);
    prefs.remove("xMin");
    prefs.remove("xMax");
    prefs.remove("yMin");
    prefs.remove("yMax");
    prefs.end();
    cal_ = {0, 319, 0, 239}; // reset to defaults

    return true;
}

void Touch::mapToDisplay(uint16_t rawX, uint16_t rawY, int16_t &x, int16_t &y) {
    // Apply calibration: linear mapping with integer math
    int32_t xRange = (int32_t)cal_.xMax - cal_.xMin;
    int32_t yRange = (int32_t)cal_.yMax - cal_.yMin;

    int32_t mx, my;
    if (xRange > 0) {
        mx = (int32_t)(rawY - cal_.xMin) * 319 / xRange;
    } else {
        mx = rawY;
    }
    if (yRange > 0) {
        my = (int32_t)(cal_.yMax - rawX) * 239 / yRange;
    } else {
        my = 239 - rawX;
    }

    x = constrain((int)mx, 0, 319);
    y = constrain((int)my, 0, 239);
}

TouchPoint Touch::read() {
    TouchPoint p = {0, 0, false, 0, 0};
    // Read status + first contact atomically. The vendor scan() sleeps 10 ms
    // per register and retries forever on an I2C error; neither is UI-safe.
    Wire.beginTransmission(0x38);
    Wire.write(0x02); // TD_STATUS, P1_XH, P1_XL, P1_YH, P1_YL
    if (Wire.endTransmission(false) != 0) return p;
    if (Wire.requestFrom((uint8_t)0x38, (uint8_t)5) != 5) return p;
    uint8_t data[5];
    for (int i = 0; i < 5; ++i) data[i] = Wire.read();
    const uint8_t contacts = data[0] & 0x0F;
    const uint8_t event = data[1] >> 6;
    if (contacts > 0 && contacts <= 2 && event != 1) {
        uint16_t tx = ((data[1] & 0x0F) << 8) | data[2];
        uint16_t ty = ((data[3] & 0x0F) << 8) | data[4];
        if (tx > 239 || ty > 319) return p;
        p.rawX = tx;
        p.rawY = ty;
        mapToDisplay(tx, ty, p.x, p.y);
        p.touched = true;
    }
    return p;
}

void Touch::setCalibration(TouchCal cal) {
    cal_ = cal;
}

TouchCal Touch::getCalibration() {
    return cal_;
}

void Touch::saveCalibration() {
    prefs.begin("touchcal", false); // RW
    prefs.putInt("xMin", cal_.xMin);
    prefs.putInt("xMax", cal_.xMax);
    prefs.putInt("yMin", cal_.yMin);
    prefs.putInt("yMax", cal_.yMax);
    prefs.end();
}

void Touch::loadCalibration() {
    prefs.begin("touchcal", true); // RO
    cal_.xMin = prefs.getInt("xMin", 0);
    cal_.xMax = prefs.getInt("xMax", 319);
    cal_.yMin = prefs.getInt("yMin", 0);
    cal_.yMax = prefs.getInt("yMax", 239);
    prefs.end();
}
