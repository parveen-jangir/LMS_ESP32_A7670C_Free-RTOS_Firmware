/**
 * @file    A7670C.cpp
 * @brief   Implementation of the A7670C Arduino library for ESP32
 */

#include "A7670C.h"

// ─── Constructor ──────────────────────────────────────────────────────────────
A7670C::A7670C(HardwareSerial &serial,
               int pwrPin)
    : _serial(serial), _pwrPin(pwrPin) {}

// ═══════════════════════════════════════════════════════════════════════════════
//  LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

bool A7670C::begin()
{
    pinMode(_pwrPin, OUTPUT);
    digitalWrite(_pwrPin, LOW); // Idle high

    // _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Try a quick ping first – maybe module is already on
    if (isAlive())
    {
        reset();
        
        while(true){
            SignalInfo info = getSignalStrength();
            if(info.csq != 99){
                _log("[A7670C] Signal strength: " + String(info.rssi) + " dBm");
                break;
            } else {
                _log("[A7670C] Signal strength unknown");
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        _initialized = true;
        _log("[A7670C] Module already on.");
        // Echo off, verbose errors
        sendAT("ATE0");
        sendAT("AT+CMEE=2");
        return true;
    }

    // Power on
    if (!powerOn())
    {
        _log("[A7670C] Power-on failed.");
        return false;
    }

    _initialized = true;
    sendAT("ATE0");      // Echo off
    sendAT("AT+CMEE=2"); // Verbose error codes
    return true;
}

void A7670C::end()
{
    powerOff();
    _serial.end();
    _initialized = false;
}

bool A7670C::powerOn()
{
    _log("[A7670C] Powering on...");
    digitalWrite(_pwrPin, LOW);
    vTaskDelay(1000 / portTICK_PERIOD_MS); // A7670C needs ~1 s pulse
    // digitalWrite(_pwrPin, HIGH);
    return waitReady(15000);
}

bool A7670C::powerOff()
{
    _log("[A7670C] Powering off...");
    digitalWrite(_pwrPin, HIGH);
    sendAT("AT+CPOF", "OK", 5000);
    return true;
}

bool A7670C::reset()
{
    _log("[A7670C] Resetting module...");
    sendAT("AT+CRESET", "OK", 10000);
    return waitReady(20000);
}

bool A7670C::isAlive()
{
    sendAT("AT", "OK", 1000);
    return _lastOk;
}

bool A7670C::waitReady(uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
        if (isAlive())
            return true;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    _log("[A7670C] Timeout waiting for ready.");
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MODULE INFO
// ═══════════════════════════════════════════════════════════════════════════════

String A7670C::getIMEI()
{
    String r = sendAT("AT+CGSN", "OK");
    // Response lines: first non-empty line before OK is the IMEI
    int start = r.indexOf('\n');
    int end = r.lastIndexOf('\n');
    if (start < 0)
        return "";
    String imei = r.substring(start + 1, end);
    imei.trim();
    return imei;
}

String A7670C::getICCID()
{
    String r = sendAT("AT+CCID", "OK");
    int p = r.indexOf("+CCID:");
    if (p < 0)
        return "";
    String s = r.substring(p + 7);
    s.trim();
    s = s.substring(0, s.indexOf('\n'));
    s.trim();
    return s;
}

String A7670C::getOperator()
{
    String r = sendAT("AT+COPS?", "OK");
    int p = r.indexOf("+COPS:");
    if (p < 0)
        return "";
    // Format: +COPS: 0,0,"Operator Name",7
    int q1 = r.indexOf('"', p);
    int q2 = r.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0)
        return "";
    return r.substring(q1 + 1, q2);
}

String A7670C::getFirmwareVersion()
{
    String r = sendAT("AT+CGMR", "OK");
    int start = r.indexOf('\n');
    String v = r.substring(start + 1);
    v = v.substring(0, v.indexOf('\n'));
    v.trim();
    return v;
}

RegStatus A7670C::getNetworkStatus()
{
    String r = sendAT("AT+CREG?", "OK");
    int p = r.indexOf("+CREG:");
    if (p < 0)
        return RegStatus::UNKNOWN;
    // +CREG: <n>,<stat>
    int comma = r.indexOf(',', p);
    if (comma < 0)
    {
        // Short form: +CREG: <stat>
        int colon = r.indexOf(':', p);
        int stat = r.substring(colon + 1).toInt();
        return (RegStatus)stat;
    }
    int stat = r.substring(comma + 1).toInt();
    return (RegStatus)stat;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SIGNAL
// ═══════════════════════════════════════════════════════════════════════════════

SignalInfo A7670C::getSignalStrength()
{
    SignalInfo info = {-113, 99, 99};
    String r = sendAT("AT+CSQ", "OK");
    int p = r.indexOf("+CSQ:");
    if (p < 0)
        return info;
    String vals = r.substring(p + 5);
    vals.trim();
    int comma = vals.indexOf(',');
    info.csq = vals.substring(0, comma).toInt();
    info.ber = vals.substring(comma + 1).toInt();
    // CSQ → dBm: rssi = -113 + 2*csq  (0-31 range; 99 = unknown)
    if (info.csq != 99)
        info.rssi = -113 + 2 * info.csq;
    return info;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  INTERNET (PDP CONTEXT)
// ═══════════════════════════════════════════════════════════════════════════════

A7670C_Result A7670C::enableInternet(const String &apn)
{
    if (!apn.isEmpty())
        _apn = apn;

    // Set APN
    String r = sendAT("AT+CGDCONT=1,\"IP\",\"" + _apn + "\"");
    if (!_lastOk)
        return A7670C_Result::ERROR;

    // Activate PDP context
    r = sendAT("AT+CGACT=1,1", "OK", 15000);
    if (!_lastOk)
        return A7670C_Result::ERROR;

    _internetUp = true;
    _log("[A7670C] Internet enabled.");
    return A7670C_Result::OK;
}

A7670C_Result A7670C::disableInternet()
{
    sendAT("AT+CGACT=0,1", "OK", 10000);
    _internetUp = false;
    _log("[A7670C] Internet disabled.");
    return A7670C_Result::OK;
}

bool A7670C::isInternetEnabled()
{
    String r = sendAT("AT+CGACT?", "OK");
    // +CGACT: 1,1  means context 1 is active
    return r.indexOf("+CGACT: 1,1") >= 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HTTP / REST API
// ═══════════════════════════════════════════════════════════════════════════════

A7670C_Result A7670C::_httpInit()
{
    sendAT("AT+HTTPTERM", "OK", 2000); // Terminate any stale session
    delay(200);
    String r = sendAT("AT+HTTPINIT", "OK", 5000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::HTTP_ERROR;
}

A7670C_Result A7670C::_httpTerminate()
{
    sendAT("AT+HTTPTERM", "OK", 3000);
    return A7670C_Result::OK;
}

String A7670C::waitForHTTPAction(const String &cmd, uint32_t timeoutMs)
{
    _flushRx();
    _log(">> " + cmd);
    _serial.println(cmd);

    String response = "";
    uint32_t startTime = millis();

    while ((millis() - startTime) < timeoutMs)
    {
        while (_serial.available())
        {
            char c = _serial.read();
            response += c;

            // Check if HTTPACTION result has arrived
            if (response.indexOf("+HTTPACTION:") != -1)
            {
                // Wait a little longer to ensure line completion
                delay(100);

                while (_serial.available())
                {
                    response += (char)_serial.read();
                }

                return response;
            }
        }

        delay(10);
    }

    return response; // Return whatever was received on timeout
}

A7670C_Result A7670C::httpRequest(HttpMethod method,
                                  const String &url,
                                  const String &body,
                                  const String &contentType,
                                  HttpResponse &response)
{
    response = {0, "", false};

    if (_httpInit() != A7670C_Result::OK)
        return A7670C_Result::HTTP_ERROR;

    // Set URL
    sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 3000);
    if (!_lastOk)
    {
        _httpTerminate();
        return A7670C_Result::HTTP_ERROR;
    }

    // Content-Type header
    if (!contentType.isEmpty())
        sendAT("AT+HTTPPARA=\"CONTENT\",\"" + contentType + "\"", "OK");

    // User-Agent
    // sendAT("AT+HTTPPARA=\"USERAGENT\",\"A7670C-ESP32-Lib\"", "OK");

    // For POST/PUT: write body data
    if ((method == HttpMethod::POST || method == HttpMethod::PUT) && !body.isEmpty())
    {
        String sizeCmd = "AT+HTTPDATA=" + String(body.length()) + ",10000";
        String r = sendAT(sizeCmd, "DOWNLOAD", 5000);
        if (!_lastOk)
        {
            _httpTerminate();
            return A7670C_Result::HTTP_ERROR;
        }
        // Stream body bytes
        _serial.print(body);
        delay(200);
        _waitForToken("OK", 5000);
    }

    // Execute request
    String actionCmd;
    switch (method)
    {
    case HttpMethod::GET:
        actionCmd = "AT+HTTPACTION=0";
        break;
    case HttpMethod::POST:
        actionCmd = "AT+HTTPACTION=1";
        break;
    case HttpMethod::PUT:
        actionCmd = "AT+HTTPACTION=3";
        break;
    case HttpMethod::DELETE_:
        actionCmd = "AT+HTTPACTION=4";
        break;
    }

    // The module will reply with +HTTPACTION: <method>,<statusCode>,<length>
    // after the HTTP round-trip. We give it HTTP_TIMEOUT.
    String r = waitForHTTPAction(actionCmd, A7670C_HTTP_TIMEOUT_MS);
    Serial.println("HTTPACTION response: " + r);
    if (!_lastOk && r.indexOf("+HTTPACTION:") < 0)
    {
        _httpTerminate();
        return A7670C_Result::HTTP_ERROR;
    }

    // Parse status code
    int p = r.indexOf("+HTTPACTION:");

    if (p >= 0)
    {
        String info = r.substring(p + 12);
        info.trim();
        int c1 = info.indexOf(',');
        int c2 = info.indexOf(',', c1 + 1);
        response.statusCode = info.substring(c1 + 1, c2).toInt();
        int bodyLen = info.substring(c2 + 1).toInt();

        if (bodyLen > 0)
        {
            // Read body
            // String readCmd = "AT+HTTPREAD=0," + String(min(bodyLen, A7670C_MAX_PAYLOAD_LEN));
            // String bodyResp = sendAT(readCmd, "OK", 10000);
            // // Body is between +HTTPREAD: <len>\r\n … and OK
            // int dataStart = bodyResp.indexOf('\n');
            // int dataEnd   = bodyResp.lastIndexOf("\nOK");
            // if (dataStart >= 0 && dataEnd > dataStart)
            //     response.body = bodyResp.substring(dataStart + 1, dataEnd);
            // response.body.trim();
            _flushRx();

            _serial.printf("AT+HTTPREAD=0,%lu\r\n", min(bodyLen, A7670C_MAX_PAYLOAD_LEN));

            // String response;
            uint32_t start = millis();

            while (millis() - start < 15000)
            {
                while (_serial.available())
                {
                    char c = _serial.read();
                    response.body += c;

                    // wait for final OK after payload
                    if (response.body.endsWith("\r\nOK\r\n"))
                    {
                        break;
                    }
                }

                yield();
            }
            response.body.trim();
        }
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
    }

    _httpTerminate();
    return response.success ? A7670C_Result::OK : A7670C_Result::HTTP_ERROR;
}

HttpResponse A7670C::httpGet(const String &url)
{
    HttpResponse resp;
    httpRequest(HttpMethod::GET, url, "", "", resp);
    return resp;
}

HttpResponse A7670C::httpPost(const String &url,
                              const String &jsonBody,
                              const String &contentType)
{
    HttpResponse resp;
    httpRequest(HttpMethod::POST, url, jsonBody, contentType, resp);
    return resp;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MQTT
// ═══════════════════════════════════════════════════════════════════════════════

A7670C_Result A7670C::mqttConfigure(const String &clientId, const String &host, uint16_t port, const String &user, const String &pass)
{
    _mqttClientId = clientId;
    _mqttHost = host;
    _mqttPort = port;
    _mqttUser = user;
    _mqttPass = pass;

    if(mqttIsConnected())
    {
        _log("[A7670C] MQTT already connected.");
        return A7670C_Result::OK;
    }

    // Start MQTT service
    sendAT("AT+CMQTTSTART", "OK", 10000);
    if (!_lastOk)
        return A7670C_Result::MQTT_ERROR;

    // Create MQTT client instance
    sendAT("AT+CMQTTACCQ=0,\"" + _mqttClientId + "\",0", "OK", 5000);
    if (!_lastOk)
        return A7670C_Result::MQTT_ERROR;

    if (!_lastOk)
        return A7670C_Result::MQTT_ERROR;

    _log("[A7670C] MQTT configured");
    return A7670C_Result::OK;
}

A7670C_Result A7670C::mqttConnect()
{
    String brokerUrl =
        "tcp://" + _mqttHost + ":" + String(_mqttPort);

    String cmd;

    if (!_mqttUser.isEmpty())
    {
        cmd = "AT+CMQTTCONNECT=0,\"" + brokerUrl + "\",20,1,\"" + _mqttUser + "\",\"" + _mqttPass + "\"";
    }
    else
    {
        cmd = "AT+CMQTTCONNECT=0,\"" + brokerUrl + "\",20,1";
    }

    String r = sendAT(cmd,
                      "+CMQTTCONNECT: 0,0",
                      A7670C_MQTT_TIMEOUT_MS);

    if (!_lastOk)
    {
        _log("[A7670C] MQTT connect failed: " + r);
        return A7670C_Result::MQTT_ERROR;
    }

    _mqttConnected = true;

    _log("[A7670C] MQTT connected.");
    return A7670C_Result::OK;
}

A7670C_Result A7670C::mqttDisconnect()
{
    sendAT("AT+CMQTTDISC=0,60", "OK", 10000);

    sendAT("AT+CMQTTREL=0", "OK", 5000);

    sendAT("AT+CMQTTSTOP", "OK", 10000);

    _mqttConnected = false;

    return A7670C_Result::OK;
}

bool A7670C::mqttIsConnected()
{
    String r = sendAT("AT+CMQTTCONNECT?", "OK", 5000);
    return r.indexOf(_mqttHost) >= 0;
}

A7670C_Result A7670C::mqttPublish(const String &topic,
                                  const String &payload,
                                  uint8_t qos,
                                  bool retain)
{
    String r;

    r = sendAT(
        "AT+CMQTTTOPIC=0," + String(topic.length()),
        ">",
        5000);

    if (r.indexOf('>') < 0)
        return A7670C_Result::MQTT_ERROR;

    _serial.print(topic);

    r = _readUntil("OK", 5000);

    if (r.indexOf("OK") < 0)
        return A7670C_Result::MQTT_ERROR;

    r = sendAT(
        "AT+CMQTTPAYLOAD=0," + String(payload.length()),
        ">",
        5000);

    if (r.indexOf('>') < 0)
        return A7670C_Result::MQTT_ERROR;

    _serial.print(payload);

    r = _readUntil("OK", 5000);

    if (r.indexOf("OK") < 0)
        return A7670C_Result::MQTT_ERROR;

    r = sendAT(
        "AT+CMQTTPUB=0," +
            String(qos) +
            ",60",
        "+CMQTTPUB:",
        15000);

    if (r.indexOf("+CMQTTPUB: 0,0") < 0)
        return A7670C_Result::MQTT_ERROR;

    return A7670C_Result::OK;
}

A7670C_Result A7670C::mqttSubscribe(const String &topic, uint8_t qos)
{
    // Step 1: Send subtopic command, expect '>'
    String r = sendAT("AT+CMQTTSUBTOPIC=0," + String(topic.length()) + "," + String(qos), ">", 5000);
    if (r.indexOf('>') < 0)
        return A7670C_Result::MQTT_ERROR;

    // Step 2: Send the actual topic string
    _serial.print(topic);
    _serial.print("\r\n");

    // Step 3: Wait for OK after topic is accepted
    String resp = _readUntil("OK", 5000);
    if (resp.indexOf("OK") < 0)
        return A7670C_Result::MQTT_ERROR;

    // Step 4: Send SUB command, modem replies OK then +CMQTTSUB: 0,0
    // sendAT may only catch "OK" before the URC arrives, so read in two parts
    r = sendAT("AT+CMQTTSUB=0," + String(qos), "OK", 15000);
    if (r.indexOf("OK") < 0)
        return A7670C_Result::MQTT_ERROR;

    // Step 5: Read the trailing URC "+CMQTTSUB: 0,0"
    String urc = _readUntil("+CMQTTSUB:", 5000);
    String status = _readUntil("\n", 2000); // captures " 0,0"
    status.trim();

    // status should be "0,0" → client 0, result 0 (success)
    if (status.indexOf("0,0") < 0)
        return A7670C_Result::MQTT_ERROR;

    return A7670C_Result::OK;
}

A7670C_Result A7670C::mqttUnsubscribe(const String &topic)
{
    String r;

    r = sendAT(
        "AT+CMQTTUNSUBTOPIC=0," +
            String(topic.length()) +
            ",1",
        ">",
        5000);

    if (r.indexOf('>') < 0)
        return A7670C_Result::MQTT_ERROR;

    _serial.print(topic);

    r = _readUntil("OK", 5000);

    if (r.indexOf("OK") < 0)
        return A7670C_Result::MQTT_ERROR;

    r = sendAT(
        "AT+CMQTTUNSUB=0",
        "+CMQTTUNSUB:",
        15000);

    if (r.indexOf("+CMQTTUNSUB: 0,0") < 0)
        return A7670C_Result::MQTT_ERROR;

    return A7670C_Result::OK;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SMS
// ═══════════════════════════════════════════════════════════════════════════════

bool A7670C::_setSmsTextMode()
{
    sendAT("AT+CMGF=1", "OK"); // Text mode
    sendAT("AT+CSCS=\"GSM\""); // GSM charset
    return _lastOk;
}

A7670C_Result A7670C::smsSend(const String &number, const String &text)
{
    _setSmsTextMode();

    String cmd = "AT+CMGS=\"" + number + "\"";
    String r = sendAT(cmd, ">", 5000);
    if (r.indexOf('>') < 0)
        return A7670C_Result::ERROR;

    _serial.print(text);
    delay(100);
    _serial.write(0x1A); // Ctrl+Z to send
    _waitForToken("+CMGS:", 10000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::ERROR;
}

SmsMessage A7670C::smsRead(int index)
{
    SmsMessage msg;
    _setSmsTextMode();
    String r = sendAT("AT+CMGR=" + String(index), "OK", 5000);
    // +CMGR: "REC UNREAD","+91xxxxxxxx",,"24/01/01,10:00:00+22"
    int p = r.indexOf("+CMGR:");
    if (p < 0)
        return msg;

    msg.index = String(index);
    String header = r.substring(p + 7);
    // Parse quoted fields
    int q1 = header.indexOf('"');
    int q2 = header.indexOf('"', q1 + 1);
    msg.status = header.substring(q1 + 1, q2);

    int q3 = header.indexOf('"', q2 + 1);
    int q4 = header.indexOf('"', q3 + 1);
    msg.sender = header.substring(q3 + 1, q4);

    // Timestamp: skip one empty field (name field) then next quoted
    int q5 = header.indexOf('"', q4 + 2);
    int q6 = header.indexOf('"', q5 + 1);
    if (q5 >= 0 && q6 > q5)
        msg.timestamp = header.substring(q5 + 1, q6);

    // Text is after the first newline following the header
    int nl = header.indexOf('\n');
    if (nl >= 0)
    {
        msg.text = header.substring(nl + 1);
        int okPos = msg.text.lastIndexOf("\nOK");
        if (okPos >= 0)
            msg.text = msg.text.substring(0, okPos);
        msg.text.trim();
    }
    return msg;
}

int A7670C::smsList(SmsMessage *msgs, int maxCount, const String &status)
{
    _setSmsTextMode();
    String r = sendAT("AT+CMGL=\"" + status + "\"", "OK", 10000);
    int count = 0;
    int pos = 0;
    while (count < maxCount)
    {
        int p = r.indexOf("+CMGL:", pos);
        if (p < 0)
            break;
        // Parse index from: +CMGL: <index>,"status","sender",…
        String line = r.substring(p + 6);
        line.trim();
        int comma = line.indexOf(',');
        msgs[count].index = line.substring(0, comma);
        msgs[count].index.trim();
        // Reuse smsRead to fill the full struct
        msgs[count] = smsRead(msgs[count].index.toInt());
        pos = p + 6;
        count++;
    }
    return count;
}

A7670C_Result A7670C::smsDelete(int index, int flag)
{
    sendAT("AT+CMGD=" + String(index) + "," + String(flag), "OK", 5000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::ERROR;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VOICE CALLS
// ═══════════════════════════════════════════════════════════════════════════════

A7670C_Result A7670C::callDial(const String &number)
{
    sendAT("ATD" + number + ";", "OK", 5000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::ERROR;
}

A7670C_Result A7670C::callAnswer()
{
    sendAT("ATA", "OK", 5000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::ERROR;
}

A7670C_Result A7670C::callHangUp()
{
    sendAT("ATH", "OK", 5000);
    return _lastOk ? A7670C_Result::OK : A7670C_Result::ERROR;
}

bool A7670C::callIsActive()
{
    String r = sendAT("AT+CLCC", "OK");
    // Empty +CLCC means no active call
    return r.indexOf("+CLCC:") >= 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  RAW AT ENGINE
// ═══════════════════════════════════════════════════════════════════════════════

String A7670C::sendAT(const String &cmd, const String &expected, uint32_t timeoutMs)
{
    _flushRx();
    _log(">> " + cmd);
    _serial.println(cmd);
    String response = _readUntil(expected, timeoutMs);
    _lastResponse = response;
    _lastOk = (response.indexOf(expected) >= 0);
    _log("<< " + response);
    return response;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  LOOP – URC PARSER
// ═══════════════════════════════════════════════════════════════════════════════

void A7670C::loop()
{
    while (_serial.available())
    {
        String line = _serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
        {
            _parseURC(line);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  PRIVATE HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void A7670C::_flushRx()
{
    while (_serial.available())
        _serial.read();
}

String A7670C::_readUntil(const String &token, uint32_t timeoutMs)
{
    String buffer;
    uint32_t start = millis();
    buffer.reserve(256);

    while (millis() - start < timeoutMs)
    {
        while (_serial.available())
        {
            char c = _serial.read();
            buffer += c;
            if (buffer.indexOf(token) >= 0)
                return buffer;
            // Also stop on common error tokens
            if (buffer.indexOf("ERROR") >= 0)
                return buffer;
        }
        yield(); // ESP32 WDT
    }
    return buffer;
}

bool A7670C::_waitForToken(const String &token, uint32_t timeoutMs)
{
    String r = _readUntil(token, timeoutMs);
    _lastOk = r.indexOf(token) >= 0;
    _lastResponse += r;
    return _lastOk;
}

void A7670C::_parseURC(const String &line)
{
    // ── Incoming SMS ──────────────────────────────────────────────────────────
    if (line.startsWith("+CMTI:") && _smsCb)
    {
        // +CMTI: "SM",<index>
        int comma = line.lastIndexOf(',');
        if (comma >= 0)
        {
            int idx = line.substring(comma + 1).toInt();
            SmsMessage msg = smsRead(idx);
            _smsCb(msg);
        }
        return;
    }

    // ── Incoming call (RING + CLIP) ───────────────────────────────────────────
    if ((line == "RING" || line.startsWith("+CLIP:")) && _ringCb)
    {
        String caller = "";
        if (line.startsWith("+CLIP:"))
        {
            int q1 = line.indexOf('"');
            int q2 = line.indexOf('"', q1 + 1);
            if (q1 >= 0)
                caller = line.substring(q1 + 1, q2);
        }
        _ringCb(caller);
        return;
    }

    // ── MQTT inbound message ───────────────────────────────────────────────────
    // +CMQTTRXPAYLOAD: "<topic index>",<len>\r\n<payload>
    if (line.startsWith("+CMQTTRXPAYLOAD:") && _mqttMsgCb)
    {
        int q1 = line.indexOf('"');
        int q2 = line.indexOf('"', q1 + 1);
        String topic = line.substring(q1 + 1, q2);

        // Payload arrives on the next line(s) – read it
        String payload = "";
        uint32_t t = millis();
        while (millis() - t < 2000)
        {
            while (_serial.available())
            {
                char c = _serial.read();
                if (c == '\n')
                {
                    payload.trim();
                    _mqttMsgCb(topic, payload);
                    return;
                }
                payload += c;
            }
            yield();
        }
        return;
    }

    // ── Call ended ────────────────────────────────────────────────────────────
    if (line == "NO CARRIER")
    {
        // Notify if needed; currently just logs
        _log("[A7670C] NO CARRIER");
        return;
    }

    // ── Pass everything else to user callback ─────────────────────────────────
    if (_unsolCb)
        _unsolCb(line);
}

void A7670C::_log(const String &msg)
{
    if (_debugEn && _dbg)
    {
        _dbg->println(msg);
    }
}
