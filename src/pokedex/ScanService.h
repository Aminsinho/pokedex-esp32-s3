#pragma once
#include <Arduino.h>
#include "NetworkTask.h"

/* ============================================================
 * ScanService — display-side scan state machine (runs on core 0).
 *
 *   IDLE -> (user taps SCAN) -> REQUESTING -> WAITING_CAMERA
 *        -> CAPTURING -> UPLOADING -> PROCESSING -> MATCH -> SHOWING
 *        (any state -> ERROR)
 *
 * The display polls GET /scan/{id} every SCAN_POLL_MS while the scan
 * is in flight; the mock camera (or real ESP32-CAM) drives the
 * backend state. When COMPLETE, we wait SCAN_MATCH_DELAY_MS with
 * "MATCH FOUND!" then request the detail and hand over to the UI.
 * ============================================================ */

enum class ScanPhase {
    IDLE, STARTING, WAITING_CAMERA, CAPTURING, UPLOADING,
    PROCESSING, MATCH, SHOWING, ERROR
};

class ScanService {
public:
    /* called by UI when user taps SCAN (posts REQUEST_SCAN) */
    static void start();
    static void cancel();

    /* called every loop() iteration (core 0) */
    static void tick();

    static ScanPhase phase() { return phase_; }
    static bool isActive() {
        return phase_ != ScanPhase::IDLE && phase_ != ScanPhase::ERROR;
    }
    static const String& scanId() { return scanId_; }
    static uint16_t matchedId() { return matchedId_; }
    static float confidence() { return conf_; }
    static uint32_t phaseAt() { return phaseAt_; }

    /* results from the network bridge (core 0) */
    static void onCreated(const String& scanId);
    static void onStatus(const String& state, uint16_t id, float conf);
    static void onError(const String& msg);

private:
    static void setPhase(ScanPhase p);
    static ScanPhase phase_;
    static String scanId_;
    static uint16_t matchedId_;
    static float conf_;
    static uint32_t phaseAt_;
    static uint32_t lastPoll_;
};
