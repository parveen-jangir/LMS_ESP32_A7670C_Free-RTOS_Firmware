#include "A7670C.h"

// ─────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────
A7670C::A7670C(HardwareSerial &serial, int pwrPin, DataLogger &dataLogger):
    _serial(&serial), _pwrPin(pwrPin), logger(dataLogger)
{
    // _serial = &serial;
    // _dataLogger = dataLogger;

    _mqttCallback = nullptr;
    _smsCallback = nullptr;
    _httpCallback = nullptr;
    _callCallback = nullptr;
    _bootCallback = nullptr;
    _networkCallback = nullptr;

    _parserState = STATE_IDLE;
    _pendingCmd = nullptr;
    _pendingMutex = xSemaphoreCreateMutex();

    _mqttPort = 1883;
    _mqttConnected = false;
    _httpInAction = false;
    _onReset = false;
    // _pwrPin = pwrPin;
}

// ─────────────────────────────────────────────────────────────────
// begin
// ─────────────────────────────────────────────────────────────────

bool A7670C::begin()
{
    pinMode(_pwrPin, OUTPUT);

    BaseType_t result =
        xTaskCreatePinnedToCore(
            rxTask,
            "gsm_rx",
            GSM_TASK_STACK_SIZE,
            this,
            GSM_TASK_PRIORITY,
            &_rxTaskHandle,
            GSM_TASK_CORE);

    String formatted;

    if(moduleOn())
        setTime(formatted);

    return result == pdPASS;
}

// ─────────────────────────────────────────────────────────────────
// sendAT / sendATWait
// ─────────────────────────────────────────────────────────────────
void A7670C::sendAT(const String &cmd)
{
    _serial->println(cmd);
}

bool A7670C::sendATWait(
    const String &cmd,
    LineMatcher matcher,
    String &result,
    uint32_t timeoutMs)
{
    QueueHandle_t q = xQueueCreate(4, sizeof(QueueItem));
    if (!q)
        return false;

    PendingCmd pending{matcher, q};

    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
    _pendingCmd = &pending;
    xSemaphoreGive(_pendingMutex);

    sendAT(cmd);

    QueueItem item;
    bool got = xQueueReceive(q, &item, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;

    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
    _pendingCmd = nullptr;
    xSemaphoreGive(_pendingMutex);

    vQueueDelete(q);

    if (got)
        result = String(item.buf);

    return got;
}

// ─────────────────────────────────────────────────────────────────
// _sendPromptData
// Waits for '>' prompt then writes raw data (no println).
// Used for SUBTOPIC and PAYLOAD steps.
// ─────────────────────────────────────────────────────────────────

bool A7670C::_sendPromptData(const String &data)
{
    // Register a pending filter for '>'
    QueueHandle_t q = xQueueCreate(2, sizeof(QueueItem));
    if (!q)
        return false;

    PendingCmd pending{
        [](const String &l)
        { return l == ">"; },
        q};

    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
    _pendingCmd = &pending;
    xSemaphoreGive(_pendingMutex);

    // Wait up to 3 s for '>'
    QueueItem item;
    bool got = xQueueReceive(q, &item, pdMS_TO_TICKS(3000)) == pdTRUE;

    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
    _pendingCmd = nullptr;
    xSemaphoreGive(_pendingMutex);

    vQueueDelete(q);

    if (!got)
        return false;

    // Send data without \r\n — modem reads exact byte count
    _serial->print(data);
    return true;
}

// ─────────────────────────────────────────────────────────────────
// _mqttStart  —  AT+CMQTTSTART
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttStart()
{
    String result;
    // Wait for URC: +CMQTTSTART: 0
    bool ok = sendATWait(
        "AT+CMQTTSTART",
        [](const String &l)
        { return l.startsWith("+CMQTTSTART:") || l.startsWith("ERROR"); },
        result,
        2000);

    if (!ok)
    {
        return false;
    }

    if (result.startsWith("ERROR"))
    {
        Serial.println("[MQTT] Already started");
        logger.log('I', "[MQTT] Already started");
        return false;
    }

    // +CMQTTSTART: 0  means success; any other code is error
    return result.endsWith(": 0") || result.endsWith(":0");
}

bool A7670C::mqttStop()
{
    String result;
    bool ok = sendATWait(
        "AT+CMQTTSTOP",
        [](const String &l)
        { return l.startsWith("ERROR") || l == "OK"; },
        result,
        2000);

    ok = sendATWait(
        "AT+CMQTTDISC=0,60",
        [](const String &l)
        { return l.startsWith("ERROR") || l == "OK"; },
        result,
        2000);

    ok = sendATWait(
        "AT+CMQTTREL=0",
        [](const String &l)
        { return l.startsWith("ERROR") || l == "OK"; },
        result,
        2000);

    _mqttConnected = false;

    return ok && result == "OK";
}

// ─────────────────────────────────────────────────────────────────
// _mqttAcquire  —  AT+CMQTTACCQ
// ─────────────────────────────────────────────────────────────────

bool A7670C::_mqttAcquire()
{
    // AT+CMQTTACCQ=0,"clientId",0
    String cmd = "AT+CMQTTACCQ=0,\"" + _mqttClientId + "\",0";
    String result;
    bool ok = sendATWait(
        cmd,
        [](const String &l)
        { return l == "OK" || l.startsWith("ERROR"); },
        result,
        5000);

    return ok;
}

// ─────────────────────────────────────────────────────────────────
// configureMqtt  —  store credentials only
// ─────────────────────────────────────────────────────────────────

bool A7670C::configureMqtt(
    const String &clientId,
    const String &host,
    uint16_t port,
    const String &user,
    const String &pass)
{
    _mqttClientId = clientId;
    _mqttHost = host;
    _mqttPort = port;
    _mqttUser = user;
    _mqttPass = pass;
    _mqttConnected = false;
    return true;
}

// ─────────────────────────────────────────────────────────────────
// mqttConnect
// Flow: CMQTTSTART → CMQTTACCQ → CMQTTCONNECT
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttConnect()
{
    if(!_isPoweredOn)
    {
        moduleOn();
    }

    mqttStart();

    if (!_mqttAcquire())
    {
        Serial.println("[MQTT] CMQTTACCQ failed");
        logger.log('E', "[MQTT] CMQTTACCQ failed");
        return false;
    }

    // Build connect command
    // AT+CMQTTCONNECT=0,"tcp://host:port",keepalive,cleanSession[,"user","pass"]
    String cmd = "AT+CMQTTCONNECT=0,\"tcp://" +
                 _mqttHost + ":" + String(_mqttPort) +
                 "\",20,1";

    if (_mqttUser.length() > 0)
    {
        cmd += ",\"" + _mqttUser + "\",\"" + _mqttPass + "\"";
    }

    String result;
    bool ok = sendATWait(
        cmd,
        [](const String &l)
        { return l.startsWith("+CMQTTCONNECT:") || l.startsWith("ERROR"); },
        result,
        15000);

    if (!ok)
    {
        Serial.println("[MQTT] CMQTTCONNECT timeout");
        logger.log('E', "[MQTT] CMQTTCONNECT timeout");
        return false;
    }

    if (result.startsWith("+CMQTTCONNECT: 0,19"))
    {
        _mqttConnected = true;
        Serial.println("[MQTT] Already connected");
        return true;
    }

    if (result.startsWith("ERROR"))
    {
        Serial.println("[MQTT] broker error");
        logger.log('E', "[MQTT] broker error");
        return false;
    }

    // +CMQTTCONNECT: 0,0  — second field is error code; 0 = success
    // Parse: find second comma-separated token
    int comma = result.indexOf(',');
    if (comma < 0)
        return false;

    int code = result.substring(comma + 1).toInt();
    if (code != 0)
    {
        Serial.println("[MQTT] Connect error code: " + String(code));
        logger.log('E', "[MQTT] Connect error code: " + String(code));
        return false;
    }

    Serial.println("[MQTT] Connected");
    logger.log('I', "[MQTT] Connected");

    _mqttConnected = true;
    return true;
}

bool A7670C::mqttReconnect()
{
    mqttDisconnect();
    return mqttConnect();
}

// ─────────────────────────────────────────────────────────────────
// mqttDisconnect
// Flow: CMQTTDISC → CMQTTREL → CMQTTSTOP
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttDisconnect()
{
    String result;

    // Disconnect from broker
    sendATWait(
        "AT+CMQTTDISC=0,20",
        [](const String &l)
        { return l.startsWith("+CMQTTDISC:"); },
        result,
        10000);

    // Release client
    sendATWait(
        "AT+CMQTTREL=0",
        [](const String &l)
        { return l == "OK" || l.startsWith("ERROR"); },
        result,
        5000);

    // Stop MQTT service
    sendATWait(
        "AT+CMQTTSTOP",
        [](const String &l)
        { return l.startsWith("+CMQTTSTOP:") || l == "ERROR"; },
        result,
        5000);

    _mqttConnected = false;
    return true;
}

// ─────────────────────────────────────────────────────────────────
// mqttIsConnected
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttIsConnected()
{
    return _mqttConnected;
}

// ─────────────────────────────────────────────────────────────────
// mqttSubscribe
// Flow: CMQTTSUBTOPIC (→ '>' → send topic) → CMQTTSUB → wait URC
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttSubscribe(const String &topic)
{
    if (!_mqttConnected)
        return false;

    uint16_t topicLen = topic.length();

    // Step 1: Tell modem topic length; wait for '>'
    String cmd = "AT+CMQTTSUBTOPIC=0," + String(topicLen) + ",0";
    sendAT(cmd);

    if (!_sendPromptData(topic))
    {
        Serial.println("[MQTT] SUBTOPIC prompt failed");
        logger.log('E', "[MQTT] SUBTOPIC failed");
        return false;
    }

    // Step 2: Wait for OK after topic data
    String result;
    bool ok = sendATWait(
        "", // no command — just wait for the OK that follows data entry
        [](const String &l)
        { return l == "OK" || l.startsWith("ERROR"); },
        result,
        3000);

    if (!ok || result != "OK")
        return false;

    // Step 3: Execute subscribe; wait for URC +CMQTTSUB: 0,0
    ok = sendATWait(
        "AT+CMQTTSUB=0,0",
        [](const String &l)
        { return l.startsWith("+CMQTTSUB:"); },
        result,
        10000);

    if (!ok)
        return false;

    // +CMQTTSUB: 0,0 — second field 0 = success
    int comma = result.indexOf(',');
    if (comma < 0)
        return false;
    
    if(result.substring(comma + 1).toInt() == 0)
    {
        Serial.println("[MQTT] Topic subscribed");
        logger.log('I', "[MQTT] Topic subscribed");
        return true;
    }
    else
    {
        Serial.println("[MQTT] Subscription failed");
        logger.log('E', "[MQTT] Subs failed");
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────
// mqttPublish
// Flow: CMQTTTOPIC (→ '>' → topic)
//     → CMQTTPAYLOAD (→ '>' → payload)
//     → CMQTTPUB → wait URC
// ─────────────────────────────────────────────────────────────────

bool A7670C::mqttPublish(
    const String &topic,
    const String &payload)
{
    if (!_mqttConnected)
        return false;

    String result;

    // ── Step 1: Set topic ─────────────────────────────────────────
    String topicCmd = "AT+CMQTTTOPIC=0," + String(topic.length());
    sendAT(topicCmd);

    if (!_sendPromptData(topic))
    {
        Serial.println("[MQTT] TOPIC prompt failed");
        return false;
    }

    // Wait for OK after topic data
    bool ok = sendATWait(
        "",
        [](const String &l)
        { return l == "OK" || l.startsWith("ERROR"); },
        result,
        3000);

    if (!ok || result != "OK")
        return false;

    // ── Step 2: Set payload ───────────────────────────────────────
    String payloadCmd = "AT+CMQTTPAYLOAD=0," + String(payload.length());
    sendAT(payloadCmd);

    if (!_sendPromptData(payload))
    {
        Serial.println("[MQTT] PAYLOAD prompt failed");
        return false;
    }

    // Wait for OK after payload data
    ok = sendATWait(
        "",
        [](const String &l)
        { return l == "OK" || l.startsWith("ERROR"); },
        result,
        3000);

    if (!ok || result != "OK")
        return false;

    // ── Step 3: Publish ───────────────────────────────────────────
    // AT+CMQTTPUB=0,qos,timeout
    ok = sendATWait(
        "AT+CMQTTPUB=0,1,60",
        [](const String &l)
        { return l.startsWith("+CMQTTPUB:"); },
        result,
        10000);

    if (!ok)
        return false;

    // +CMQTTPUB: 0,0 — second field 0 = success
    int comma = result.indexOf(',');
    if (comma < 0)
        return false;

    return result.substring(comma + 1).toInt() == 0;
}

// ─────────────────────────────────────────────────────────────────
// rxTask  —  sole serial reader
// ─────────────────────────────────────────────────────────────────

void A7670C::rxTask(void *param)
{
    A7670C *self = static_cast<A7670C *>(param);

    char buffer[1024];
    size_t index = 0;

    while (true)
    {
        while (self->_serial->available())
        {
            char c = self->_serial->read();

            if (c == '\r')
                continue;

            if (index < sizeof(buffer) - 1)
                buffer[index++] = c;

            if (c == '\n')
            {
                buffer[index] = '\0';

                String line(buffer);
                if(DEBUG_GSM_RX){
                    Serial.print("[GSM] RX: ");
                    Serial.print(line);
                }

                self->processLine(line);
                index = 0;
            }
            // Handle bare '>' prompt (no newline from modem)
            else if (c == '>' && index == 1)
            {
                buffer[index] = '\0';
                String line = ">";
                if(DEBUG_GSM_RX)
                    Serial.println("[GSM] RX: >");
                self->processLine(line);
                index = 0;
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// ─────────────────────────────────────────────────────────────────
// processLine  —  route to pending waiter and/or URC callbacks
// ─────────────────────────────────────────────────────────────────

void A7670C::processLine(String line)
{
    line.trim();

    if (line.isEmpty())
        return;

    // ── Pending command filter ────────────────────────────────────
    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
    PendingCmd *pending = _pendingCmd;
    xSemaphoreGive(_pendingMutex);

    if (pending && pending->matcher(line))
    {
        QueueItem item;
        strncpy(item.buf, line.c_str(), sizeof(item.buf) - 1);
        item.buf[sizeof(item.buf) - 1] = '\0';
        xQueueSend(pending->queue, &item, 0);
        // fall through — URC handlers still run
    }

    // ── URC dispatch ─────────────────────────────────────────────
    switch (_parserState)
    {
    case STATE_IDLE:

        if (line.startsWith("+CMQTTRXSTART"))
        {
            _mqttMsg.topic.clear();
            _mqttMsg.payload.clear();
            return;
        }

        if (line.startsWith("+CMQTTCONNLOST: 0,"))
        {
            _mqttConnected = false;
            Serial.println("[MQTT] Connection lost");
            return;
        }

        if (line.startsWith("+CMQTTRXTOPIC"))
        {
            _parserState = STATE_MQTT_READ_TOPIC;
            return;
        }

        if (line.startsWith("+CMQTTRXPAYLOAD"))
        {
            _parserState = STATE_MQTT_READ_PAYLOAD;
            return;
        }

        if (line.startsWith("+CMQTTRXEND"))
        {
            if (_mqttCallback)
                _mqttCallback(_mqttMsg);
            return;
        }

        if (line.startsWith("+CMTI"))
        {
            if (_smsCallback)
                _smsCallback(line);
            return;
        }

        if (line.startsWith("+HTTPACTION:"))
        {
            if (sscanf(line.c_str(), "+HTTPACTION: %*d,%d,%d",
                       &_httpResponse.statusCode, &_httpResponse.dataLength) == 2)
            {
                _httpResponse.success = (_httpResponse.statusCode >= 200 && _httpResponse.statusCode < 300);
            }

            _httpInAction = false;

            if (_httpCallback)
                _httpCallback(_httpResponse);

            return;
        }

        if (line.startsWith("+CLCC") ||
            line.startsWith("VOICE CALL") ||
            line.startsWith("NO CARRIER"))
        {
            if (_callCallback)
                _callCallback(line);
            return;
        }

        if (line.startsWith("*ATREADY") ||
            line.startsWith("+CPIN") ||
            line.startsWith("PB DONE"))
        {
            if (_bootCallback)
                _bootCallback(line);
            return;
        }

        if (line.startsWith("+CGEV"))
        {
            if(line.startsWith("+CGEV: ME DETACH"))
            {
                _mqttConnected = false;
                _isPoweredOn = false;

                Serial.println("[A7670C] turn off");
            }
            if (_networkCallback)
                _networkCallback(line);
            return;
        }

        break;

    case STATE_MQTT_READ_TOPIC:
        _mqttMsg.topic = line;
        _parserState = STATE_IDLE;
        return;

    case STATE_MQTT_READ_PAYLOAD:
        if (line.startsWith("+CMQTTRXEND"))
        {
            _parserState = STATE_IDLE;
            if (_mqttCallback)
                _mqttCallback(_mqttMsg);
            return;
        }
        _mqttMsg.payload = _mqttMsg.payload + line;
        return;
    }
}

bool A7670C::hitHttpGetRequest(const String &url)
{
    if (_httpInAction)
    {
        Serial.println("[HTTP] Already in action");
        return false;
    }

    String result;
    bool ok = sendATWait(
        "AT+HTTPTERM",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        2000);

    ok = sendATWait(
        "AT+HTTPINIT",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        2000);

    if (result.startsWith("ERROR") || !ok)
    {
        Serial.println("[HTTP] HTTPINIT failed");
        return false;
    }

    ok = sendATWait(
        "AT+HTTPPARA=\"URL\",\"" + url + "\"",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        2000);

    if (result.startsWith("ERROR") || !ok)
    {
        Serial.println("[HTTP] HTTPPARA failed");
        return false;
    }

    ok = sendATWait(
        "AT+HTTPACTION=0",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        5000);

    if (result.startsWith("ERROR") || !ok)
    {
        Serial.println("[HTTP] HTTPACTION failed");
        return false;
    }

    Serial.println("[HTTP] GET request sent");

    _httpInAction = true;
    return true;
}

// Max size will be 1024 bytes, min(dataLength, 1024)
bool A7670C::readHttpResponse(int dataLength, String &response)
{
    int actualLength = min(dataLength, 1024);
    String result;

    bool ok = sendATWait(
        "AT+HTTPREAD=0," + String(actualLength),
        [](const String &l)
        { return l.startsWith("{") || l.startsWith("ERROR"); },
        result,
        2000);

    if (!ok || result.startsWith("ERROR"))
    {
        Serial.println("[HTTP] HTTPREAD failed");
        return false;
    }

    response = result;
    return true;
}

// Get and update system time from modem's CCLK
bool A7670C::setTime(String &timeString)
{
    bool ok = sendATWait(
        "AT+CCLK?",
        [](const String &l){ return l.startsWith("+CCLK:") || l.startsWith("ERROR"); },
        timeString,
        2000);

    if (!ok || timeString.startsWith("ERROR"))
    {
        Serial.println("[TIME] CCLK failed");
        return false;
    }

    int yy, MM, dd, hh, mm, ss, tz;
    int count = sscanf(timeString.c_str(),
        "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%d\"",
        &yy, &MM, &dd, &hh, &mm, &ss, &tz);

    if (count != 7)
        return false;

    if (yy < 24 || MM < 1 || MM > 12 || dd < 1 || dd > 31 ||
        hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
        return false;

    struct tm tmTime = {};
    tmTime.tm_year = yy + 100;
    tmTime.tm_mon  = MM - 1;
    tmTime.tm_mday = dd;
    tmTime.tm_hour = hh;
    tmTime.tm_min  = mm;
    tmTime.tm_sec  = ss;
    tmTime.tm_isdst = -1;

    time_t localEpoch = mktime(&tmTime);
    if (localEpoch == -1)
        return false;

    time_t utcEpoch = localEpoch - (tz * 900);

    struct timeval tv = { .tv_sec = utcEpoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    // Format from UTC epoch — not from raw parsed variables
    struct tm *utcTm = gmtime(&utcEpoch);
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
        utcTm->tm_year + 1900,
        utcTm->tm_mon  + 1,
        utcTm->tm_mday,
        utcTm->tm_hour,
        utcTm->tm_min,
        utcTm->tm_sec);

    timeString = buffer;
    Serial.printf("[TIME] System synced: %s UTC\n", buffer);
    return true;
}

bool A7670C::moduleOn()
{
    String resp;
    bool ok;

    Serial.println("[A7670C] power ON...");

    ok = sendATWait(
        "AT",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        resp,
        1300);

    if (ok && resp.startsWith("OK"))
    {
        Serial.println("[A7670C] already ON");
    }
    else
    {

        digitalWrite(_pwrPin, LOW); // PWRKEY LOW to turn on
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(_pwrPin, HIGH); // Release PWRKEY

        for (int i = 0; i < 10; i++)
        {
            ok = sendATWait(
                "AT",
                [](const String &l)
                { return l.startsWith("OK") || l.startsWith("ERROR"); },
                resp,
                1300);

            if (ok && resp.startsWith("OK"))
            {
                Serial.println("[A7670C] powered ON successfully");
                break;
            }
        }
    }

    int rssi;

    for (int i = 0; i < 10; i++)
    {
        if (getSignalStrength(rssi))
        {
            Serial.println("[A7670C] signal strength: " + String(rssi));
            Serial.println("[A7670C] ready");
            _isPoweredOn = true;
            return true;
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS); // Wait 1 second before retrying
    }

    Serial.println("[A7670C] power ON failed");
    return false;
}

bool A7670C::moduleReset()
{
    Serial.println("[A7670C] resetting...");
    digitalWrite(_pwrPin, HIGH); // Release PWRKEY

    String resp;
    bool ok;

    ok = sendATWait(
        "AT+CRESET",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        resp,
        1000);

    if (!ok || !resp.startsWith("OK"))
    {
        Serial.println("[A7670C] CRESET command failed");
        _isPoweredOn = false;
        _mqttConnected = false;
        return false;
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for modem to reboot

    for (int i = 0; i < 10; i++)
    {
        ok = sendATWait(
            "AT",
            [](const String &l)
            { return l.startsWith("OK") || l.startsWith("ERROR"); },
            resp,
            1000);

        if (ok && resp.startsWith("OK"))
        {
            Serial.println("[A7670C] reset successfully");
        }
    }

    int rssi;

    for (int i = 0; i < 10; i++)
    {
        if (getSignalStrength(rssi))
        {
            Serial.println("[A7670C] signal strength: " + String(rssi));
            _isPoweredOn = true;
            return true;
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS); // Wait 1 second before retrying
    }

    Serial.println("[A7670C] reset failed");
    return false;
}

bool A7670C::moduleOff()
{
    Serial.println("[A7670C] power OFF...");
    digitalWrite(_pwrPin, HIGH); // Release PWRKEY

    // Wait for modem to shut down (no response to AT)
    String resp;

    bool ok = sendATWait(
        "AT+CPOF",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        resp,
        5000);

    vTaskDelay(2000 / portTICK_PERIOD_MS); // Extra delay to ensure shutdown

    ok = sendATWait(
        "AT",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        resp,
        2000);

    if (!ok)
    {
        Serial.println("[A7670C] powered OFF");
        _isPoweredOn = false;
        _mqttConnected = false;
        return true;
    }

    Serial.println("[A7670C] power OFF failed");
    return false;
}

bool A7670C::getSignalStrength(int &rssi)
{
    String result;
    bool ok = sendATWait(
        "AT+CSQ",
        [](const String &l)
        { return l.startsWith("+CSQ:"); },
        result,
        3000);

    if (!ok)
        return false;

    // result = "+CSQ: 18,99"
    int colon = result.indexOf(':');
    if (colon < 0)
        return false;

    rssi = result.substring(colon + 1).toInt();

    // 99 means not detectable
    return rssi != 99;
}

bool A7670C::getIp(String &ip)
{
    String result;
    bool ok = sendATWait(
        "AT+CGPADDR=1",
        [](const String &l)
        { return l.startsWith("+CGPADDR:") || l.startsWith("ERROR"); },
        result,
        3000);

    if (!ok)
        return false;

    if (result.startsWith("ERROR")){
        ip = "0.0.0.0";
        return true;
    }

    // result = "+CGPADDR: 192.168.1.100"
    int colon = result.indexOf(':');
    if (colon < 0)
        return false;

    ip = result.substring(colon + 1);

    return true;
}

void A7670C::pauseTasks()
{
    if (_rxTaskHandle)
    {
        vTaskSuspend(_rxTaskHandle);
    }
}

void A7670C::resumeTasks()
{
    if (_rxTaskHandle)
    {
        vTaskResume(_rxTaskHandle);
    }
}

bool A7670C::sendSms(const String &number, const String &message)
{
    String result;

    // Set SMS mode to text
    bool ok = sendATWait(
        "AT+CMGF=1",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        2000);

    if (!ok || result.startsWith("ERROR"))
    {
        Serial.println("[SMS] CMGF failed");
        return false;
    }

    ok = sendATWait(
        "AT+CSCS=\"GSM\"",
        [](const String &l)
        { return l.startsWith("OK") || l.startsWith("ERROR"); },
        result,
        2000);

    if (!ok || result.startsWith("ERROR"))
    {
        Serial.println("[SMS] CSCS failed");
        return false;
    }

    // Send command to set recipient number
    String cmd = "AT+CMGS=\"" + number + "\"";
    sendAT(cmd);

    String smsData = message;
    smsData += (char)0x1A; // Ctrl+Z to indicate end of message

    if (!_sendPromptData(smsData))
    {
        Serial.println("[SMS] msg prompt failed");
        return false;
    }

    // Wait for confirmation
    ok = sendATWait(
        "",
        [](const String &l)
        { return l.startsWith("+CMGS:") || l.startsWith("ERROR"); },
        result,
        10000);

    if (!ok || result.startsWith("ERROR"))
    {
        Serial.println("[SMS] Send failed");
        return false;
    }

    Serial.println("[SMS] Sent successfully");
    return true;
}

// ─────────────────────────────────────────────────────────────────
// Callback setters
// ─────────────────────────────────────────────────────────────────
void A7670C::onMqttMessage(MqttCallback cb) { _mqttCallback = cb; }
void A7670C::onSmsReceived(LineCallback cb) { _smsCallback = cb; }
void A7670C::onHttpAction(HttpActionCallback cb) { _httpCallback = cb; }
void A7670C::onCallEvent(LineCallback cb) { _callCallback = cb; }
void A7670C::onBootEvent(LineCallback cb) { _bootCallback = cb; }
void A7670C::onNetworkEvent(LineCallback cb) { _networkCallback = cb; }