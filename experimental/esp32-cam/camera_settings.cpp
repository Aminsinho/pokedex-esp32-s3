#include "camera_settings.h"
#include "camera_config.h"

CameraSettings camera_default_settings() {
    CameraSettings s;
    s.live_size      = FRAMESIZE_VGA;     // 640x480 (stable live view)
    s.hq_size        = FRAMESIZE_VGA;     // 640x480 (HQ capture - same buffer, higher quality)
    s.brightness     = 0;
    s.contrast       = 0;
    s.saturation     = 0;
    s.sharpness      = 0;
    s.denoise        = 0;
    s.gainceiling    = 2;                 // GAINCEILING_8X
    s.aec            = true;
    s.aec_level      = 0;
    s.agc            = true;
    s.agc_gain       = 0;
    s.awb            = true;
    s.awb_gain       = true;
    s.wb_mode        = 0;                 // auto
    s.bpc            = true;
    s.wpc            = true;
    s.raw_gma        = true;
    s.lenc           = true;
    s.special_effect = 0;                 // none
    s.hmirror        = false;
    s.vflip          = false;
    s.jpeg_quality   = DEF_JPEG_QUALITY;
    s.stream_quality = DEF_STREAM_QUALITY;
    s.best_of_n      = DEF_BEST_OF_N;
    return s;
}

const char* framesize_name(framesize_t fs) {
    switch (fs) {
        case FRAMESIZE_QVGA: return "QVGA(320x240)";
        case FRAMESIZE_CIF:  return "CIF(400x296)";
        case FRAMESIZE_VGA:  return "VGA(640x480)";
        case FRAMESIZE_SVGA: return "SVGA(800x600)";
        case FRAMESIZE_XGA:  return "XGA(1024x768)";
        case FRAMESIZE_HD:   return "HD(1280x720)";
        case FRAMESIZE_SXGA: return "SXGA(1280x1024)";
        case FRAMESIZE_UXGA: return "UXGA(1600x1200)";
        default:             return "?";
    }
}

void camera_apply_image_settings(const CameraSettings& s) {
    sensor_t* sen = esp_camera_sensor_get();
    if (!sen) return;
    // Image params (sensor may ignore unsupported ones; safe to call repeatedly)
    sen->set_brightness(sen, s.brightness);
    sen->set_contrast(sen, s.contrast);
    sen->set_saturation(sen, s.saturation);
    sen->set_sharpness(sen, s.sharpness);
    sen->set_denoise(sen, s.denoise);
    sen->set_gainceiling(sen, (gainceiling_t)s.gainceiling);
    // Auto controls
    sen->set_aec2(sen, s.aec ? 1 : 0);
    sen->set_ae_level(sen, s.aec_level);
    sen->set_gain_ctrl(sen, s.agc ? 1 : 0);
    sen->set_agc_gain(sen, s.agc_gain);
    sen->set_whitebal(sen, s.awb ? 1 : 0);
    sen->set_awb_gain(sen, s.awb_gain ? 1 : 0);
    sen->set_wb_mode(sen, s.wb_mode);
    // Corrections
    sen->set_bpc(sen, s.bpc ? 1 : 0);
    sen->set_wpc(sen, s.wpc ? 1 : 0);
    sen->set_raw_gma(sen, s.raw_gma ? 1 : 0);
    sen->set_lenc(sen, s.lenc ? 1 : 0);
    sen->set_special_effect(sen, s.special_effect);
    // Transform
    sen->set_hmirror(sen, s.hmirror ? 1 : 0);
    sen->set_vflip(sen, s.vflip ? 1 : 0);
}
