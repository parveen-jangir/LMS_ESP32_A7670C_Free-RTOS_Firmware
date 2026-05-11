// ============================================================
// main.cpp — ESP32 Multi-Sensor Hub
// Framework: Arduino / PlatformIO
// ============================================================

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "sensor_manager.h"
#include "websocket_handler.h"

// ── Wi-Fi state ───────────────────────────────────────────────
static uint32_t _lastWifiAttempt = 0;
static bool     _wifiConnected   = false;

// ─────────────────────────────────────────────────────────────
static void wifiConnect() {
    Serial.printf("\n[WiFi] Connecting to '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println("\n[WiFi] Timeout — will retry");
            return;
        }
        delay(500);
        Serial.print(".");
    }
    _wifiConnected = true;
    Serial.printf("\n[WiFi] Connected! IP: %s  RSSI: %d dBm\n",
        WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ─────────────────────────────────────────────────────────────
static void checkWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        return;
    }
    _wifiConnected = false;
    uint32_t now = millis();
    if (now - _lastWifiAttempt < WIFI_RETRY_MS) return;
    _lastWifiAttempt = now;
    Serial.println("[WiFi] Disconnected — attempting reconnect…");
    WiFi.disconnect();
    wifiConnect();
    if (_wifiConnected) {
        // Re-announce IP in case it changed
        Serial.printf("[WiFi] Reconnected! IP: %s\n",
            WiFi.localIP().toString().c_str());
    }
}

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== ESP32 Multi-Sensor Hub ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    // 1. Connect to Wi-Fi
    wifiConnect();

    // 2. Init sensor hardware
    sensorManager.begin();

    // 3. Start WebSocket server (only if Wi-Fi is up)
    if (_wifiConnected) {
        wsHandler.begin();
        Serial.printf("[WS] Connect your browser to ws://%s:%d\n",
            WiFi.localIP().toString().c_str(), WS_PORT);
    }

    Serial.println("[MAIN] Setup complete — entering loop");
}

// ─────────────────────────────────────────────────────────────
void loop() {
    // Wi-Fi watchdog
    checkWifi();

    // Read sensors (non-blocking, millis-gated)
    sensorManager.update();

    // Handle WS events + periodic broadcasts
    if (_wifiConnected) {
        wsHandler.update();
    }

    // Yield to RTOS — avoid WDT resets
    yield();
}
