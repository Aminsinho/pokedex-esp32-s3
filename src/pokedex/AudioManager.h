#pragma once
/*
 * AudioManager — Audio output for the Pokédex (ESP32-S3 + ES8311)
 *
 * Tone/UI events and bounded canonical PCM WAV playback from SD.
 * Commands never block the caller. TTS is not implemented.
 *
 * Architecture:
 *   - ES8311 codec via I2C (shared bus with touch)
 *   - I2S output via ESP_I2S library (44100 Hz, 16-bit, stereo, left slot)
 *   - Audio task on Core 0 generates PCM in real-time
 *   - I2S DMA buffer (6×240) absorbs scheduling jitter
 *
 * I2S pins (from hardware_config.h):
 *   MCK=4, BCK=5, DIN=6 (codec ADC, unused), DOUT=8, WS=7
 */

#include <Arduino.h>
#include "ESP_I2S.h"

class AudioManager {
public:
    // Initialize codec + I2S. Call after Touch::init() (Wire already up).
    // sampleRate: 44100 for both codec and I2S (Freenove coefficient).
    // volume: 0-100
    static bool begin(int sampleRate = 44100, int volume = 85);
    static void playCry(uint16_t pokemonId);
    static void playNarration(uint16_t pokemonId);
    static void playClick();

    // Non-blocking; bounded queue, software amplitude capped at 40 percent.
    static void playTone(float freq, uint32_t durationMs, int16_t volume = 25, bool square = false);

    // Predefined UI sounds
    static void playConfirm();
    static void playError();
    static void playScan();

    // Boot beep test (1kHz, 200ms) — physical hardware verification.
    static void bootBeepTest();

    // Volume control (0-100)
    static void setVolume(int vol);

    // State
    static bool isReady();
    static bool isPlaying();

    // Stop current tone immediately
    static void stop();

    // Canonical PCM WAV, copied asynchronously; TTS not implemented.
    static void playWav(const uint8_t* data, size_t len);
    static void playTTS(const char* text, bool blocking);

    // Start the audio generation task on the specified core
    static void startTask(int core = 0);
};
