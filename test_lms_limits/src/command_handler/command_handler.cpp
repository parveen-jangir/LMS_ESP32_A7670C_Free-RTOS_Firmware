#include "command_handler.h"

CommandHandler *CommandHandler::_instance = nullptr;

void CommandHandler::mqttCallback(const A7670C::MQTTMessage &msg)
{
    if (_instance)
        _instance->handleMqttMessage(msg);
}

void CommandHandler::bootCallback(const String &line)
{
    if (_instance)
        _instance->handleBootEvent(line);
}

void CommandHandler::networkCallback(const String &line)
{
    if (_instance)
        _instance->handleNetworkEvent(line);
}

void CommandHandler::httpCallback(const HttpResponse &response)
{
    if (_instance)
        _instance->handleHttpAction(response);
}

void CommandHandler::handleMqttMessage(const A7670C::MQTTMessage &msg)
{
    Serial.println("======== MQTT MESSAGE =========");
    Serial.println("  Topic  : " + msg.topic);
    Serial.println("  Payload: " + msg.payload);
    Serial.println("================================");

    // Enqueue for processing in worker task
    if (!enqueue(msg.payload.c_str(), msg.payload.length(), false))
    {
        Serial.println(F("[CMD] Failed enqueue command"));
    }
}

void CommandHandler::handleBootEvent(const String &line)
{
    Serial.println("[BOOT] " + line);
}

void CommandHandler::handleNetworkEvent(const String &line)
{
    Serial.println("[NET] " + line);
}

void CommandHandler::handleHttpAction(const HttpResponse &response)
{
    Serial.println("[HTTP] Action: " + String(response.statusCode));

    Serial.println("        Success: " + String(response.success ? "YES" : "NO"));
    Serial.println("        Data length: " + String(response.dataLength) + " bytes");
    Serial.println("        Response code: " + String(response.statusCode));
}

void CommandHandler::onMqttMessage(const A7670C::MQTTMessage &msg)
{
    Serial.println(msg.topic);
}

void CommandHandler::onHttpAction(const HttpResponse &response)
{
    Serial.println(response.statusCode);
}

void CommandHandler::onOtaProgress(int percent, int written, int total)
{
    JsonDocument doc;
    doc["type"] = "ota_progress";
    doc["progress"] = percent;

    sendResponse(doc, true, true);
}

CommandHandler::CommandHandler(SensorManager &sensorMgr, StorageManager &storageMgr, GSM_OTA &gsmOta, A7670C &modem)
    : sensorMgr(sensorMgr), storageMgr(storageMgr), gsmOta(gsmOta), modem(modem)
{
    cmdQueue = xQueueCreate(COMMAND_QUEUE_DEPTH, sizeof(commandFormat));
    configASSERT(cmdQueue);
}

void CommandHandler::buildDeviceMac()
{
    uint64_t chipid = ESP.getEfuseMac();
    uint8_t mac[6];
    mac[0] = (chipid >> 40) & 0xFF;
    mac[1] = (chipid >> 32) & 0xFF;
    mac[2] = (chipid >> 24) & 0xFF;
    mac[3] = (chipid >> 16) & 0xFF;
    mac[4] = (chipid >> 8) & 0xFF;
    mac[5] = chipid & 0xFF;

    _deviceMac = "";
    _deviceMac.reserve(12);
    for (int i = 5; i >= 0; i--)
    {
        if (mac[i] < 0x10)
            _deviceMac += '0';
        _deviceMac += String(mac[i], HEX);
    }

    Serial.println("[DEVICE] MAC: " + _deviceMac);
}

void CommandHandler::buildTopic()
{
    _topic = String("lms/devices/") + _deviceMac + "/configuration/send";

    Serial.println("[MQTT] Subscribed Topic: " + _topic);
}

// ── Private: enqueue ──────────────────────────────────────────────────────────

bool CommandHandler::enqueue(const char *jsonStr, size_t len, bool fromBle)
{
    if (len > MAX_PAYLOAD_LEN)
    {
        Serial.printf("[CMD] enqueue: payload too large (%u > %u) — dropped\n",
                      len, MAX_PAYLOAD_LEN);
        return false;
    }
    commandFormat cmdPkt;
    cmdPkt.len = len;
    strncpy(cmdPkt.payload, jsonStr, len);
    cmdPkt.payload[len] = '\0';
    cmdPkt.formBle = fromBle;
    cmdPkt.formMqtt = !fromBle;

    Serial.printf("[CMD] from %s enqueue: (%u bytes): %s\n", fromBle ? "WEBSOCKET" : "MQTT", len, cmdPkt.payload);

    // Non-blocking: we must never block a BLE stack callback or MQTT loop.
    return (xQueueSendToBack(cmdQueue, &cmdPkt, 0) == pdTRUE);
}

CommandHandler::~CommandHandler()
{
    if (workerHandle)
    {
        vTaskDelete(workerHandle);
    }
    if (cmdQueue)
    {
        vQueueDelete(cmdQueue);
    }
}

void CommandHandler::begin()
{
    _instance = this;
    
    modem.onMqttMessage(mqttCallback);
    modem.onBootEvent(bootCallback);
    modem.onNetworkEvent(networkCallback);
    modem.onHttpAction(httpCallback);

    xTaskCreate(
        workerTask,
        "CmdWorker",
        COMMAND_TASK_STACK_SIZE,
        this,
        COMMAND_TASK_PRIORITY,
        &workerHandle);

    // // Enable AT traffic debug output
    // modem.setDebug(Serial);
    // modem.setDebugEnabled(true); // ← set false to hide raw AT traffic

    buildDeviceMac();
    buildTopic();

    setupMqtt();

    xTaskCreate(
        apiTask,
        "ApiWorker",
        API_TASK_STACK_SIZE,
        this,
        API_TASK_PRIORITY,
        &apiHandle);
}

void CommandHandler::setupMqtt()
{
    modem.configureMqtt(
        _deviceMac, // client ID
        MQTT_BROKER, // host
        MQTT_PORT,            // port
        MQTT_USER,
        MQTT_PASS);

    if (modem.mqttConnect())
    {
        if (modem.mqttSubscribe(_topic))
        {
            JsonDocument doc;
            doc["status"] = "ok";
            doc["message"] = "[MQTT] Live";
            sendResponse(doc, true, true);
        }
    }
}

void CommandHandler::workerTask(void *param)
{
    CommandHandler *handler = static_cast<CommandHandler *>(param);
    if (!handler)
    {
        vTaskDelete(nullptr);
        return;
    }
    handler->workerLoop();
    vTaskDelete(nullptr);
}


void CommandHandler::workerLoop()
{
    commandFormat cmdPkt;
    while (true)
    {
        // Block indefinitely until a packet arrives — zero CPU burn while idle.
        if (xQueueReceive(cmdQueue, &cmdPkt, portMAX_DELAY) == pdTRUE)
        {
            dispatch(cmdPkt);
        }
        else
        {
            Serial.println(F("[CMD] workerLoop: xQueueReceive failed"));
        }
        vTaskDelay(pdMS_TO_TICKS(COMMAND_TASK_DELAY_MS));
    }
}

void CommandHandler::apiTask(void *param)
{
    CommandHandler *self = static_cast<CommandHandler *>(param);
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true)
    {
        String apiUrl = self->sensorMgr.generateApiUrl(self->tid);
        Serial.println("[API] URL: " + apiUrl);

        self->modem.hitHttpGetRequest(apiUrl);

        vTaskDelay(pdMS_TO_TICKS(API_HIT_INTERVAL_MS));
    }
    vTaskDelete(nullptr);
}

void CommandHandler::dispatch(const commandFormat &pkt)
{
    // Parse JSON — use a local doc so every command gets a clean slate.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, pkt.payload);
    if (err)
    {
        Serial.printf("[CMD] JSON parse error: %s  payload: %s\n",
                      err.c_str(), pkt.payload);

        // Send a structured error back to the caller.
        doc["status"] = "error";
        doc["error"] = "json_parse_failed";
        doc["detail"] = err.c_str();
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
        return;
    }

    // Every command must have a "type" field.
    const char *cmdStr = doc["type"] | "";
    if (cmdStr[0] == '\0')
    {
        Serial.println(F("[CMD] dispatch: missing 'type' field"));

        JsonDocument errResp;
        errResp["status"] = "error";
        errResp["error"] = "missing_cmd_field";
        sendResponse(errResp, pkt.formBle, pkt.formMqtt);
        return;
    }
    if (strcmp(cmdStr, "status") == 0)
    {
        doc["status"] = "ok";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "ping") == 0)
    {
        doc["status"] = "ok";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "sensor_state") == 0)
    {
        handleSensorState(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "sensor_broadcast") == 0)
    {
        handleSensorBroadcast(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "system_info") == 0)
    {
        if (!getSystemInfo(doc, pkt.formBle, pkt.formMqtt))
        {
            doc["status"] = "error";
            doc["error"] = "failed_get_system_info";
        }
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "update_device") == 0)
    {
        handleOtaUpdate(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "reboot") == 0)
    {
        doc["status"] = "ok";
        doc["message"] = "Rebooting...";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
        esp_restart();
    }
    else
    {
        Serial.printf("[CMD] dispatch: unknown command '%s'\n", cmdStr);
        doc["status"] = "error";
        doc["error"] = "unknown_command";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
}

void CommandHandler::sendResponse(JsonDocument &response, bool toBle, bool toMqtt)
{
    String topic = String("lms/devices/") + _deviceMac + "/configuration/callback";
    String payload;
    serializeJson(response, payload);
    
    if (toBle)
    {
    }

    if (modem.getMQTTConnected() && toMqtt)
    {
        if (!modem.mqttPublish(topic, payload))
        {
            Serial.println(F("[CMD] sendResponse(MQTT): publish failed"));
        }
    }

    Serial.print(("[CMD] sendResponse: " + payload + "\n"));
}

void CommandHandler::handleSensorState(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    const char *sensorName = doc["sensor"] | "";
    bool state = doc["state"] | false;

    if (sensorName[0] == '\0')
    {
        Serial.println(F("[CMD][ERROR] incomplete cmd"));
        JsonDocument errResp;
        errResp["status"] = "error";
        errResp["error"] = "missing_sensor_field";
        sendResponse(errResp, fromBle, fromMqtt);
        return;
    }

    sensorMgr.sensorState(sensorName, state);

    doc["status"] = "ok";
    doc["msg"] = String("Sensor '") + sensorName + "' " + (state ? "enabled" : "disabled");
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleSensorBroadcast(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    doc.clear();

    sensorMgr.getAllReadingsJson(doc);

    sendResponse(doc, fromBle, fromMqtt);
}

bool CommandHandler::getSystemInfo(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    int rssi;
    String time;
    getTime(time);
    modem.getSignalStrength(rssi);

    doc["uptime_s"] = esp_timer_get_time() / 1000000ULL;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["gsm_signal"] = rssi;
    doc["mqtt_connected"] = modem.getMQTTConnected();
    doc["device_mac"] = _deviceMac;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["device_name"] = DEVICE_NAME;
    doc["current_time"] = time;

    doc["status"] = "ok";
    doc["msg"] = "System information retrieved";

    return true;
}

time_t CommandHandler::getTime(String &formatted)
{
    time_t now = time(nullptr);
    if (now < 0)
        return -1;

    time_t ist = now + (5 * 3600) + (30 * 60);

    struct tm *t = gmtime(&ist);

    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        t->tm_year + 1900,
        t->tm_mon  + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec);

    formatted = buf;
    return ist;
}

void CommandHandler::handleOtaUpdate(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    // modem.mqttDisconnect();
    vTaskSuspend(apiHandle);
    // vTaskSuspend(gsmHandle);
    
    while (modem.getHttpInAction())
    {
        Serial.println("[OTA] HTTP in action...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    modem.pauseTasks();

    gsmOta.setAPN(APN);
    gsmOta.setChunkSize(1024); // optional – 1024 is default
    gsmOta.setDebugEnabled(true);

    String url = doc["url"] | OTA_URL;
    OTAResult result = gsmOta.performOTA(url.c_str());

    // setupMqtt();
    modem.resumeTasks();

    if (result == OTA_SUCCESS)
    {
        Serial.println("[OTA] done! Rebooting in 3 s");
        delay(3000);
        doc["status"] = "ok";
        doc["msg"] = "OTA successful, rebooting...";
        sendResponse(doc, true, true);
    }
    else
    {
        doc["status"] = "error";
        doc["error"] = GSM_OTA::resultToString(result);
        doc["msg"] = "rebooting...";
        sendResponse(doc, true, true);
        Serial.printf("[OTA] FAILED: %s (code %d)\n", GSM_OTA::resultToString(result), (int)result);
    }

    esp_restart();
    // vTaskResume(gsmHandle);
}