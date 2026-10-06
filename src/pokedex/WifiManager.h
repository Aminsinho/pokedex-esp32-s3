#pragma once
#include <Arduino.h>
#include <WiFi.h>

enum class WifiState { DISCONNECTED, CONNECTING, CONNECTED };

/* STA-mode Wi-Fi manager with auto-reconnect.
 * Call begin() from setup() and loop() from the main loop (1 Hz checks).
 */
class WifiManager {
public:
    static void begin();
    static void loop();
    static bool connected() { return state_ == WifiState::CONNECTED; }
    static WifiState state() { return state_; }
    static String localIP() { return WiFi.localIP().toString(); }
private:
    static WifiState state_;
    static uint32_t lastCheck_;
};
