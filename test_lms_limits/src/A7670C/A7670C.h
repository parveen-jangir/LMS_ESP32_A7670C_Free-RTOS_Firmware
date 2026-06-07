#pragma once
/**
 * @file    A7670C.h
 * @brief   Full-featured Arduino C++ library for the SIM A7670C LTE module on ESP32
 *
 * Features:
 *  - Module power / reset via PWRKEY pin
 *  - AT command engine with timeout & callback
 *  - SMS send / receive (text mode)
 *  - Voice call make / answer / hang-up + ring detection
 *  - HTTP/HTTPS GET & POST (hit any REST API)
 *  - MQTT client using built-in AT+MQTT firmware commands
 *  - Mobile data (PDP context) ON / OFF
 *  - Signal strength / quality (AT+CSQ, AT+CPSI)
 *
 * Tested on: ESP32 + A7670C at 115200 baud, HardwareSerial (Serial1/Serial2)
 *
 * Author : Your Name
 * License: MIT
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

// ─── Constants ────────────────────────────────────────────────────────────────
#define A7670C_DEFAULT_BAUD       115200
#define A7670C_CMD_TIMEOUT_MS     5000
#define A7670C_HTTP_TIMEOUT_MS    30000
#define A7670C_MQTT_TIMEOUT_MS    10000
#define A7670C_RX_BUFFER_SIZE     2048
#define A7670C_MAX_SMS_LEN        160
#define A7670C_MAX_TOPIC_LEN      128
#define A7670C_MAX_PAYLOAD_LEN    512

// ─── Result codes ─────────────────────────────────────────────────────────────
enum class A7670C_Result {
    OK = 0,
    ERROR,
    TIMEOUT,
    NO_CARRIER,
    BUSY,
    NO_DIALTONE,
    CME_ERROR,
    NOT_INITIALIZED,
    HTTP_ERROR,
    MQTT_ERROR,
    PARSE_ERROR
};

// ─── Network registration states ──────────────────────────────────────────────
enum class RegStatus {
    NOT_REGISTERED = 0,
    REGISTERED_HOME,
    SEARCHING,
    DENIED,
    UNKNOWN,
    ROAMING
};

// ─── HTTP methods ─────────────────────────────────────────────────────────────
enum class HttpMethod { GET, POST, PUT, DELETE_ };

// ─── Structs ──────────────────────────────────────────────────────────────────
struct SignalInfo {
    int  rssi;        // Received Signal Strength Indicator (dBm)
    int  ber;         // Bit Error Rate (0-7, 99=unknown)
    int  csq;         // Raw CSQ value (0-31, 99=unknown)
};

struct SmsMessage {
    String index;
    String status;    // "REC UNREAD", "REC READ", etc.
    String sender;
    String timestamp;
    String text;
};

struct HttpResponse {
    int    statusCode;
    String body;
    bool   success;
};

// ─── Callback types ───────────────────────────────────────────────────────────
typedef void (*SmsCallback)(SmsMessage msg);
typedef void (*MqttMessageCallback)(const String& topic, const String& payload);
typedef void (*RingCallback)(const String& callerNumber);
typedef void (*AtUnsolicitedCallback)(const String& line);

// ─── Main class ───────────────────────────────────────────────────────────────
class A7670C {
public:
    // ── Construction ──────────────────────────────────────────────────────────
    /**
     * @param serial    HardwareSerial instance (Serial1 or Serial2)
     * @param rxPin     ESP32 RX GPIO connected to A7670C TX
     * @param txPin     ESP32 TX GPIO connected to A7670C RX
     * @param pwrPin    GPIO connected to A7670C PWRKEY (active LOW pulse)
     * @param baud      UART baud rate (default 115200)
     */
    A7670C(HardwareSerial &serial, int pwrPin);

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool         begin();                   // Init serial, power on, wait ready
    void         end();                     // Power off gracefully
    void         loop();                    // Call from Arduino loop() – parses URC

    bool         powerOn();                 // Assert PWRKEY pulse to turn on
    bool         powerOff();                // Graceful power off via AT+CPOWD
    bool         reset();                   // PWRKEY pulse reset
    bool         isAlive();                 // AT ping → OK?
    bool         waitReady(uint32_t timeoutMs = 15000);

    // ── Module info ───────────────────────────────────────────────────────────
    String       getIMEI();
    String       getICCID();
    String       getOperator();
    String       getFirmwareVersion();
    RegStatus    getNetworkStatus();
    String       waitForHTTPAction(const String& cmd, uint32_t timeoutMs);

    // ── Signal ──────────────────────────────────────────────────────────────────
    SignalInfo   getSignalStrength();       // AT+CSQ

    // ── Internet (PDP context) ─────────────────────────────────────────────────
    /**
     * @param apn  Carrier APN string e.g. "airtelgprs.com"
     */
    A7670C_Result enableInternet(const String& apn = "");
    A7670C_Result disableInternet();
    bool          isInternetEnabled();

    // ── HTTP / REST API ───────────────────────────────────────────────────────
    /**
     * Perform an HTTP(S) request.
     * @param method    GET, POST, PUT, DELETE_
     * @param url       Full URL including https://
     * @param body      Request body (for POST/PUT); empty for GET
     * @param contentType  MIME type, default "application/json"
     * @param response  Output struct filled with status + body
     */
    A7670C_Result httpRequest(HttpMethod   method,
                              const String& url,
                              const String& body,
                              const String& contentType,
                              HttpResponse& response);

    // Convenience wrappers
    HttpResponse httpGet(const String& url);
    HttpResponse httpPost(const String& url,
                          const String& jsonBody,
                          const String& contentType = "application/json");

    // ── MQTT ──────────────────────────────────────────────────────────────────
    /**
     * Configure MQTT client (call once before connect).
     * @param clientId  Unique MQTT client ID
     * @param host      Broker hostname / IP
     * @param port      Broker port (default 1883; use 8883 for TLS)
     * @param user      Username (empty = anonymous)
     * @param pass      Password
     */
    A7670C_Result mqttConfigure(const String& clientId,
                                const String& host,
                                uint16_t      port     = 1883,
                                const String& user     = "",
                                const String& pass     = "");

    A7670C_Result mqttConnect();
    A7670C_Result mqttDisconnect();
    bool          mqttIsConnected();

    A7670C_Result mqttPublish(const String& topic,
                              const String& payload,
                              uint8_t       qos    = 0,
                              bool          retain = false);

    A7670C_Result mqttSubscribe(const String& topic, uint8_t qos = 0);
    A7670C_Result mqttUnsubscribe(const String& topic);

    /** Register callback fired on every inbound MQTT message */
    void          onMqttMessage(MqttMessageCallback cb) { _mqttMsgCb = cb; }

    // ── SMS ───────────────────────────────────────────────────────────────────
    /**
     * Send an SMS in text mode.
     * @param number  Destination in international format e.g. "+919876543210"
     * @param text    Message body (max 160 chars)
     */
    A7670C_Result smsSend(const String& number, const String& text);

    /** Read one SMS by storage index (1-based). */
    SmsMessage    smsRead(int index);

    /** List all stored SMS messages. Returns count; fills array up to maxCount. */
    int           smsList(SmsMessage* msgs, int maxCount, const String& status = "ALL");

    /** Delete SMS at index (0 = all read, 4 = all). */
    A7670C_Result smsDelete(int index, int flag = 0);

    /** Register callback fired when new SMS arrives via +CMTI URC. */
    void          onSms(SmsCallback cb) { _smsCb = cb; }

    // ── Voice calls ───────────────────────────────────────────────────────────
    /**
     * Initiate an outgoing voice call.
     * @param number  E.164 number e.g. "+919876543210"
     */
    A7670C_Result callDial(const String& number);
    A7670C_Result callAnswer();             // ATA
    A7670C_Result callHangUp();             // ATH
    bool          callIsActive();           // AT+CLCC check

    /** Register callback fired when RING / +CLIP URC is received. */
    void          onRing(RingCallback cb)   { _ringCb = cb; }

    // ── Raw AT access ─────────────────────────────────────────────────────────
    /**
     * Send a raw AT command and wait for expected response or timeout.
     * @param cmd        AT command string (without CR/LF)
     * @param expected   Token to treat as success (default "OK")
     * @param timeoutMs  Per-command timeout in ms
     * @returns full response string from modem
     */
    String        sendAT(const String& cmd,
                         const String& expected  = "OK",
                         uint32_t      timeoutMs = A7670C_CMD_TIMEOUT_MS);

    /** True if last sendAT() response contained the expected token. */
    bool          lastOK() const { return _lastOk; }

    /** Raw response of last sendAT() call. */
    const String& lastResponse() const { return _lastResponse; }

    /** Register callback for any unsolicited result codes not handled internally. */
    void          onUnsolicited(AtUnsolicitedCallback cb) { _unsolCb = cb; }

    // ── Debug ─────────────────────────────────────────────────────────────────
    void          setDebug(Stream& dbg) { _dbg = &dbg; }
    void          setDebugEnabled(bool en) { _debugEn = en; }

private:
    // Hardware
    HardwareSerial& _serial;
    int    _rxPin, _txPin, _pwrPin;
    long   _baud;

    // State
    bool   _initialized  = false;
    bool   _internetUp   = false;
    bool   _mqttConnected = false;
    bool   _lastOk        = false;
    String _lastResponse;
    String _apn;

    // MQTT config cache
    String   _mqttClientId;
    String   _mqttHost      =    MQTT_BROKER;
    uint16_t _mqttPort      =    MQTT_PORT;
    String   _mqttUser     =    MQTT_USER;
    String   _mqttPass    =    MQTT_PASS;

    // Callbacks
    SmsCallback             _smsCb     = nullptr;
    MqttMessageCallback     _mqttMsgCb = nullptr;
    RingCallback            _ringCb    = nullptr;
    AtUnsolicitedCallback   _unsolCb   = nullptr;

    // Debug stream
    Stream* _dbg      = nullptr;
    bool    _debugEn  = false;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void   _flushRx();
    String _readUntil(const String& token, uint32_t timeoutMs);
    bool   _waitForToken(const String& token, uint32_t timeoutMs);
    void   _parseURC(const String& line);
    void   _log(const String& msg);

    // HTTP helpers
    A7670C_Result _httpInit();
    A7670C_Result _httpTerminate();

    // SMS helper
    bool   _setSmsTextMode();
};
