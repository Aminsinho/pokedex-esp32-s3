#pragma once
#include <Arduino.h>
#include "Pokemon.h"

/* ============================================================
 * NetworkTask
 * FreeRTOS task on core 1 that owns ALL backend I/O.
 *
 *   loop() (core 0)  --NetCommand-->  [cmd queue]  --task (core 1)
 *   task (core 1)    --NetResult-->  [res queue]  --loop() (core 0)
 *
 * Heavy parsed data (list/detail) lives in a mutex-protected store
 * that is copied into PokemonService in the UI context. This keeps
 * all String/heap traffic off the LVGL core except for small copies.
 * ============================================================ */

enum class NetCmd {
    CHECK_STATUS,   /* GET /api/v1/status */
    GET_LIST,       /* GET /api/v1/pokemon */
    GET_DETAIL,     /* GET /api/v1/pokemon/{pokemonId} */
    REQUEST_SCAN,   /* POST /api/v1/scan/request */
    GET_SCAN,       /* GET /api/v1/scan/{scanId} */
    REGISTER,       /* POST /api/v1/devices/status */
    CHECK_CAMERA,   /* GET /api/v1/devices */
    SEARCH          /* GET /api/v1/pokemon?search=... */
};

struct NetCommand {
    NetCmd cmd;
    uint16_t pokemonId = 0;   /* for GET_DETAIL */
    String param;             /* for GET_SCAN / SEARCH query */
};

enum class NetEvt {
    STATUS,        /* ok = backend reachable */
    LIST,          /* ok = list stored */
    DETAIL,        /* ok = detail stored */
    SCAN_CREATED,  /* data = scan_id */
    SCAN_STATUS,   /* data = state string; pokemonId; confidence */
    REGISTERED,    /* ok = accepted */
    CAMERA,        /* ok = camera online */
    SEARCH,        /* ok = found; pokemonId; name */
    ERROR          /* data = error message */
};

struct NetResult {
    NetEvt evt;
    bool ok;
    String data;        /* scan_id / state / error msg */
    String name;        /* matched pokemon name (SCAN_STATUS complete) */
    String scanId;      /* ignore stale scan polls after cancel/retry */
    uint16_t pokemonId = 0;
    float confidence = 0.0f;
};

class NetworkTask {
public:
    static const int MAX_LIST = 50;

    static void begin();                                  /* start the task */
    static void post(NetCmd cmd, uint16_t pokemonId = 0, const String& scanId = "");
    static bool pop(NetResult& out);                      /* non-blocking */

    /* store access (mutex-protected; safe from core 0) */
    static bool backendOnline();
    static bool cameraOnline();
    static bool copyList(Pokemon* out, int* n);
    static bool copyDetail(Pokemon& out);   // last stored detail (results are ordered)
    static void pushResult(const NetResult& r);

private:
    static void taskMain(void* param);
    static void runCommand(const NetCommand& c);
};
