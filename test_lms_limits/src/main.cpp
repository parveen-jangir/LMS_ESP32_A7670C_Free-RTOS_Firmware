#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "SensorManager/SensorManager.h"

// ==================== WiFi Configuration ====================
const char *WIFI_SSID = "ESP32-WebSocket-Test";
const char *WIFI_PASSWORD = "12345678";
const IPAddress local_IP(192, 168, 4, 1);
const IPAddress gateway(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);

// ==================== Server Configuration ====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==================== Global Variables ====================
SensorManager sensorManager;
unsigned long lastBroadcastTime = 0;
const unsigned long BROADCAST_INTERVAL = 5000; // 5 seconds

// ==================== Function Prototypes ====================
void setupWiFi();
void setupWebSocket();
void setupHTTPServer();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len);
void broadcastSensorData();
void broadcastConnectionStatus(const char *status);
void sendJSONToClient(AsyncWebSocketClient *client, const char *type,
                      const char *message, bool includeTimestamp = true);
void sendJSONBroadcast(const char *type, const char *message,
                       bool includeTimestamp = true);
String sensorReadingsToJSON();

void setup()
{
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    delay(2000);

    // Clear screen
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║     ESP32 WebSocket Server with Multi-Sensor Integration       ║");
    Serial.println("║                                                                ║");
    Serial.println("║  WiFi AP Mode: ESP32-WebSocket-Test                            ║");
    Serial.println("║  AP IP: 192.168.4.1                                            ║");
    Serial.println("║  WebSocket: ws://192.168.4.1/ws                                ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");

    // Initialize sensors
    Serial.println("[*] Initializing sensor manager...\n");
    delay(1000);

    if (!sensorManager.initialize())
    {
        Serial.println("[ERROR] Failed to initialize sensors!");
        while (1)
        {
            delay(1000);
        }
    }

    Serial.println("[✓] Sensors initialized\n");

    // Set calibration
    sensorManager.setBMP180CalibrationOffset(0.0, 0.0);
    sensorManager.setBH1750CalibrationOffset(0.0);
    sensorManager.setMPU6050CalibrationOffset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    sensorManager.setDHT22CalibrationOffset(0.0, 0.0);
    sensorManager.setSoilMoistureCalibrationOffset(0, 4095);
    sensorManager.setRainGaugeTipVolume(0.2794);

    // Start sensor reading task
    sensorManager.startReadingTask();

    // Setup WiFi
    setupWiFi();

    // Setup WebSocket
    setupWebSocket();

    // Setup HTTP Server
    setupHTTPServer();

    // Start HTTP server
    server.begin();

    Serial.println("\n[✓] System initialized and ready for connections!\n");
}

void loop()
{
    // Broadcast sensor data every 5 seconds
    if (millis() - lastBroadcastTime >= BROADCAST_INTERVAL)
    {
        broadcastSensorData();
        lastBroadcastTime = millis();
    }

    delay(100);
}

// ==================== WiFi Setup ====================
void setupWiFi()
{
    Serial.println("[*] Setting up WiFi in AP mode...");

    // Configure AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    // Start AP
    if (WiFi.softAP(WIFI_SSID, WIFI_PASSWORD))
    {
        Serial.println("[✓] WiFi AP started successfully");
        Serial.printf("    SSID: %s\n", WIFI_SSID);
        Serial.printf("    Password: %s\n", WIFI_PASSWORD);
        Serial.printf("    IP Address: 192.168.4.1\n");
        Serial.printf("    Connected Clients: %d\n", WiFi.softAPgetStationNum());
    }
    else
    {
        Serial.println("[ERROR] Failed to start WiFi AP!");
    }

    Serial.println();
}

// ==================== WebSocket Setup ====================
void setupWebSocket()
{
    Serial.println("[*] Setting up WebSocket server...");

    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    Serial.println("[✓] WebSocket handler registered at /ws\n");
}

// ==================== HTTP Server Setup ====================
void setupHTTPServer()
{
    Serial.println("[*] Setting up HTTP server...");

    // Root endpoint - return system info
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String response = "ESP32 WebSocket Server with Multi-Sensor Integration\n";
        response += "WebSocket endpoint: ws://192.168.4.1/ws\n";
        response += "Connected clients: ";
        response += String(ws.count());
        request->send(200, "text/plain", response); });

    // API endpoint to get current sensor readings
    server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String jsonResponse = sensorReadingsToJSON();
        request->send(200, "application/json", jsonResponse); });

    // API endpoint to enable/disable sensors
    server.on("/api/sensor/control", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("sensor") && request->hasParam("action")) {
            String sensor = request->getParam("sensor")->value();
            String action = request->getParam("action")->value();
            
            if (action == "enable") {
                sensorManager.enableSensor(sensor.c_str());
                request->send(200, "application/json", "{\"status\":\"enabled\"}");
            } else if (action == "disable") {
                sensorManager.disableSensor(sensor.c_str());
                request->send(200, "application/json", "{\"status\":\"disabled\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid action\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
        } });

    // API endpoint to get sensor status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DynamicJsonDocument doc(512);
        doc["timestamp"] = millis();
        doc["clients_connected"] = ws.count();
        doc["uptime_ms"] = millis();
        doc["free_heap"] = ESP.getFreeHeap();
        
        JsonObject sensors = doc.createNestedObject("sensors");
        sensors["BMP180"] = sensorManager.isSensorEnabled("BMP180");
        sensors["BH1750"] = sensorManager.isSensorEnabled("BH1750");
        sensors["MPU6050"] = sensorManager.isSensorEnabled("MPU6050");
        sensors["DHT22"] = sensorManager.isSensorEnabled("DHT22");
        sensors["SoilMoisture"] = sensorManager.isSensorEnabled("SoilMoisture");
        sensors["RainGauge"] = sensorManager.isSensorEnabled("RainGauge");
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response); });

    Serial.println("[✓] HTTP server configured\n");
}

// ==================== WebSocket Event Handler ====================
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
    {
        Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                      client->remoteIP().toString().c_str());

        // Send welcome message
        DynamicJsonDocument doc(256);
        doc["type"] = "welcome";
        doc["message"] = "Connected to ESP32 Sensor System";
        doc["client_id"] = client->id();
        doc["timestamp"] = millis();

        String response;
        serializeJson(doc, response);
        client->text(response);

        // Broadcast new connection status
        broadcastConnectionStatus("Client connected");
        break;
    }

    case WS_EVT_DISCONNECT:
    {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
        broadcastConnectionStatus("Client disconnected");
        break;
    }

    case WS_EVT_ERROR:
    {
        Serial.printf("[WS] Client #%u error: %s\n", client->id(), (char *)data);
        break;
    }

    case WS_EVT_PONG:
    {
        Serial.printf("[WS] Client #%u pong\n", client->id());
        break;
    }

    case WS_EVT_DATA:
    {
        handleWebSocketMessage(arg, data, len);
        break;
    }
    }
}

// ==================== WebSocket Message Handler ====================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    // Null-terminate the message
    data[len] = 0;

    Serial.printf("[WS] Received message (%u bytes): %s\n", len, (char *)data);

    // Try to parse as JSON
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, (char *)data);

    if (error)
    {
        Serial.printf("[WS] JSON parse error: %s\n", error.c_str());

        // Echo back as plain text if not JSON
        DynamicJsonDocument response(256);
        response["type"] = "echo";
        response["message"] = String((char *)data);
        response["timestamp"] = millis();

        String responseStr;
        serializeJson(response, responseStr);
        ws.textAll(responseStr);
    }
    else
    {
        // Handle JSON message
        const char *messageType = doc["type"] | "unknown";

        Serial.printf("[WS] Message type: %s\n", messageType);

        DynamicJsonDocument response(512);
        response["type"] = "ack";
        response["original_type"] = messageType;
        response["timestamp"] = millis();

        // Handle different message types
        if (strcmp(messageType, "ping") == 0)
        {
            response["type"] = "pong";
            response["message"] = "Pong from ESP32";
        }
        else if (strcmp(messageType, "sensor_data") == 0)
        {
            response["type"] = "sensor_data";

            // Get all sensor readings
            AllSensorReadings readings = sensorManager.getAllReadings();

            JsonObject bmp180 = response.createNestedObject("bmp180");
            bmp180["temperature"] = readings.bmp180.temperature;
            bmp180["pressure"] = readings.bmp180.pressure;
            bmp180["valid"] = readings.bmp180.isValid;

            JsonObject bh1750 = response.createNestedObject("bh1750");
            bh1750["illuminance"] = readings.bh1750.illuminance;
            bh1750["valid"] = readings.bh1750.isValid;

            JsonObject dht22 = response.createNestedObject("dht22");
            dht22["temperature"] = readings.dht22.temperature;
            dht22["humidity"] = readings.dht22.humidity;
            dht22["valid"] = readings.dht22.isValid;

            JsonObject soilMoisture = response.createNestedObject("soil_moisture");
            soilMoisture["raw"] = readings.soilMoisture.rawValue;
            soilMoisture["percentage"] = readings.soilMoisture.percentage;
            soilMoisture["valid"] = readings.soilMoisture.isValid;
        }
        else if (strcmp(messageType, "sensor_control") == 0)
        {
            const char *sensor = doc["sensor"] | "unknown";
            const char *action = doc["action"] | "unknown";

            if (strcmp(action, "enable") == 0)
            {
                sensorManager.enableSensor(sensor);
                response["message"] = String(sensor) + " enabled";
            }
            else if (strcmp(action, "disable") == 0)
            {
                sensorManager.disableSensor(sensor);
                response["message"] = String(sensor) + " disabled";
            }
        }
        else if (strcmp(messageType, "echo") == 0)
        {
            response["type"] = "echo";
            response["message"] = doc["message"] | "No message";
        }
        else if (strcmp(messageType, "system_info") == 0)
        {
            response["type"] = "system_info";
            response["uptime_ms"] = millis();
            response["free_heap"] = ESP.getFreeHeap();
            response["connected_clients"] = ws.count();
        }

        String responseStr;
        serializeJson(response, responseStr);
        ws.textAll(responseStr);
    }
}

// ==================== Broadcast Functions ====================
void broadcastSensorData()
{
    // Get all sensor readings
    AllSensorReadings readings = sensorManager.getAllReadings();

    DynamicJsonDocument doc(1024);
    doc["type"] = "sensor_broadcast";
    doc["timestamp"] = millis();

    // BMP180
    JsonObject bmp180 = doc.createNestedObject("bmp180");
    bmp180["temperature"] = readings.bmp180.temperature;
    bmp180["pressure"] = readings.bmp180.pressure;
    bmp180["valid"] = readings.bmp180.isValid;
    bmp180["errors"] = readings.bmp180.errorCount;

    // BH1750
    JsonObject bh1750 = doc.createNestedObject("bh1750");
    bh1750["illuminance"] = readings.bh1750.illuminance;
    bh1750["valid"] = readings.bh1750.isValid;
    bh1750["errors"] = readings.bh1750.errorCount;

    // MPU6050
    JsonObject mpu6050 = doc.createNestedObject("mpu6050");
    JsonObject accel = mpu6050.createNestedObject("acceleration");
    accel["x"] = readings.mpu6050.accelX;
    accel["y"] = readings.mpu6050.accelY;
    accel["z"] = readings.mpu6050.accelZ;
    JsonObject gyro = mpu6050.createNestedObject("gyroscope");
    gyro["x"] = readings.mpu6050.gyroX;
    gyro["y"] = readings.mpu6050.gyroY;
    gyro["z"] = readings.mpu6050.gyroZ;
    mpu6050["temperature"] = readings.mpu6050.temperature;
    mpu6050["valid"] = readings.mpu6050.isValid;
    mpu6050["errors"] = readings.mpu6050.errorCount;

    // DHT22
    JsonObject dht22 = doc.createNestedObject("dht22");
    dht22["temperature"] = readings.dht22.temperature;
    dht22["humidity"] = readings.dht22.humidity;
    dht22["valid"] = readings.dht22.isValid;
    dht22["errors"] = readings.dht22.errorCount;

    // Soil Moisture
    JsonObject soilMoisture = doc.createNestedObject("soil_moisture");
    soilMoisture["raw"] = readings.soilMoisture.rawValue;
    soilMoisture["percentage"] = readings.soilMoisture.percentage;
    soilMoisture["valid"] = readings.soilMoisture.isValid;
    soilMoisture["errors"] = readings.soilMoisture.errorCount;

    // Rain Gauge
    JsonObject rainGauge = doc.createNestedObject("rain_gauge");
    rainGauge["tips"] = readings.rainGauge.tipCount;
    rainGauge["total_rainfall"] = readings.rainGauge.totalRainfall;
    rainGauge["valid"] = readings.rainGauge.isValid;

    String message;
    serializeJson(doc, message);

    // Broadcast to all connected clients
    ws.textAll(message);

    // Print to serial for debugging
    Serial.printf("[BROADCAST] Sent sensor data to %u clients\n", ws.count());
}

void broadcastConnectionStatus(const char *status)
{
    DynamicJsonDocument doc(256);
    doc["type"] = "status";
    doc["message"] = status;
    doc["timestamp"] = millis();
    doc["connected_clients"] = ws.count();

    String message;
    serializeJson(doc, message);
    ws.textAll(message);
}

String sensorReadingsToJSON()
{
    AllSensorReadings readings = sensorManager.getAllReadings();

    DynamicJsonDocument doc(1024);

    JsonObject bmp180 = doc.createNestedObject("bmp180");
    bmp180["temperature"] = readings.bmp180.temperature;
    bmp180["pressure"] = readings.bmp180.pressure;
    bmp180["valid"] = readings.bmp180.isValid;

    JsonObject bh1750 = doc.createNestedObject("bh1750");
    bh1750["illuminance"] = readings.bh1750.illuminance;
    bh1750["valid"] = readings.bh1750.isValid;

    JsonObject mpu6050 = doc.createNestedObject("mpu6050");
    JsonObject accel = mpu6050.createNestedObject("acceleration");
    accel["x"] = readings.mpu6050.accelX;
    accel["y"] = readings.mpu6050.accelY;
    accel["z"] = readings.mpu6050.accelZ;
    JsonObject gyro = mpu6050.createNestedObject("gyroscope");
    gyro["x"] = readings.mpu6050.gyroX;
    gyro["y"] = readings.mpu6050.gyroY;
    gyro["z"] = readings.mpu6050.gyroZ;
    mpu6050["temperature"] = readings.mpu6050.temperature;

    JsonObject dht22 = doc.createNestedObject("dht22");
    dht22["temperature"] = readings.dht22.temperature;
    dht22["humidity"] = readings.dht22.humidity;

    JsonObject soilMoisture = doc.createNestedObject("soil_moisture");
    soilMoisture["raw"] = readings.soilMoisture.rawValue;
    soilMoisture["percentage"] = readings.soilMoisture.percentage;

    JsonObject rainGauge = doc.createNestedObject("rain_gauge");
    rainGauge["tips"] = readings.rainGauge.tipCount;
    rainGauge["total_rainfall"] = readings.rainGauge.totalRainfall;

    String response;
    serializeJson(doc, response);
    return response;
}