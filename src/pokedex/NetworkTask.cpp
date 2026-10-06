#include "NetworkTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "BackendClient.h"
#include "app_config.h"
#include <new>


/* ---------------- queues / store ---------------- */

static QueueHandle_t cmdQ = nullptr;
static QueueHandle_t resQ = nullptr;
static SemaphoreHandle_t storeMutex = nullptr;
static TaskHandle_t taskHandle = nullptr;

struct Store {
    bool backendOnline = false;
    bool cameraOnline = false;
    bool listOk = false;
    int listCount = 0;
    Pokemon list[NetworkTask::MAX_LIST];
    bool detailOk = false;
    uint16_t detailId = 0;
    Pokemon detail;
};
static Store store;

static BackendClient client;  /* task-private */

/* ---------------- public API ---------------- */

void NetworkTask::begin() {
    if (cmdQ) return;
    // FreeRTOS copies bytes: String-containing objects must not be memcpy'd.
    // Queue owning pointers; consumer deletes after copying/processing.
    cmdQ = xQueueCreate(8, sizeof(NetCommand*));
    resQ = xQueueCreate(8, sizeof(NetResult*));
    storeMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(taskMain, "net_task", 16384, nullptr, 1, &taskHandle, 1);
    Serial.println("[NET] task started on core 1");
}

void NetworkTask::post(NetCmd cmd, uint16_t pokemonId, const String& param) {
    if (!cmdQ) return;
    NetCommand* c = new (std::nothrow) NetCommand;
    if (!c) return;
    c->cmd = cmd;
    c->pokemonId = pokemonId;
    c->param = param;
    if (xQueueSend(cmdQ, &c, 0) != pdTRUE) {
        delete c;
        Serial.println("[NET] cmd queue full, dropped");
    }
}

bool NetworkTask::pop(NetResult& out) {
    NetResult* result = nullptr;
    if (!resQ || xQueueReceive(resQ, &result, 0) != pdTRUE) return false;
    out = *result;
    delete result;
    return true;
}

bool NetworkTask::backendOnline() {
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    bool v = store.backendOnline;
    xSemaphoreGive(storeMutex);
    return v;
}

bool NetworkTask::cameraOnline() {
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    bool v = store.cameraOnline;
    xSemaphoreGive(storeMutex);
    return v;
}

bool NetworkTask::copyList(Pokemon* out, int* n) {
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    bool ok = store.listOk;
    int count = ok ? store.listCount : 0;
    for (int i = 0; i < count; i++) out[i] = store.list[i];
    *n = count;
    xSemaphoreGive(storeMutex);
    return ok;
}

bool NetworkTask::copyDetail(Pokemon& out) {
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    bool ok = store.detailOk;
    if (ok) out = store.detail;
    xSemaphoreGive(storeMutex);
    return ok;
}



/* ---------------- task internals ---------------- */

void NetworkTask::pushResult(const NetResult& r) {
    if (!resQ) return;
    NetResult* result = new (std::nothrow) NetResult(r);
    if (!result) return;
    if (xQueueSend(resQ, &result, 0) != pdTRUE) {
        delete result;
        Serial.println("[NET] result queue full, dropped");
    }
}

static void setResult(NetEvt evt, bool ok) {
    NetResult r;
    r.evt = evt;
    r.ok = ok;
    NetworkTask::pushResult(r);
}

static void runStatus() {
    String out;
    bool ok = client.getStatus(out);
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    store.backendOnline = ok;
    xSemaphoreGive(storeMutex);
    setResult(NetEvt::STATUS, ok);
}

static void runList() {
    Serial.printf("[NET] runList: WiFi status=%d\n", (int)WiFi.status());
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NET] runList: WiFi NOT connected, aborting");
        setResult(NetEvt::LIST, false);
        return;
    }
    /* RAW TCP test - bypass HTTPClient entirely */
    {
        WiFiClient raw;
        raw.setTimeout(HTTP_TIMEOUT_MS);
        Serial.print("[NET] RAW connecting...");
        if (!raw.connect(BACKEND_HOST, BACKEND_PORT, 5000)) {
            Serial.println("FAILED");
            setResult(NetEvt::LIST, false);
            return;
        }
        Serial.println(" OK");
        raw.println("GET /api/v1/pokemon?limit=50 HTTP/1.1");
        raw.print("Host: "); raw.print(BACKEND_HOST); raw.print(":"); raw.println(BACKEND_PORT);
        raw.println("Connection: close");
        raw.println();
        Serial.println("[NET] RAW sent request, reading...");
        delay(500);  // wait for response
        String resp;
        uint8_t buf[256];
        uint32_t t0 = millis();
        uint32_t lastDataTime = millis();
        while (millis() - t0 < 8000) {
            int avail = raw.available();
            if (avail > 0) {
                int n = raw.read(buf, (size_t)(avail < 255 ? avail : 255));
                if (n > 0) { resp.concat((const char*)buf, n); lastDataTime = millis(); }
            } else {
                if (resp.length() > 0 && millis() - lastDataTime > 300) break; // 300ms no data = done
                delay(5);
            }
        }
        raw.stop();
        int hdrEnd = resp.indexOf("\r\n\r\n");
        if (hdrEnd >= 0) {
            String body = resp.substring(hdrEnd + 4);
            /* Parse this body */
            String out = body;
            bool ok = true;
            int n = 0;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, out);
            if (err == DeserializationError::Ok) {
                JsonArray items = doc["items"];
                for (JsonObject it : items) {
                    if (n >= NetworkTask::MAX_LIST) break;
                    Pokemon& p = store.list[n];
                    p.id = (uint16_t)(it["id"] | 0);
                    p.name = it["name"] | "";
                    p.type_count = 0;
                    JsonArray types = it["types"];
                    for (const char* t : types) {
                        if (p.type_count >= 3) break;
                        p.types[p.type_count].name = t;
                        p.types[p.type_count].color = typeColor(p.types[p.type_count].name);
                        p.type_count++;
                    }
                    n++;
                }
            } else {
                Serial.printf("[NET] LIST parse err=%s\n", err.c_str());
                ok = false;
            }
            Serial.printf("[NET] LIST ok=%d n=%d\n", (int)ok, n);
            xSemaphoreTake(storeMutex, portMAX_DELAY);
            store.listOk = ok;
            store.listCount = ok ? n : 0;
            xSemaphoreGive(storeMutex);
            setResult(NetEvt::LIST, ok);
            return;
        } else {
            Serial.println("[NET] LIST no header in response");
        }
    }
    /* Fallback to normal path */
    String out;
    bool ok = client.getPokemonList(out);
    Serial.printf("[NET] runList: http ok=%d resp_len=%d\n", (int)ok, (int)out.length());
    int n = 0;
    if (ok) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, out);
        if (err == DeserializationError::Ok) {
            Serial.printf("[NET] runList: JSON parse OK\n");
            JsonArray items = doc["items"];
            for (JsonObject it : items) {
                if (n >= NetworkTask::MAX_LIST) break;
                Pokemon& p = store.list[n];
                p.id = (uint16_t)(it["id"] | 0);
                p.name = it["name"] | "";
                p.type_count = 0;
                JsonArray types = it["types"];
                for (const char* t : types) {
                    if (p.type_count >= 3) break;
                    p.types[p.type_count].name = t;
                    p.types[p.type_count].color = typeColor(p.types[p.type_count].name);
                    p.type_count++;
                }
                n++;
            }
        } else {
            Serial.printf("[NET] runList: JSON parse FAILED: %s\n", err.c_str());
            ok = false;
        }
    }
    Serial.printf("[NET] runList: final ok=%d count=%d\n", (int)ok, n);
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    store.listOk = ok;
    store.listCount = ok ? n : 0;
    xSemaphoreGive(storeMutex);
    setResult(NetEvt::LIST, ok);
}

static void runDetail(uint16_t id) {
    String out;
    bool ok = client.getPokemon(id, out);
    bool parsed = false;
    if (ok) {
        JsonDocument doc;
        if (deserializeJson(doc, out) == DeserializationError::Ok) {
            store.detail.id = (uint16_t)(doc["id"] | id);
            store.detail.name = doc["name"] | "";
            store.detail.type_count = 0;
            JsonArray types = doc["types"];
            for (const char* t : types) {
                if (store.detail.type_count >= 3) break;
                store.detail.types[store.detail.type_count].name = t;
                store.detail.types[store.detail.type_count].color =
                    typeColor(store.detail.types[store.detail.type_count].name);
                store.detail.type_count++;
            }
            store.detail.height_m = doc["height"] | 0.0f;
            store.detail.weight_kg = doc["weight"] | 0.0f;
            store.detail.base_experience = (uint8_t)(doc["base_experience"] | 0);
            store.detail.generation = (uint8_t)(doc["generation"] | 1);
            store.detail.stats.hp = doc["stats"]["hp"] | 0;
            store.detail.stats.attack = doc["stats"]["attack"] | 0;
            store.detail.stats.defense = doc["stats"]["defense"] | 0;
            store.detail.stats.special_attack = doc["stats"]["special_attack"] | 0;
            store.detail.stats.special_defense = doc["stats"]["special_defense"] | 0;
            store.detail.stats.speed = doc["stats"]["speed"] | 0;
            // Prefer description_es, fallback to description_en
            if (doc["description_es"] && strlen(doc["description_es"]) > 0)
                store.detail.description = String(doc["description_es"]);
            else
                store.detail.description = String(doc["description_en"] | doc["description"] | "");
            parsed = true;
        }
    }
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    store.detailOk = parsed;
    store.detailId = store.detail.id;
    xSemaphoreGive(storeMutex);
    setResult(NetEvt::DETAIL, parsed);
}

static void runRequestScan() {
    String scanId;
    bool ok = client.requestScan(scanId);
    NetResult r;
    r.evt = NetEvt::SCAN_CREATED;
    r.ok = ok;
    r.data = scanId;
    NetworkTask::pushResult(r);
}

static void runSearch(const String& query) {
    String out;
    bool ok = client.searchPokemon(query, out);
    NetResult r;
    r.evt = NetEvt::SEARCH;
    r.ok = false;
    if (ok) {
        JsonDocument doc;
        if (deserializeJson(doc, out) == DeserializationError::Ok) {
            int total = doc["total"] | 0;
            if (total > 0) {
                JsonArray items = doc["items"];
                if (items.size() > 0) {
                    JsonObject it = items[0];
                    r.ok = true;
                    r.pokemonId = (uint16_t)(it["id"] | 0);
                    r.name = it["name"] | "";
                }
            }
        }
    }
    NetworkTask::pushResult(r);
}

static void runGetScan(const String& scanId) {
    String out;
    bool ok = client.getScanStatus(scanId, out);
    NetResult r;
    r.evt = NetEvt::SCAN_STATUS;
    r.scanId = scanId;
    r.ok = ok;
    if (ok) {
        JsonDocument doc;
        if (deserializeJson(doc, out) == DeserializationError::Ok) {
            r.data = doc["status"] | "unknown";
            if (r.data == "error") r.name = doc["error"] | "scan_failed";
            if (doc["status"] == "complete") {
                r.pokemonId = (uint16_t)(doc["pokemon"]["id"] | 0);
                r.name = doc["pokemon"]["name"] | "";
                r.confidence = doc["confidence"] | 0.0f;
            }
        } else {
            r.data = "bad_response";
            r.ok = false;
        }
    }
    NetworkTask::pushResult(r);
}

static void runRegister() {
    String ip = WiFi.localIP().toString();
    bool ok = client.registerDisplay(ip);
    setResult(NetEvt::REGISTERED, ok);
}

static void runCheckCamera() {
    bool online = client.isCameraOnline();
    xSemaphoreTake(storeMutex, portMAX_DELAY);
    store.cameraOnline = online;
    xSemaphoreGive(storeMutex);
    setResult(NetEvt::CAMERA, online);
}

void NetworkTask::runCommand(const NetCommand& c) {
    switch (c.cmd) {
        case NetCmd::CHECK_STATUS: runStatus(); break;
        case NetCmd::GET_LIST:     runList(); break;
        case NetCmd::GET_DETAIL:   runDetail(c.pokemonId); break;
        case NetCmd::REQUEST_SCAN: runRequestScan(); break;
        case NetCmd::GET_SCAN:     runGetScan(c.param); break;
        case NetCmd::REGISTER:     runRegister(); break;
        case NetCmd::CHECK_CAMERA: runCheckCamera(); break;
        case NetCmd::SEARCH:       runSearch(c.param); break;
    }
}

void NetworkTask::taskMain(void* param) {
    (void)param;
    for (;;) {
        NetCommand* c = nullptr;
        if (xQueueReceive(cmdQ, &c, pdMS_TO_TICKS(1000)) == pdTRUE) {
            runCommand(*c);
            delete c;
        }
    }
}
