#include "camera_backend.h"
#include "camera.h"
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

// ---- POST /api/v1/scan/request → returns scan_id ("" on failure) ----
const char* camera_request_scan(const char* host, int port) {
  static char scan_id[64];
  scan_id[0] = '\0';

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, host, port, "/api/v1/scan/request")) {
    Serial.printf("[backend] request_scan: begin failed\n");
    return scan_id;
  }
  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");
  String body = http.getString();
  http.end();

  if (code != 200) {
    Serial.printf("[backend] request_scan: HTTP %d\n", code);
    return scan_id;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body) == DeserializationError::Ok) {
    const char* sid = doc["scan_id"];
    if (sid && strlen(sid) < sizeof(scan_id)) {
      strcpy(scan_id, sid);
    }
  }
  return scan_id;
}

// ---- GET /api/v1/scan/{scan_id} → fills status + result fields ----
// Returns: 0=still processing, 1=completed, 2=error/failed, -1=network error
int camera_get_scan_result(const char* host, int port, const char* scan_id,
                           ScanResult* out) {
  out->status = SCAN_PROCESSING;
  
  char url[128];
  snprintf(url, sizeof(url), "/api/v1/scan/%s", scan_id);

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, host, port, url)) {
    return -1;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String body = http.getString();
  http.end();

  if (code != 200) return -1;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return -1;

  const char* st = doc["status"];
  if (!st) return -1;

  if (strcmp(st, "complete") == 0 || strcmp(st, "completed") == 0) {
    out->status = SCAN_COMPLETED;
    JsonVariant pokemon = doc["pokemon"];
    if (pokemon.is<JsonObject>()) {
      auto po = pokemon.as<JsonObject>();
      out->pokemon_id    = po["id"] | 0;
      const char* nm = po["name"] | "";
      strncpy(out->pokemon_name, nm, sizeof(out->pokemon_name) - 1);
      out->pokemon_name[sizeof(out->pokemon_name) - 1] = '\0';
    }
    out->confidence    = doc["confidence"] | 0.0;
    return 1;
  } else if (strcmp(st, "error") == 0 || strcmp(st, "failed") == 0) {
    out->status = SCAN_FAILED;
    const char* err = doc["error"];
    if (err) snprintf(out->error_msg, sizeof(out->error_msg), "%s", err);
    else strcpy(out->error_msg, "unknown error");
    return 2;
  }
  // "pending_image" or "recognizing" → still processing
  return 0;
}

// ---- Full scan: request → callback(capture) → upload → poll → result ----
int camera_run_scan(const char* host, int port,
                    bool (*capture_fn)(uint8_t** out, size_t* outLen),
                    ScanResult* result) {
  result->status = SCAN_FAILED;
  unsigned long t0 = millis();

  // 1. Request scan from backend
  const char* scan_id = camera_request_scan(host, port);
  if (scan_id[0] == '\0') {
    Serial.printf("[scan] ERROR: failed to create scan\n");
    return -1;
  }
  Serial.printf("[scan] created scan %s\n", scan_id);

  // 2. Capture image
  uint8_t* jpeg = nullptr;
  size_t jpegLen = 0;
  if (!capture_fn(&jpeg, &jpegLen) || !jpeg || jpegLen == 0) {
    Serial.printf("[scan] ERROR: capture failed\n");
    // Report error to backend
    camera_report_error(host, port, scan_id);
    return -1;
  }
  Serial.printf("[scan] captured %u bytes in %lums\n", (unsigned)jpegLen, millis() - t0);

  // 3. Upload to backend
  int upCode = camera_upload(host, port, scan_id, jpeg, jpegLen);
  free(jpeg);
  if (upCode != 200) {
    Serial.printf("[scan] ERROR: upload failed (HTTP %d)\n", upCode);
    return -1;
  }
  Serial.printf("[scan] uploaded OK in %lums\n", millis() - t0);

  // 4. Poll for result (max 60s)
  unsigned long pollStart = millis();
  while (millis() - pollStart < 60000) {
    delay(1000);  // poll every 1s
    int r = camera_get_scan_result(host, port, scan_id, result);
    if (r == 1) {  // completed
      result->status = SCAN_COMPLETED;
      Serial.printf("[scan] RESULT: %s (#%d) conf=%.2f match=%.2f [%lums total]\n",
                    result->pokemon_name, result->pokemon_id,
                    result->confidence, result->match_score, millis() - t0);
      return 1;
    } else if (r == 2) {  // error
      Serial.printf("[scan] BACKEND ERROR: %s\n", result->error_msg);
      return -1;
    }
    // r == 0: still processing, keep polling
  }
  Serial.printf("[scan] TIMEOUT waiting for result\n");
  return -1;
}
