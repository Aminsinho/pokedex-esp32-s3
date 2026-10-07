#ifndef CAMERA_SETTINGS_H
#define CAMERA_SETTINGS_H

#include <Arduino.h>
#include "esp_camera.h"

// Runtime-tweakable camera parameters (applied to the live sensor).
struct CameraSettings {
    framesize_t live_size;      // live view resolution
    framesize_t hq_size;        // HQ capture resolution

    // Image
    int  brightness;     // -3..3
    int  contrast;       // -3..3
    int  saturation;     // -4..4
    int  sharpness;      // -2..2
    int  denoise;        // 0..8
    int  gainceiling;    // 0..6 (2x..128x)

    // Auto controls
    bool aec;            // auto exposure
    int  aec_level;      // -5..5
    bool agc;            // auto gain
    int  agc_gain;       // 0..64 (manual, when AGC off)
    bool awb;            // auto white balance
    bool awb_gain;       // AWB gain
    int  wb_mode;        // 0=auto 1=sunny 2=cloudy 3=office 4=home

    // Corrections
    bool bpc;            // black pixel correct
    bool wpc;            // white pixel correct
    bool raw_gma;        // raw gamma
    bool lenc;           // lens correction
    int  special_effect; // 0 none 1 neg 2 gray 3..6 tints 7 sepia

    // Transform
    bool hmirror;
    bool vflip;

    // Quality
    int  jpeg_quality;   // 6..63 (lower = better)
    int  stream_quality;
    int  best_of_n;      // 1..5
};

CameraSettings camera_default_settings();

// Apply a settings struct to the live sensor (image params, not resolution).
void camera_apply_image_settings(const CameraSettings& s);

// Human-readable name for a framesize.
const char* framesize_name(framesize_t fs);

#endif // CAMERA_SETTINGS_H
