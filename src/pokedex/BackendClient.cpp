#include "BackendClient.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app_config.h"

static String base_url() {
    return String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT);
}

bool BackendClient::http(const char* method, const char* path,
                         const String& body, String& out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient client;
    client.setTimeout(HTTP_TIMEOUT_MS);

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    String url = base_url() + path;
    if (!http.begin(client, url)) {
        Serial.println("[NET] http.begin failed");
        return false;
    }

    int code;
    if (strcmp(method, "POST") == 0) {
        http.addHeader("Content-Type", "application/json");
        code = http.POST(body);
    } else {
        code = http.GET();
    }

    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    if (ok) {
        out = resp;
    } else {
        Serial.printf("[NET] %s %s -> HTTP %d: %.80s\n",
                      method, path, code, resp.c_str());
    }
    return ok;
}

bool BackendClient::getStatus(String& out) {
    return http("GET", "/api/v1/status", "", out);
}

bool BackendClient::getPokemonList(String& out) {
    return http("GET", "/api/v1/pokemon?limit=5", "", out);
}

bool BackendClient::searchPokemon(const String& query, String& out) {
    // URL-encode the query (basic: replace spaces with +)
    String q = query;
    q.replace(' ', '+');
    String path = "/api/v1/pokemon?search=" + q + "&limit=5";
    return http("GET", path.c_str(), "", out);
}

bool BackendClient::getPokemon(uint16_t id, String& out) {
    char path[48];
    snprintf(path, sizeof(path), "/api/v1/pokemon/%u", id);
    return http("GET", path, "", out);
}

bool BackendClient::requestScan(String& scanIdOut) {
    String body = String("{\"device_id\":\"") + DISPLAY_DEVICE_ID + "\"}";
    String out;
    if (!http("POST", "/api/v1/scan/request", body, out)) return false;

    /* Extract the value of "scan_id" from the JSON response.
     * Response looks like: {"scan_id":"abc123","status":"waiting_camera"} */
    int k = out.indexOf("\"scan_id\"");      /* start of the key (incl. quote) */
    if (k < 0) return false;
    int qv = out.indexOf('"', k + 9);       /* opening quote of the value */
    if (qv < 0) return false;
    int qe = out.indexOf('"', qv + 1);      /* closing quote of the value */
    if (qe < 0) return false;
    scanIdOut = out.substring(qv + 1, qe);
    return scanIdOut.length() > 0;
}

bool BackendClient::getScanStatus(const String& scanId, String& out) {
    String path = String("/api/v1/scan/") + scanId;
    return http("GET", path.c_str(), "", out);
}

bool BackendClient::registerDisplay(const String& ip) {
    String body = "{\"device_id\":\"" + String(DISPLAY_DEVICE_ID) + "\""
                  ",\"device_type\":\"display\",\"status\":\"online\""
                  ",\"ip\":\"" + ip + "\"}";
    String out;
    return http("POST", "/api/v1/devices/status", body, out);
}

bool BackendClient::isCameraOnline() {
    String out;
    if (!http("GET", "/api/v1/devices", "", out)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, out) != DeserializationError::Ok) return false;
    for (JsonObject device : doc["devices"].as<JsonArray>()) {
        if (device["device_id"] == CAMERA_DEVICE_ID)
            return device["status"] == "online";
    }
    return false;
}

bool BackendClient::get(const char* path, String& out) {
    return http("GET", path, "", out);
}

bool BackendClient::getRaw(const char* path, uint8_t* buf, size_t bufSize, size_t* outLen) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!buf || bufSize == 0) return false;

    WiFiClient client;
    client.setTimeout(HTTP_TIMEOUT_MS);

    HTTPClient http;
    String url = base_url() + path;
    if (!http.begin(client, url)) return false;

    int code = http.GET();
    if (code < 200 || code >= 300) {
        http.end();
        Serial.printf("[NET] GET %s -> HTTP %d\n", path, code);
        return false;
    }

    int len = http.getSize();
    if (len <= 0 || len > (int)bufSize) {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t total = 0;
    uint32_t start = millis();
    while (total < (size_t)len) {
        if (millis() - start > HTTP_TIMEOUT_MS) break;
        int available = stream->available();
        if (available > 0) {
            int toRead = min(available, (int)(bufSize - total));
            int n = stream->readBytes(buf + total, toRead);
            if (n <= 0) break;
            total += n;
        } else {
            delay(1);
        }
    }
    http.end();

    if (outLen) *outLen = total;
    return total > 0;
}
