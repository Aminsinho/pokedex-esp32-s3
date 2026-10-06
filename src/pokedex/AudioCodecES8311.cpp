// ES8311, 44.1 kHz/256x MCLK. Register sequence derived from Freenove's
// es8311.cpp (Espressif Apache-2.0), using Wire shared with touch.
#include "AudioCodecES8311.h"
bool AudioCodecES8311::i2cWrite(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDR); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission() == 0;
}
bool AudioCodecES8311::i2cRead(uint8_t reg, uint8_t* value) {
    Wire.beginTransmission(I2C_ADDR); Wire.write(reg);
    if (Wire.endTransmission(false) || Wire.requestFrom(I2C_ADDR, uint8_t(1)) != 1) return false;
    *value = Wire.read(); return true;
}
void AudioCodecES8311::delayMs(uint32_t ms) { delay(ms); }
bool AudioCodecES8311::isPresent() { uint8_t value; return i2cRead(0xFD, &value); }
uint16_t AudioCodecES8311::readChipId() {
    uint8_t hi=0, lo=0; i2cRead(0xFD, &hi); i2cRead(0xFE, &lo);
    return uint16_t(hi)<<8 | lo;
}
void AudioCodecES8311::setVolume(uint8_t volume) {
    volume_ = min(volume, uint8_t(100));
    i2cWrite(0x32, volume_ ? volume_*256/100-1 : 0);
}
uint8_t AudioCodecES8311::getVolume() { return volume_; }
void AudioCodecES8311::setMute(bool muted) {
    uint8_t reg; if (!i2cRead(0x31, &reg)) return;
    i2cWrite(0x31, muted ? reg|0x60 : reg&~0x60); muted_=muted;
}
bool AudioCodecES8311::begin(uint32_t sampleRate, uint8_t volume) {
    if (sampleRate != 44100 || !isPresent()) return false;
    if (!i2cWrite(0x00, 0x1F)) return false;
    delay(20);
    // 11289600/44100 coefficient: pre_div=1, mult=0, adc/dac_div=1,
    // lrck=255, bclk_div=4, adc/dac_osr=16. I2S 16-bit = 3<<2.
    const uint8_t registers[][2] = {
        {0x00,0x00},{0x00,0x80},{0x01,0x3F},
        {0x02,0x00},{0x03,0x10},{0x04,0x10},{0x05,0x00},
        {0x06,0x03},{0x07,0x00},{0x08,0xFF},
        {0x09,0x0C},{0x0A,0x0C},
        {0x0D,0x01},{0x0E,0x02},{0x12,0x00},{0x13,0x10},
        {0x1C,0x6A},{0x37,0x08},{0x31,0x00}
    };
    for (const auto& r : registers) if (!i2cWrite(r[0],r[1])) return false;
    volume_=min(volume,uint8_t(100));
    if (!i2cWrite(0x32, volume_ ? volume_*256/100-1 : 0)) return false;
    Serial.printf("[AUDIO] ES8311 ID=%04X rate=44100 MCLK=11289600 volume=%u\n",readChipId(),volume_);
    return true;
}
