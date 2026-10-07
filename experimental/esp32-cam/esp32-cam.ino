// POKEDEX — ESP32-CAM (GC2145)
// Firmware: high-quality photo capture + MJPEG web preview + backend upload.
//
//  - Live view (VGA) + HQ capture (XGA/UXGA) with best-of-N sharpness.
//  - Web UI:   http://<ip>/           (controls + live view + capture)
//  - Stream:   http://<ip>:81/stream
//  - Backend:  GET /api/v1/camera/pending -> capture -> POST /scan/{id}/image
//
// See camera_config.h for all configuration.

#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "camera_config.h"
#include "camera.h"
#include "camera_web.h"

static uint32_t g_lastPoll = 0;
static uint32_t g_lastHb   = 0;

// ---- capture task (isolated from loop so we can timeout) ----
struct CaptureResult {
    uint8_t* data;
    size_t   len;
    bool     done;
    bool     ok;
};
static CaptureResult g_cap;
static SemaphoreHandle_t g_capSem = nullptr;
static TaskHandle_t  g_capTask  = nullptr;

void capture_task_fn(void*) {
    CameraSettings s = camera_current_settings();
    uint8_t* data = nullptr;
    size_t len = 0;
    bool ok = camera_capture_hq(s, &data, &len);
    g_cap.data = data;
    g_cap.len  = len;
    g_cap.ok   = ok;
    g_cap.done = true;
    if (g_capSem) xSemaphoreGive(g_capSem);
    vTaskDelete(nullptr);
}

// --- small HTTP GET helper: returns the full response body ---
// Uses the same low-level pattern as camera_heartbeat() (which works).
static bool http_get(const char* host, int port, const char* path, String& body,
                     uint32_t timeout_ms) {
    char head[256];
    int hlen = snprintf(head, sizeof(head),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n\r\n",
        path, host, port);
    if (hlen < 0) return false;

    WiFiClient c;
    if (!c.connect(host, port, 5000)) return false;
    c.write((const uint8_t*)head, hlen);

    // Use readString() which reliably reads until connection closes or timeout.
    c.setTimeout(timeout_ms);
    String raw = c.readString();
    c.stop();
    if (raw.length() == 0) return false;

    // Strip the HTTP headers (up to the first blank line) so callers get the body.
    int hdrEnd = raw.indexOf("\r\n\r\n");
    if (hdrEnd < 0) hdrEnd = raw.indexOf("\n\n");
    if (hdrEnd >= 0) body = raw.substring(hdrEnd + 4);
    else body = String();  // no body found
    return body.length() > 0;
}

// Extract the scan_id from {"pending":true,"scan_id":"..."}.
static bool extract_scan_id(const String& body, String& scan_id) {
    // FastAPI returns: {"pending": true, "scan_id": "xxx"} (space after colon)
    // Handle both with and without space: "pending":true or "pending": true
    if (body.indexOf("\"pending\": true") < 0 &&
        body.indexOf("\"pending\":true") < 0) return false;
    // Handle both "scan_id":" and "scan_id": "
    int a = body.indexOf("\"scan_id\": \"");
    if (a >= 0) a += strlen("\"scan_id\": \"");
    else {
        a = body.indexOf("\"scan_id\":\"");
        if (a < 0) return false;
        a += strlen("\"scan_id\":\"");
    }
    int b = body.indexOf('"', a);
    if (b <= a) return false;
    scan_id = body.substring(a, b);
    return scan_id.length() > 0;
}

// Poll the backend; if a scan is pending, capture HQ and upload it (multipart).
// Entire operation has a hard timeout of SCAN_TIMEOUT_MS to avoid indefinite blocking.
static void poll_backend() {
    if (millis() - g_lastPoll < POLL_INTERVAL_MS) return;
    g_lastPoll = millis();

    String path = String(BACKEND_API_BASE) + "/camera/pending?device_id=" + BACKEND_DEVICE_ID;
    String body;
    if (!http_get(BACKEND_HOST, BACKEND_PORT, path.c_str(), body, 2500)) return;

    String scan_id;
    if (!extract_scan_id(body, scan_id)) return;   // no pending scan

    Serial.printf("[NET] pending scan: %s -> capturing\n", scan_id.c_str());

    // Run capture in a separate task with 16KB stack so we can timeout it.
    g_cap = {nullptr, 0, false, false};
    if (!g_capSem) g_capSem = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(capture_task_fn, "cap", 16384, nullptr, 1, &g_capTask, 1);
    BaseType_t got = xSemaphoreTake(g_capSem, pdMS_TO_TICKS(10000));  // 10s timeout
    if (got != pdTRUE || !g_cap.done) {
        Serial.println("[NET] capture TIMEOUT -> reporting error + reboot");
        camera_report_error(BACKEND_HOST, BACKEND_PORT, scan_id.c_str());
        delay(500);  // give the HTTP request time to send
        ESP.restart();
    }
    uint8_t* jpg = g_cap.data;
    size_t jlen  = g_cap.len;
    if (!g_cap.ok || !jpg) {
        Serial.println("[NET] HQ capture failed -> reporting error");
        camera_report_error(BACKEND_HOST, BACKEND_PORT, scan_id.c_str());
        if (jpg) free(jpg);
        return;
    }
    if (jlen > MAX_IMAGE_BYTES) {
        Serial.printf("[NET] JPEG %u bytes > max %u -> error\n", (unsigned)jlen, (unsigned)MAX_IMAGE_BYTES);
        free(jpg);
        camera_report_error(BACKEND_HOST, BACKEND_PORT, scan_id.c_str());
        return;
    }
    int code = camera_upload(BACKEND_HOST, BACKEND_PORT, scan_id.c_str(), jpg, jlen);
    free(jpg);
    if (code >= 200 && code < 300) {
        Serial.println("[NET] upload OK");
    } else {
        Serial.printf("[NET] upload FAIL (HTTP %d) -> reporting error\n", code);
        camera_report_error(BACKEND_HOST, BACKEND_PORT, scan_id.c_str());
    }
}

// Periodic heartbeat so the backend keeps the camera online.
static void heartbeat() {
    if (millis() - g_lastHb < HEARTBEAT_MS) return;
    g_lastHb = millis();
    int code = camera_heartbeat(BACKEND_HOST, BACKEND_PORT, BACKEND_DEVICE_ID, "online");
    if (code >= 200 && code < 300) Serial.println("[NET] heartbeat OK");
    else Serial.println("[NET] heartbeat fail (backend offline?)");
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== POKEDEX CAM (GC2145) ===");

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("pokedex-cam");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) { delay(200); Serial.print('.'); }

    if (!camera_init()) {
        Serial.println("[FATAL] camera init failed");
        while (true) delay(1000);
    }

    IPAddress ip = WiFi.localIP();
    Serial.println();
    Serial.println("===== CAMERA TEST MODE =====");
    Serial.printf("Sensor: %s\n", camera_sensor_name());
    Serial.println("XCLK: 20 MHz");
    Serial.printf("PSRAM total: %u KB\n", camera_psram_total());
    Serial.printf("PSRAM libre: %u KB\n", camera_psram_free());
    Serial.printf("heap libre: %u KB\n", camera_heap_free());
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.printf("IP: %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
        Serial.println("Camera UI:");
        Serial.printf("http://%u.%u.%u.%u/\n", ip[0], ip[1], ip[2], ip[3]);
        Serial.println("Stream:");
        Serial.printf("http://%u.%u.%u.%u:%d/stream\n", ip[0], ip[1], ip[2], ip[3], CAM_STREAM_PORT);
    } else {
        Serial.println("WiFi NOT CONNECTED (web UI offline)");
    }
    Serial.println("Backend:");
    Serial.printf("%s:%d\n", BACKEND_HOST, BACKEND_PORT);
    Serial.println("============================");

    camera_web_start();
    g_lastHb = millis();          // send the first heartbeat immediately
    g_lastPoll = millis();        // first poll immediately

    Serial.println("[OK] ready");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t lastRe = 0;
        if (millis() - lastRe > 10000) { lastRe = millis(); WiFi.reconnect(); }
        delay(100);
        return;
    }

    heartbeat();
    poll_backend();
    delay(20);
}
