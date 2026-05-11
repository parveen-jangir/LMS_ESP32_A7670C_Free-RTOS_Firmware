#pragma once
// ============================================================
// websocket_handler.h — WebSocket server management
// Protocol: JSON messages over ws://ESP32_IP:81
// ============================================================

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ── Message type strings (protocol constants) ─────────────────
#define MSG_HANDSHAKE       "handshake"
#define MSG_SENSOR_DATA     "sensor_data"
#define MSG_SENSOR_STATUS   "sensor_status"
#define MSG_CMD_ENABLE      "cmd_enable"
#define MSG_CMD_DISABLE     "cmd_disable"
#define MSG_ERROR           "error"
#define MSG_HEALTH          "health"

// ============================================================
class WebSocketHandler {
public:
    WebSocketHandler();

    // Call once in setup()
    void begin();

    // Call every loop() — dispatches events, triggers broadcasts
    void update();

    // Broadcast a JSON string to all connected clients
    void broadcastJson(const JsonDocument& doc);

    // Send to a specific client
    void sendJson(uint8_t clientNum, const JsonDocument& doc);

    // Number of currently connected clients
    uint8_t clientCount() const { return _clientCount; }

private:
    WebSocketsServer _ws;
    uint8_t          _clientCount;
    uint32_t         _lastDataBroadcast;
    uint32_t         _lastHealthBroadcast;

    // WebSocket event callback (static trampoline → member)
    static void _onEvent(uint8_t num, WStype_t type,
                         uint8_t* payload, size_t length);
    void _handleEvent(uint8_t num, WStype_t type,
                      uint8_t* payload, size_t length);

    // Message handlers
    void _handleCommand(uint8_t clientNum, const JsonDocument& doc);
    void _sendHandshake(uint8_t clientNum);
    void _sendError(uint8_t clientNum, const char* message);

    // Periodic broadcasts
    void _broadcastSensorData();
    void _broadcastHealth();
};

// Global instance
extern WebSocketHandler wsHandler;
