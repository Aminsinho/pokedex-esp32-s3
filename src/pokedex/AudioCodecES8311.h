#pragma once
/*
 * AudioCodecES8311 — ES8311 codec driver via shared I2C bus.
 *
 * Based on the proven Freenove register sequence for the FNK0104B board.
 * I2C address: 0x18 (CE pin low, as wired on this board).
 *
 * The I2C bus is shared with Touch (FT6336U at 0x38).
 * This driver does NOT call Wire.begin() — it assumes the bus is already
 * initialized (by Touch::init()).
 */

#include <Arduino.h>
#include <Wire.h>

class AudioCodecES8311 {
public:
    // Initialize the codec. Returns true on success.
    // Assumes I2C bus is already initialized (Wire.begin() called).
    bool begin(uint32_t sampleRate = 44100, uint8_t volume = 85);

    // Set DAC volume (0-100).
    void setVolume(uint8_t volume);

    // Get current volume.
    uint8_t getVolume();

    // Mute / unmute.
    void setMute(bool muted);

    // Check if codec is reachable on I2C.
    bool isPresent();

    // Read codec chip ID for verification.
    uint16_t readChipId();

private:
    static constexpr uint8_t I2C_ADDR = 0x18;
    uint8_t volume_ = 85;
    bool muted_ = false;

    bool i2cWrite(uint8_t reg, uint8_t val);
    bool i2cRead(uint8_t reg, uint8_t* val);
    void delayMs(uint32_t ms);
};
