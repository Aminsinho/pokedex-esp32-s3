#include "camera.h"
#include "camera_config.h"
#include "img_converters.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "WiFi.h"
#include "HTTPClient.h"

// ---- AI-Thinker ESP32-CAM pin map (proven stable) ----
static constexpr int PWDN_GPIO  = 32;
static constexpr int RESET_GPIO = -1;
static constexpr int XCLK_GPIO  = 0;
static constexpr int SIOD_GPIO  = 26;
static constexpr int SIOC_GPIO  = 27;
static constexpr int Y9_GPIO    = 35;
static constexpr int Y8_GPIO    = 34;
static constexpr int Y7_GPIO    = 39;
static constexpr int Y6_GPIO    = 36;
static constexpr int Y5_GPIO    = 21;
static constexpr int Y4_GPIO    = 19;
static constexpr int Y3_GPIO    = 18;
static constexpr int Y2_GPIO    = 5;
static constexpr int VSYNC_GPIO = 25;
static constexpr int HREF_GPIO  = 23;
static constexpr int PCLK_GPIO  = 22;
static constexpr int LED_GPIO   = 4;

// The camera is initialised at XGA so the DMA framebuffer is always large
// enough for both the VGA live view and the XGA HQ capture (no realloc risk).
// UXGA is larger than XGA and therefore needs the re-init path.
static framesize_t g_initSize = FRAMESIZE_VGA;

// Concurrency: a mutex serialises "reconfigure + capture" against the stream.
static SemaphoreHandle_t g_camMutex = nullptr;
static volatile bool     g_hqBusy  = false;

// ---- memory helpers ----
uint32_t camera_psram_total() { return (uint32_t)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024); }
uint32_t camera_psram_free()  { return (uint32_t)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)  / 1024); }
uint32_t camera_heap_free()   { return (uint32_t)(ESP.getFreeHeap() / 1024); }

// ---- sensor name from PID ----
const char* camera_sensor_name() {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return "unknown";
    switch (s->id.PID) {
        case 0x2145: return "GC2145";
        case 0x2640: return "OV2640";
        case 0x9650: return "OV9650";
        case 0x3660: return "OV3660";
        case 0x0328: return "GC0328";
        case 0x7020: return "GC7020";
        default: {
            static char buf[16];
            snprintf(buf, sizeof(buf), "0x%04X", (unsigned)s->id.PID);
            return buf;
        }
    }
}

// ---- low-level init at a given resolution ----
static bool camera_init_at(framesize_t fs) {
    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0 = Y2_GPIO;  cfg.pin_d1 = Y3_GPIO;
    cfg.pin_d2 = Y4_GPIO;  cfg.pin_d3 = Y5_GPIO;
    cfg.pin_d4 = Y6_GPIO;  cfg.pin_d5 = Y7_GPIO;
    cfg.pin_d6 = Y8_GPIO;  cfg.pin_d7 = Y9_GPIO;
    cfg.pin_xclk = XCLK_GPIO; cfg.pin_pclk = PCLK_GPIO;
    cfg.pin_vsync = VSYNC_GPIO; cfg.pin_href = HREF_GPIO;
    cfg.pin_sccb_sda = SIOD_GPIO; cfg.pin_sccb_scl = SIOC_GPIO;
    cfg.pin_pwdn = PWDN_GPIO; cfg.pin_reset = RESET_GPIO;
    // 20 MHz XCLK = known-stable for GC2145 on this board.
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_YUV422;
    cfg.frame_size   = fs;
    cfg.jpeg_quality = 10;
    cfg.fb_count     = 1;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    cfg.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("[CAM] init@%s FAILED err=0x%x\n", framesize_name(fs), err);
        return false;
    }
    g_initSize = fs;
    Serial.printf("[CAM] init@%s OK\n", framesize_name(fs));
    return true;
}

bool camera_init() {
    if (!g_camMutex) g_camMutex = xSemaphoreCreateMutex();
    // Init at VGA: buffer = 614KB, sensor output = VGA. No mismatch.
    // Both stream and HQ capture from the same VGA buffer.
    return camera_init_at(FRAMESIZE_VGA);
}

// ---- concurrency ----
bool camera_lock(uint32_t timeout_ms) {
    if (!g_camMutex) g_camMutex = xSemaphoreCreateMutex();
    return xSemaphoreTake(g_camMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
void camera_unlock() { if (g_camMutex) xSemaphoreGive(g_camMutex); }
bool camera_hq_busy() { return g_hqBusy; }

// ---- resolution switch (caller holds the lock) ----
void camera_set_live_size(framesize_t fs) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return;
    s->set_pixformat(s, PIXFORMAT_YUV422);
    s->set_framesize(s, fs);
}

void camera_discard(int n) {
    for (int i = 0; i < n; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ---- single grab + convert ----
bool camera_grab_jpeg(int quality, uint8_t** out, size_t* outLen) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    *out = nullptr; *outLen = 0;
    bool ok = frame2jpg(fb, (uint8_t)quality, out, outLen);
    esp_camera_fb_return(fb);
    if (!ok && *out) { free(*out); *out = nullptr; }
    return ok;
}

// ---- sharpness: variance of the Laplacian on a downsampled Y plane ----
float camera_sharpness(const uint8_t* yuv, uint16_t w, uint16_t h) {
    if (!yuv || w < 16 || h < 16) return 0.0f;
    int step = (w >= 1024) ? 4 : ((w >= 512) ? 2 : 1);
    int sw = w / step, sh = h / step;
    if (sw < 4 || sh < 4) return 0.0f;
    uint32_t rowBytes = (uint32_t)w * 2;   // YUV422 = 2 bytes/pixel
    double sum = 0.0, sumsq = 0.0;
    long n = 0;
    for (int y = 1; y < sh - 1; y++) {
        uint32_t base = (uint32_t)(y * step) * rowBytes;
        uint32_t offStep = (uint32_t)step * rowBytes;
        for (int x = 1; x < sw - 1; x++) {
            uint32_t off = (uint32_t)(x * step) * 2;
            int c  = yuv[base + off];
            int up = yuv[base - offStep + off];
            int dn = yuv[base + offStep + off];
            int lf = yuv[base + ((uint32_t)(x * step - step)) * 2];
            int rt = yuv[base + ((uint32_t)(x * step + step)) * 2];
            int lap = 4 * c - up - dn - lf - rt;
            sum += lap; sumsq += (double)lap * lap; n++;
        }
    }
    if (n == 0) return 0.0f;
    double mean = sum / (double)n;
    double var  = sumsq / (double)n - mean * mean;
    return (float)(var > 0.0 ? var : 0.0);
}

// ---- best-of-N capture (caller holds the lock, size already set) ----
static bool capture_best_of_n(const CameraSettings& s, uint8_t** out, size_t* outLen) {
    int n = (s.best_of_n < 1) ? 1 : (s.best_of_n > 5 ? 5 : s.best_of_n);
    float bestSharp = -1.0f;
    uint8_t* bestJpg = nullptr;
    size_t bestLen = 0;
    for (int i = 0; i < n; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { vTaskDelay(pdMS_TO_TICKS(40)); continue; }
        float sharp = camera_sharpness(fb->buf, (uint16_t)fb->width, (uint16_t)fb->height);
        if (sharp >= bestSharp) {
            uint8_t* jpg = nullptr; size_t jlen = 0;
            if (frame2jpg(fb, (uint8_t)s.jpeg_quality, &jpg, &jlen)) {
                if (bestJpg) free(bestJpg);
                bestJpg = jpg; bestLen = jlen; bestSharp = sharp;
            }
        }
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(60));   // small gap between shots
    }
    if (!bestJpg) return false;
    *out = bestJpg; *outLen = bestLen;
    Serial.printf("[CAM] best-of-%d: sharpness=%.1f size=%u\n", n, bestSharp, (unsigned)bestLen);
    return true;
}

// ---- simple path: HQ size <= init size (no realloc) ----
static bool capture_simple(framesize_t hq, framesize_t live, const CameraSettings& s,
                           uint8_t** out, size_t* outLen) {
    if (!camera_lock(6000)) { Serial.println("[CAM] HQ: lock timeout"); return false; }
    g_hqBusy = true;
    camera_apply_image_settings(s);
    camera_discard(1);
    bool ok = capture_best_of_n(s, out, outLen);
    g_hqBusy = false;
    camera_unlock();
    return ok;
}

// ---- re-init path: HQ size > init size (UXGA) ----
static bool capture_reinit(framesize_t hq, framesize_t live, const CameraSettings& s,
                           uint8_t** out, size_t* outLen) {
    if (!camera_lock(6000)) { Serial.println("[CAM] HQ(reinit): lock timeout"); return false; }
    g_hqBusy = true;
    esp_camera_deinit();
    if (!camera_init_at(hq)) {
        camera_init_at(FRAMESIZE_XGA); // restore invariant init size
        g_hqBusy = false;
        camera_unlock();
        Serial.println("[CAM] HQ(reinit): UXGA failed, restored");
        return false;
    }
    camera_apply_image_settings(s);
    camera_discard(1);
    bool ok = capture_best_of_n(s, out, outLen);
    // No restore needed: device reboots after HQ capture (stream_task_force_restart)
    g_hqBusy = false;
    camera_unlock();
    return ok;
}

// ---- public HQ capture ----
bool camera_capture_hq(const CameraSettings& s, uint8_t** out, size_t* outLen) {
    *out = nullptr; *outLen = 0;
    framesize_t hq   = s.hq_size;
    framesize_t live = s.live_size;
    // Fallback chain if the requested size fails.
    framesize_t chain[4] = { hq, FRAMESIZE_XGA, FRAMESIZE_SVGA, FRAMESIZE_VGA };
    for (int c = 0; c < 4; c++) {
        framesize_t trySize = chain[c];
        if (c > 0 && trySize == hq) continue;   // avoid retrying the same size
        CameraSettings tmp = s;
        tmp.hq_size = trySize;
        // Use simple path: camera is init'd at UXGA so buffer is large enough.
        // set_framesize() switches sensor output without reallocating the buffer.
        bool ok = capture_simple(trySize, live, tmp, out, outLen);
        if (ok) { Serial.printf("[CAM] HQ OK @%s\n", framesize_name(trySize)); return true; }
        Serial.printf("[CAM] HQ @%s failed, falling back\n", framesize_name(trySize));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

// ---- backend helpers ----
// Read HTTP status line; report whether status is 2xx.
static bool http_ok(WiFiClient& c, uint32_t timeout_ms) {
    c.setTimeout(timeout_ms);   // ensure readStringUntil doesn't block forever
    char buf[64];
    int n = 0;
    uint32_t t0 = millis();
    while ((millis() - t0) < timeout_ms) {
        int ch = c.read();
        if (ch < 0) {
            if (n > 0) break;   // got some data, connection closed
            delay(5);
            continue;
        }
        if (ch == '\n') { buf[n] = '\0'; break; }
        if (ch != '\r' && n < (int)sizeof(buf) - 1) buf[n++] = ch;
    }
    buf[n] = '\0';
    bool ok = (strncmp(buf, "HTTP/", 5) == 0 && strstr(buf, " 20") != nullptr);
    Serial.printf("[NET] http_ok: %s -> %s\n", buf, ok ? "2xx" : "FAIL");
    c.stop();
    return ok;
}

// Multipart upload: POST /api/v1/scan/{scan_id}/image  (device_id + image file)
int camera_upload(const char* host, int port, const char* scan_id,
                  const uint8_t* data, size_t len) {
    if (len == 0 || !data) return 0;

    const char* B = "ESP32CAMBOUNDARY";
    char pre[320], post[64], head[320];
    int body_prefix = snprintf(pre, sizeof(pre),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n",
        B, BACKEND_DEVICE_ID, B);
    int body_suffix = snprintf(post, sizeof(post), "\r\n--%s--\r\n", B);
    if (body_prefix < 0) body_prefix = 0;
    if (body_suffix < 0) body_suffix = 0;
    size_t body_total = (size_t)body_prefix + len + (size_t)body_suffix;

    int hlen = snprintf(head, sizeof(head),
        "POST %s/scan/%s/image HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n\r\n",
        BACKEND_API_BASE, scan_id, host, port, B, (unsigned long)body_total);
    if (hlen < 0) return 0;

    WiFiClient client;
    if (!client.connect(host, port, 5000)) {
        Serial.println("[NET] upload connect fail");
        return 0;
    }
    client.write((const uint8_t*)head, hlen);
    client.write((const uint8_t*)pre, body_prefix);
    client.write(data, len);
    client.write((const uint8_t*)post, body_suffix);

    bool ok = http_ok(client, 15000);
    Serial.printf("[CAM] upload -> %s (%u bytes)\n", ok ? "HTTP 200" : "FAIL", (unsigned)len);
    return ok ? 200 : 0;
}

// Report capture failure: POST /api/v1/scan/{scan_id}/camera-error
int camera_report_error(const char* host, int port, const char* scan_id) {
    char head[256];
    int hlen = snprintf(head, sizeof(head),
        "POST %s/scan/%s/camera-error HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n",
        BACKEND_API_BASE, scan_id, host, port);
    if (hlen < 0) return 0;

    WiFiClient client;
    if (!client.connect(host, port, 5000)) return 0;
    client.write((const uint8_t*)head, hlen);
    return http_ok(client, 5000) ? 200 : 0;
}

// Heartbeat: POST /api/v1/devices/status  (JSON body)
int camera_heartbeat(const char* host, int port, const char* device_id, const char* status) {
    char body[160];
    int blen = snprintf(body, sizeof(body),
        "{\"device_id\":\"%s\",\"device_type\":\"camera\",\"status\":\"%s\"}",
        device_id, status);
    if (blen < 0) return 0;

    char head[256];
    int hlen = snprintf(head, sizeof(head),
        "POST %s/devices/status HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        BACKEND_API_BASE, host, port, blen);
    if (hlen < 0) return 0;

    WiFiClient client;
    if (!client.connect(host, port, 5000)) return 0;
    client.write((const uint8_t*)head, hlen);
    client.write((const uint8_t*)body, blen);
    return http_ok(client, 5000) ? 200 : 0;
}
