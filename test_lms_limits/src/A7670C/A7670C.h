#pragma once

#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <time.h>
#include "config.h"

struct HttpResponse {
    int  statusCode;
    int  dataLength;
    bool success;
};

class A7670C
{
public:

    struct MQTTMessage
    {
        String topic;
        String payload;
    };

    typedef void (*MqttCallback)(
        const MQTTMessage &msg);

    typedef void (*LineCallback)(
        const String &line);

    typedef void (*HttpActionCallback)(
        const HttpResponse &response);

    typedef std::function<bool(const String &)> LineMatcher;

    A7670C(HardwareSerial &serial, int pwrPin);

    bool begin();

    void sendAT(const String &cmd);

    // Send AT command and wait for a line matching `matcher`.
    // Returns true if a match was received before timeout.
    // Matched line is written to `result`.
    bool sendATWait(
        const String &cmd,
        LineMatcher matcher,
        String &result,
        uint32_t timeoutMs = 2000);

    // ── MQTT ─────────────────────────────────────────────────────
    // Call once before any MQTT operation.
    // Stores credentials; does not send any AT command.
    bool configureMqtt(
        const String &clientId,
        const String &host,
        uint16_t port,
        const String &user,
        const String &pass);

    // Open TCP connection to broker.
    bool mqttConnect();

    bool mqttReconnect();

    // Disconnect from broker and release modem MQTT stack.
    bool mqttDisconnect();

    // Returns true if modem reports connected state.
    bool mqttIsConnected();

    // Subscribe to a topic.
    bool mqttSubscribe(const String &topic);

    bool mqttStart();
    bool mqttStop();

    bool hitHttpGetRequest(const String &url);

    // Publish a payload to a topic.
    bool mqttPublish(
        const String &topic,
        const String &payload);

    bool getHttpInAction()
    {
        return _httpInAction;
    }

    bool readHttpResponse(int dataLength, String &response);

    bool moduleOn();
    bool moduleOff();
    bool moduleReset();
    bool getModuleOn()
    {
        return _isPoweredOn;
    }

    bool getMQTTConnected()
    {
        return _mqttConnected;
    }

    bool moduleInfo();
    bool getSignalStrength(int &rssi);

    bool setTime(String &formatted);

    // ── Event Callbacks ──────────────────────────────────────────
    void onMqttMessage(MqttCallback cb);
    void onSmsReceived(LineCallback cb);
    void onHttpAction(HttpActionCallback cb);
    void onCallEvent(LineCallback cb);
    void onBootEvent(LineCallback cb);
    void onNetworkEvent(LineCallback cb);

private:

    int _pwrPin;
    bool _isPoweredOn;

    // Fixed-size queue item — safe to copy through FreeRTOS queue
    struct QueueItem
    {
        char buf[1024];
    };

    bool _httpInAction;

    struct PendingCmd
    {
        LineMatcher   matcher;
        QueueHandle_t queue;
    };

    HardwareSerial *_serial;

    enum ParserState
    {
        STATE_IDLE,
        STATE_MQTT_READ_TOPIC,
        STATE_MQTT_READ_PAYLOAD
    };

    ParserState   _parserState;
    MQTTMessage   _mqttMsg;
    TaskHandle_t  _rxTaskHandle;
    HttpResponse  _httpResponse;

    // Pending command slot — only one active at a time
    PendingCmd       *_pendingCmd;
    SemaphoreHandle_t _pendingMutex;

    // MQTT config
    String   _mqttClientId;
    String   _mqttHost;
    uint16_t _mqttPort;
    String   _mqttUser;
    String   _mqttPass;
    bool     _mqttConnected;

    // Internal helpers
    bool _sendPromptData(const String &data);
    bool _mqttAcquire();

    static void rxTask(void *param);
    void processLine(String line);

    MqttCallback _mqttCallback;
    LineCallback _smsCallback;
    HttpActionCallback _httpCallback;
    LineCallback _callCallback;
    LineCallback _bootCallback;
    LineCallback _networkCallback;
};