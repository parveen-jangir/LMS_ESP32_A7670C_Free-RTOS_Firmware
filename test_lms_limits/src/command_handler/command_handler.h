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
#include "A7670C/A7670C.h"
#include "config.h"

struct commandFormat
{
    size_t len;
    char payload[MAX_PAYLOAD_LEN + 1];
    bool formBle;
    bool formMqtt;
};

class CommandHandler
{
public:
    CommandHandler(SensorManager &sensorMgr, StorageManager &storageMgr, GSM_OTA &gsmOta, A7670C &modem);

    static void mqttCallback(const String &topic, const String &payload);
    static void onSmsReceived(SmsMessage msg);
    static void onIncomingCall(const String& number);

    ~CommandHandler();

    void begin();

    void configSensors();
private:
    SensorManager &sensorMgr;
    StorageManager &storageMgr;
    GSM_OTA &gsmOta;
    A7670C &modem;
    String _deviceMac;
    String _topic;

    // ── FreeRTOS ─────────────────────────────────────────────────────────────
    QueueHandle_t cmdQueue     = nullptr;
    TaskHandle_t  workerHandle = nullptr;

    static void workerTask(void *param);
    static void gsmTask(void *param);
    static void apiTask(void *param);
    
    void        workerLoop();


     // ── Command handlers ─────────────────────────────────────────────────────

    void sendResponse(JsonDocument &response, bool webSoc = false, bool toMqtt = true);
    void dispatch(const commandFormat &cmdPkt);
    void setupMqtt();
    void buildDeviceMac();
    void buildTopic();

    void handleSensorState(JsonDocument &doc, bool fromBle, bool fromMqtt);
    void handleSensorBroadcast(JsonDocument &doc, bool fromBle, bool fromMqtt);
    bool getSystemInfo(JsonDocument &doc, bool fromBle, bool fromMqtt);

    bool enqueue(const char *jsonStr, size_t len, bool fromBle = false);

};