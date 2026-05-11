// ============================================================
// websocket_handler.cpp — WebSocket server & protocol engine
// ============================================================

#include "websocket_handler.h"
#include "sensor_manager.h"
#include "config.h"

#include <ArduinoJson.h>
#include <WiFi.h>

// ── Global instance ───────────────────────────────────────────
WebSocketHandler wsHandler;

// Static pointer for the event trampoline
static WebSocketHandler* _instance = nullptr;

// ─────────────────────────────────────────────────────────────
WebSocketHandler::WebSocketHandler()
    : _ws(WS_PORT),
      _clientCount(0),
      _lastDataBroadcast(0),
      _lastHealthBroadcast(0) {}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::begin() {
    _instance = this;
    _ws.begin();
    _ws.onEvent(_onEvent);
    Serial.printf("[WS] Server started on port %d\n", WS_PORT);
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::update() {
    _ws.loop();   // Must be called every loop()

    uint32_t now = millis();

    // Broadcast sensor data at configured interval
    if (_clientCount > 0 &&
        now - _lastDataBroadcast >= SENSOR_UPDATE_MS) {
        _lastDataBroadcast = now;
        _broadcastSensorData();
    }

    // Broadcast health/status at longer interval
    if (_clientCount > 0 &&
        now - _lastHealthBroadcast >= HEALTH_UPDATE_MS) {
        _lastHealthBroadcast = now;
        _broadcastHealth();
    }
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::broadcastJson(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    _ws.broadcastTXT(out);
}

void WebSocketHandler::sendJson(uint8_t clientNum, const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    _ws.sendTXT(clientNum, out);
}

// ─────────────────────────────────────────────────────────────
// Static trampoline → member function
void WebSocketHandler::_onEvent(uint8_t num, WStype_t type,
                                uint8_t* payload, size_t length) {
    if (_instance) {
        _instance->_handleEvent(num, type, payload, length);
    }
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_handleEvent(uint8_t num, WStype_t type,
                                    uint8_t* payload, size_t length) {
    switch (type) {
        // ── Client connected ──────────────────────────────────
        case WStype_CONNECTED: {
            IPAddress ip = _ws.remoteIP(num);
            Serial.printf("[WS] Client #%d connected from %s\n",
                          num, ip.toString().c_str());
            _clientCount++;
            _sendHandshake(num);
            break;
        }
        // ── Client disconnected ───────────────────────────────
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%d disconnected\n", num);
            if (_clientCount > 0) _clientCount--;
            break;

        // ── Text message received ─────────────────────────────
        case WStype_TEXT: {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, payload, length);
            if (err) {
                Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
                _sendError(num, "Malformed JSON");
                return;
            }
            _handleCommand(num, doc);
            break;
        }

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_handleCommand(uint8_t clientNum,
                                      const JsonDocument& doc) {
    const char* type = doc["type"] | "";

    if (strcmp(type, MSG_CMD_ENABLE) == 0 ||
        strcmp(type, MSG_CMD_DISABLE) == 0) {

        const char* sensorName = doc["sensor"] | "";
        bool enable = (strcmp(type, MSG_CMD_ENABLE) == 0);

        // Map sensor name string → SensorID
        SensorID id = SENSOR_COUNT; // invalid default
        if      (strcmp(sensorName, "BMP180")  == 0) id = SENSOR_BMP180;
        else if (strcmp(sensorName, "BH1750")  == 0) id = SENSOR_BH1750;
        else if (strcmp(sensorName, "MPU6050") == 0) id = SENSOR_MPU6050;
        else if (strcmp(sensorName, "DHT22")   == 0) id = SENSOR_DHT22;
        else if (strcmp(sensorName, "SOIL")    == 0) id = SENSOR_SOIL;

        if (id == SENSOR_COUNT) {
            _sendError(clientNum, "Unknown sensor name");
            return;
        }

        sensorManager.setEnabled(id, enable);

        // Echo updated status to all clients
        StaticJsonDocument<256> resp;
        resp["type"]    = MSG_SENSOR_STATUS;
        resp["sensor"]  = sensorName;
        resp["enabled"] = enable;
        resp["status"]  = enable ? "online" : "offline";
        broadcastJson(resp);

    } else {
        _sendError(clientNum, "Unknown command type");
    }
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_sendHandshake(uint8_t clientNum) {
    StaticJsonDocument<512> doc;
    doc["type"]    = MSG_HANDSHAKE;
    doc["version"] = "1.0";
    doc["esp32_ip"]= WiFi.localIP().toString();
    doc["uptime"]  = millis() / 1000;

    // Include initial sensor status
    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensorManager.getAllStatusJson(sensors);

    sendJson(clientNum, doc);
    Serial.printf("[WS] Handshake sent to client #%d\n", clientNum);
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_sendError(uint8_t clientNum, const char* message) {
    StaticJsonDocument<128> doc;
    doc["type"]    = MSG_ERROR;
    doc["message"] = message;
    sendJson(clientNum, doc);
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_broadcastSensorData() {
    // Send one message per enabled sensor
    const char* sensorNames[] = {
        "BMP180", "BH1750", "MPU6050", "DHT22", "SOIL"
    };

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (!sensorManager.isEnabled((SensorID)i)) continue;
        if (sensorManager.getStatus((SensorID)i) != STATUS_ONLINE) continue;

        StaticJsonDocument<256> doc;
        doc["type"]   = MSG_SENSOR_DATA;
        doc["sensor"] = sensorNames[i];
        doc["ts"]     = millis();

        JsonObject data = doc["data"].to<JsonObject>();
        if (!sensorManager.getSensorDataJson((SensorID)i, data)) continue;

        broadcastJson(doc);
    }
}

// ─────────────────────────────────────────────────────────────
void WebSocketHandler::_broadcastHealth() {
    StaticJsonDocument<512> doc;
    doc["type"]   = MSG_HEALTH;
    doc["uptime"] = millis() / 1000;
    doc["rssi"]   = WiFi.RSSI();
    doc["heap"]   = ESP.getFreeHeap();

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensorManager.getAllStatusJson(sensors);

    broadcastJson(doc);
}
