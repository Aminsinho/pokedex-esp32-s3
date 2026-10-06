#include "WifiManager.h"
#include <WiFi.h>
#include "app_config.h"

WifiState WifiManager::state_ = WifiState::DISCONNECTED;
uint32_t WifiManager::lastCheck_ = 0;

void WifiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(DISPLAY_DEVICE_ID);
    Serial.printf("[WIFI] connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    state_ = WifiState::CONNECTING;
}

void WifiManager::loop() {
    uint32_t now = millis();
    if (now - lastCheck_ < 1000) return;
    lastCheck_ = now;

    if (WiFi.status() == WL_CONNECTED) {
        if (state_ != WifiState::CONNECTED) {
            state_ = WifiState::CONNECTED;
            Serial.printf("[WIFI] CONNECTED ip=%s\n", WiFi.localIP().toString().c_str());
        }
    } else if (state_ == WifiState::CONNECTED) {
        state_ = WifiState::DISCONNECTED;
        Serial.println("[WIFI] LOST");
    } else {
        state_ = WifiState::CONNECTING;
    }
}
