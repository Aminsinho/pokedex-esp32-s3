#include "ScanService.h"
#include "app_config.h"

ScanPhase ScanService::phase_ = ScanPhase::IDLE;
String ScanService::scanId_;
uint16_t ScanService::matchedId_ = 0;
float ScanService::conf_ = 0.0f;
uint32_t ScanService::phaseAt_ = 0;
uint32_t ScanService::lastPoll_ = 0;

void ScanService::setPhase(ScanPhase p) {
    if (phase_ == p) return;
    phase_ = p;
    phaseAt_ = millis();
}

void ScanService::start() {
    if (isActive()) return;
    setPhase(ScanPhase::STARTING);
    scanId_ = "";
    matchedId_ = 0;
    conf_ = 0.0f;
    Serial.println("[SCAN] START");
    NetworkTask::post(NetCmd::REQUEST_SCAN);
}

void ScanService::cancel() {
    setPhase(ScanPhase::IDLE);
    scanId_ = "";
}

void ScanService::onCreated(const String& scanId) {
    if (phase_ != ScanPhase::STARTING) return;
    scanId_ = scanId;
    setPhase(ScanPhase::WAITING_CAMERA);
    lastPoll_ = millis();
    Serial.printf("[SCAN] CREATED id=%s\n", scanId.c_str());
}

void ScanService::onStatus(const String& state, uint16_t id, float conf) {
    if (!isActive() || phase_ == ScanPhase::MATCH || phase_ == ScanPhase::SHOWING) return;
    Serial.printf("[SCAN] STATE %s\n", state.c_str());
    if (state == "waiting_camera") {
        setPhase(ScanPhase::WAITING_CAMERA);
    } else if (state == "capturing") {
        setPhase(ScanPhase::CAPTURING);
    } else if (state == "uploading") {
        setPhase(ScanPhase::UPLOADING);
    } else if (state == "processing") {
        setPhase(ScanPhase::PROCESSING);
    } else if (state == "complete") {
        if (id == 0 || id > 1025) { onError("invalid pokemon id"); return; }
        matchedId_ = id;
        conf_ = conf;
        setPhase(ScanPhase::MATCH);
        Serial.printf("[SCAN] MATCH pokemon=%u conf=%.2f\n", id, conf);
    } else if (state == "error") {
        setPhase(ScanPhase::ERROR);
        Serial.println("[SCAN] ERROR (backend)");
    }
}

void ScanService::onError(const String& msg) {
    Serial.printf("[SCAN] ERROR %s\n", msg.c_str());
    setPhase(ScanPhase::ERROR);
}

void ScanService::tick() {
    uint32_t now = millis();
    if (isActive() && now - phaseAt_ > 160000) {
        onError("scan timeout");
        return;
    }
    switch (phase_) {
        case ScanPhase::MATCH:
            if (now - phaseAt_ > SCAN_MATCH_DELAY_MS) {
                setPhase(ScanPhase::SHOWING);
                // UI bridge opens this ID through the SD-first detail screen.
            }
            break;
        case ScanPhase::WAITING_CAMERA:
        case ScanPhase::CAPTURING:
        case ScanPhase::UPLOADING:
        case ScanPhase::PROCESSING:
            if (now - lastPoll_ > SCAN_POLL_MS) {
                lastPoll_ = now;
                NetworkTask::post(NetCmd::GET_SCAN, 0, scanId_);
            }
            break;
        default:
            break;
    }
}
