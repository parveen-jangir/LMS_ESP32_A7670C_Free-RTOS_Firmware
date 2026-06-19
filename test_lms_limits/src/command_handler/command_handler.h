#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "SensorManager/SensorManager.h"
#include "local_storage/storage_manager.h"
#include "GSM_OTA/GSM_OTA.h"
#include "A7670C_handler/A7670C.h"
#include "config.h"
#include "DataLogger/DataLogger.h"
#include "power_handler/power_handler.h"
#include "LORA_Handler/LORA_Handler.h"

struct commandFormat
{
    size_t len;
    char payload[MAX_PAYLOAD_LEN + 1];
    bool formBle;
    bool formMqtt;
};

enum class httpActionType
{
    NONE,
    OTA_UPDATE,
    LMS_API,
    LOG_UPLOAD
};

extern time_t getTime(String *formatted);

class CommandHandler
{
public:
    CommandHandler(SensorManager &sensorMgr, StorageManager &storageMgr, GSM_OTA &gsmOta, A7670C &modem, DataLogger &dataLogger, powerMonitor &power);

    void onMqttMessage(const A7670C::MQTTMessage &msg);
    void onHttpAction(const HttpResponse &response);
    void onOtaProgress(int percent, int written, int total);

    ~CommandHandler();

    String getDeviceMac() const
    {
        return _deviceMac;
    }

    String getTopic() const
    {
        return _topic;
    }

    void begin();

    time_t getTimeStr(String &formatted);

    // void configSensors();
    void sendResponse(JsonDocument &response, bool webSoc = false, bool toMqtt = true);
private:
    SensorManager &sensorMgr;
    StorageManager &storageMgr;
    GSM_OTA &gsmOta;
    A7670C &modem;
    DataLogger &logger;
    powerMonitor &power;
    String _deviceMac;
    String _topic;
    String tid = DEFAULT_TID;
    httpActionType _currentHttpAction = httpActionType::NONE;
    HttpResponse _lastHttpResponse;

    // ── FreeRTOS ─────────────────────────────────────────────────────────────
    QueueHandle_t cmdQueue     = nullptr;
    TaskHandle_t  workerHandle = nullptr;
    TaskHandle_t  apiHandle    = nullptr;
    TaskHandle_t  mqttReconnectHandle = nullptr;
    static void workerTask(void *param);
    // static void gsmTask(void *param);
    static void apiTask(void *param);
    static void reconnectMqttTask(void *param);
    
    void        workerLoop();


     // ── Command handlers ─────────────────────────────────────────────────────

    void dispatch(const commandFormat &cmdPkt);
    void setupMqtt();
    void buildDeviceMac();
    void buildTopic();

    void handleSensorState(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSensorBroadcast(JsonDocument &doc, bool fromBle, bool fromMqtt);
    bool getSystemInfo(JsonDocument &doc, bool fromBle, bool fromMqtt);

    bool enqueue(const char *jsonStr, size_t len, bool fromBle = false);

    void handleMqttMessage(const A7670C::MQTTMessage &msg);
    void handleBootEvent(const String &line);
    void handleNetworkEvent(const String &line);
    void handleHttpAction(const HttpResponse &response);
    void handleOtaUpdate(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleBatteryStatus(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSolarStatus(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSystemPower(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSendSMS(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSaveTid(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleResetGsm(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleLogData(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleMpuReset(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleRainReset(JsonDocument &doc, bool fromBle, bool fromMqtt);
    String urlEncodeSpaces(const String &input);
    void handleLoraAlaram(JsonDocument &doc, bool fromBle, bool fromMqtt);

    static CommandHandler *_instance;

    static void mqttCallback(const A7670C::MQTTMessage &msg);
    static void bootCallback(const String &line);
    static void networkCallback(const String &line);
    static void httpCallback(const HttpResponse &response);
    static void movementDetectedCallback(const bool motionDetected);

};