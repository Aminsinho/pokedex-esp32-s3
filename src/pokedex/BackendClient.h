#pragma once
#include <Arduino.h>

/* Thin HTTP wrapper around the local FastAPI backend (plain HTTP, LAN only).
 * Only used from the NetworkTask (core 1) — not thread-safe.
 * All methods return true on HTTP 2xx and fill `out` with the body.
 */
class BackendClient {
public:
    /* GET /api/v1/status -> {"status":"online",...} */
    bool getStatus(String& out);

    /* GET /api/v1/pokemon?limit=151 -> {"total":N,"items":[...]} */
    bool getPokemonList(String& out);

    /* GET /api/v1/pokemon?search=query -> {"total":N,"items":[...]} */
    bool searchPokemon(const String& query, String& out);

    /* GET /api/v1/pokemon/{id} -> detail JSON */
    bool getPokemon(uint16_t id, String& out);

    /* POST /api/v1/scan/request -> {"scan_id":"..."} */
    bool requestScan(String& scanIdOut);

    /* GET /api/v1/scan/{id} -> scan status JSON */
    bool getScanStatus(const String& scanId, String& out);

    /* POST /api/v1/devices/status (registers this display) */
    bool registerDisplay(const String& ip);

    /* GET /api/v1/devices -> camera online? (checks CAMERA_DEVICE_ID) */
    bool isCameraOnline();

/* GET any JSON path -> response body as String */
    bool get(const char* path, String& out);

/* GET binary asset (sprite PNG) into buffer. Returns true on 2xx. */
    bool getRaw(const char* path, uint8_t* buf, size_t bufSize, size_t* outLen);

private:
    bool http(const char* method, const char* path, const String& body, String& out);
};
