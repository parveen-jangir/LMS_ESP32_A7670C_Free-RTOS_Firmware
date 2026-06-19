#include "command_handler.h"

CommandHandler *CommandHandler::_instance = nullptr;

time_t getTime()
{
    time_t now = time(nullptr);
    if (now < 0)
        return -1;

    time_t ist = now + (5 * 3600) + (30 * 60);

    struct tm *t = gmtime(&ist);
    return ist;
}

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
    logger.log('I', "[BOOT] " + line);
}

void CommandHandler::handleNetworkEvent(const String &line)
{
    Serial.println("[NET] " + line);
    logger.log('I', "[NET] " + line);
}

void CommandHandler::handleHttpAction(const HttpResponse &response)
{
    _lastHttpResponse = response;

    Serial.println("[HTTP] Action: " + String(response.statusCode));

    Serial.println("        Success: " + String(response.success ? "YES" : "NO"));
    Serial.println("        Data length: " + String(response.dataLength) + " bytes");
    Serial.println("        Response code: " + String(response.statusCode));
    
    if(_instance->_currentHttpAction == httpActionType::LOG_UPLOAD)
    {
        logger.log('I', "[HTTP] Action 3: " + String(response.statusCode));
    }
    else if(_instance->_currentHttpAction == httpActionType::LMS_API)
    {
        logger.log('I', "[HTTP] Action 2: " + String(response.statusCode));
    }
    _instance->_currentHttpAction = httpActionType::NONE;
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

CommandHandler::CommandHandler(SensorManager &sensorMgr, StorageManager &storageMgr, GSM_OTA &gsmOta, A7670C &modem, DataLogger &dataLogger, powerMonitor &power)
    : sensorMgr(sensorMgr), storageMgr(storageMgr), gsmOta(gsmOta), modem(modem), logger(dataLogger), power(power)
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
    // _topic = String("lms/devices/") + _deviceMac + "/configuration/send";
    _topic = String("lms/devices/") + tid + "/configuration/send";

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

    tid = storageMgr.getTid();
    
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

    xTaskCreate(
        reconnectMqttTask,
        "ReconnectMqtt",
        MQTT_RECONNECT_TASK_STACK_SIZE,
        this,
        MQTT_RECONNECT_TASK_PRIORITY,
        &mqttReconnectHandle);
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
    vTaskDelay(pdMS_TO_TICKS(7000));

    while (true)
    {
        while (self->logger.isReadyForUpload())
        {
            PacketData lastPacket = self->logger.getPacket(LOG_DATA_UPLOAD_SIZE);
            String payload = String(lastPacket.data);

            payload = self->urlEncodeSpaces(payload);
            
            String uploadUrl = String(LOG_UPLOAD_URL) + self->getDeviceMac() + "&tid=" + String(self->tid) + "&data=" + payload + "&fileName=" + String(self->getDeviceMac()) + "_" + String(self->tid);
            
            self->modem.hitHttpGetRequest(uploadUrl);
            self->_currentHttpAction = httpActionType::LOG_UPLOAD;
            Serial.println("[LOG] upload URL: " + uploadUrl);

            while (self->modem.getHttpInAction())
            {
                // Serial.println("[LOG] HTTP in action...");
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            if (self->_lastHttpResponse.success)
            {
                PacketData lastPacket = self->logger.getPacket(LOG_DATA_UPLOAD_SIZE);
                self->logger.markUploaded(lastPacket.endOffset);
            }
            else
            {
                Serial.println("[LOG] upload failed, will retry later");
                break;
            }
        }

        String apiUrl = self->sensorMgr.generateApiUrl(self->tid);
        Serial.println("[API] URL: " + apiUrl);
        self->modem.hitHttpGetRequest(apiUrl);
        self->_currentHttpAction = httpActionType::LMS_API;

        vTaskDelay(pdMS_TO_TICKS(API_HIT_INTERVAL_MS));
    }
    vTaskDelete(nullptr);
}

void CommandHandler::reconnectMqttTask(void *param)
{
    CommandHandler *self = static_cast<CommandHandler *>(param);
    vTaskDelay(pdMS_TO_TICKS(10000));

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(20000));
        int count = 0;

        while (!self->modem.getMQTTConnected() && !self->modem.getModuleReset())
        {
            self->modem.mqttConnect();
            if (self->modem.mqttSubscribe(self->getTopic()))
            {
                String formatted;
                self->modem.setTime(formatted);
            }

            vTaskDelay(pdMS_TO_TICKS(10000));

            if (count++ > 5)
            {
                Serial.println("[MQTT] Failed, resetting GSM module");
                self->logger.log('E', "[MQTT] Failed, resetting GSM module");
                self->modem.moduleReset();
                count = 0;
            }
        }
    }
    vTaskDelete(nullptr);
}

String CommandHandler::urlEncodeSpaces(const String &input)
{
    String output;
    output.reserve(input.length() + 20);

    for (size_t i = 0; i < input.length(); i++)
    {
        if (input[i] == ' ')
            output += "%20";
        else
            output += input[i];
    }

    return output;
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

    logger.log('I', cmdStr);

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
    else if(strcmp(cmdStr, "battery_status") == 0)
    {
        handleBatteryStatus(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "solar_status") == 0)
    {
        handleSolarStatus(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "system_power") == 0)
    {
        handleSystemPower(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "system_info") == 0)
    {
        getSystemInfo(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "send_sms") == 0)
    {
        handleSendSMS(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "save_tid") == 0)
    {
        handleSaveTid(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "gsm_reset") == 0)
    {
        handleResetGsm(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "update_device") == 0)
    {
        handleOtaUpdate(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "log_data") == 0)
    {
        handleLogData(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "mpu_reset") == 0)
    {
        handleMpuReset(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "rain_reset") == 0)
    {
        handleRainReset(doc, pkt.formBle, pkt.formMqtt);
    }
    else if(strcmp(cmdStr, "hit_api") == 0)
    {
        String apiUrl = sensorMgr.generateApiUrl(tid);
        Serial.println("[API] URL: " + apiUrl);

        modem.hitHttpGetRequest(apiUrl);
        
        doc["status"] = "ok";
        doc["msg"] = "Packet sent to API";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "log_info") == 0)
    {
        Serial.println("  --- Log Info ---");
        LoggerInfo info = logger.getInfo();
        Serial.printf("  File size    : %u B\n", info.currentSize);
        Serial.printf("  Max size     : %u B\n", info.maxSize);
        Serial.printf("  Uploaded off : %u B\n", info.uploadedOffset);
        Serial.printf("  Pending      : %u B\n", info.pendingBytes);
        Serial.printf("  Full         : %s\n", info.full ? "YES !!!" : "no");
        Serial.println("  ---------------");

        doc["status"] = "ok";
        doc["msg"] = "Log info retrieved";
        doc["current_size"] = info.currentSize;
        doc["max_size"] = info.maxSize;
        doc["uploaded_offset"] = info.uploadedOffset;
        doc["pending_bytes"] = info.pendingBytes;
        doc["full"] = info.full;
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "log_clear") == 0)
    {
        PacketData lastPacket        = logger.getPacket(LOG_DATA_MAX_SIZE);
        // if (!lastPacketPending)
        // {
        //     Serial.println("  No packet pending – run 'packet' first.");
        //     return;
        // }
        bool ok = logger.markUploaded(lastPacket.endOffset);
        Serial.printf("  markUploaded(%u) -> %s\n", lastPacket.endOffset, ok ? "OK" : "FAIL");
        // lastPacketPending = false;
        // printInfo();
        Serial.println("STRESS DONE");

        doc["status"] = "ok";
        doc["msg"] = "Log cleared successfully";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
    else if (strcmp(cmdStr, "log_reset") == 0)
    {
        bool ok = logger.clear();

        if (ok)
        {
            doc["status"] = "ok";
            doc["msg"] = "Log reset successfully";
            logger.log('I', "[LOGGER] reset success");
        }
        else
        {
            doc["status"] = "error";
            doc["msg"] = "Log reset failed";
            logger.log('E', "[LOGGER] reset failed");
        }
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
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
        doc["msg"] = "Unknown command";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
    }
}

void CommandHandler::sendResponse(JsonDocument &response, bool toBle, bool toMqtt)
{
    // String topic = String("lms/devices/") + _deviceMac + "/configuration/callback";
    String topic = String("lms/devices/") + tid + "/configuration/callback";
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
    String time, ip;
    getTimeStr(time);
    modem.getSignalStrength(rssi);
    modem.getIp(ip);

    doc["uptime_s"] = esp_timer_get_time() / 1000000ULL;
    doc["tid"] = tid;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["mqtt_connected"] = modem.getMQTTConnected();
    doc["mac"] = _deviceMac;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["device_name"] = DEVICE_NAME;
    doc["current_time"] = time;
    doc["rssi"] = rssi;
    doc["ip"] = ip;

    doc["status"] = "ok";
    doc["msg"] = "System information retrieved";

    sendResponse(doc, fromBle, fromMqtt);

    return true;
}

time_t CommandHandler::getTimeStr(String &formatted)
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
    doc["msg"] = "Starting OTA update...";
    sendResponse(doc, fromBle, fromMqtt);
    
    // modem.mqttDisconnect();
    vTaskSuspend(apiHandle);
    vTaskSuspend(mqttReconnectHandle);
    // vTaskSuspend(gsmHandle);
    
    while (modem.getHttpInAction())
    {
        Serial.println("[OTA] HTTP in action...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskSuspend(modem.rxTaskHandle);

    gsmOta.setAPN(APN);
    gsmOta.setChunkSize(1024); // optional – 1024 is default
    gsmOta.setDebugEnabled(true);

    String url = doc["url"] | OTA_URL;
    OTAResult result = gsmOta.performOTA(url.c_str());

    // setupMqtt();
    vTaskResume(modem.rxTaskHandle);

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

void CommandHandler::handleBatteryStatus(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    power.getBatteryStatusJson(doc);

    doc["status"] = "ok";
    doc["msg"] = "Battery data retrieved";
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleSolarStatus(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    power.getSolarStatusJson(doc);
    doc["status"] = "ok";
    doc["msg"] = "Solar data retrieved";
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleSystemPower(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    power.getSystemPowerJson(doc);
    doc["status"] = "ok";
    doc["msg"] = "System power data retrieved";
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleSendSMS(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    String number = doc["num"] | "";
    String message = doc["msg"] | "Land Slide Monitoring Live";

    if (number == "" || message == "")
    {
        doc["status"] = "error";
        doc["error"] = "missing_number_or_message";
        sendResponse(doc, fromBle, fromMqtt);
        return;
    }

    bool success = modem.sendSms(number, message);

    if (success)
    {
        doc["status"] = "ok";
        doc["msg"] = "SMS sent successfully";
    }
    else
    {
        doc["status"] = "error";
        doc["error"] = "failed_to_send_sms";
        doc["msg"] = "Failed to send SMS";
    }
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleSaveTid(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    String newTid = doc["tid"] | "";
    if (newTid == "")
    {
        doc["status"] = "error";
        doc["error"] = "invalid_tid";
        doc["msg"] = "TID is missing";
        sendResponse(doc, fromBle, fromMqtt);
        return;
    }

    tid = newTid;
    storageMgr.saveTid(tid);

    doc["status"] = "ok";
    doc["msg"] = String("TID updated to ") + tid;
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleResetGsm(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    modem.setModuleReset(true);

    doc["msg"] = "GSM module reseting...";
    sendResponse(doc, fromBle, fromMqtt);

    if(modem.moduleReset())
    {
        modem.mqttConnect();
        modem.mqttSubscribe(getTopic());

        doc["status"] = "ok";
        doc["msg"] = "GSM module back online";
    }
    else
    {
        doc["status"] = "error";
        doc["msg"] = "Failed to reset GSM module";
    }
    modem.setModuleReset(false);
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleLogData(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
        size_t sz = LOG_DATA_MAX_SIZE;
        PacketData lastPacket;
        bool lastPacketPending = false;
        Serial.print("GET PACKET");
        lastPacket = logger.getPacket(sz);
        lastPacketPending = (lastPacket.packetSize > 0);

        if (lastPacket.packetSize == 0)
        {
            Serial.println("  (no pending data)");
            return;
        }

        Serial.printf("  Offset range : [%u .. %u]\n", lastPacket.startOffset, lastPacket.endOffset);
        Serial.printf("  Packet size  : %u B\n", lastPacket.packetSize);
        Serial.println("  --- payload ---");
        Serial.print(lastPacket.data);
        Serial.println("  ---------------");

        doc["status"] = "ok";
        doc["packet_size"] = lastPacket.packetSize;
        doc["start_offset"] = lastPacket.startOffset;
        doc["end_offset"] = lastPacket.endOffset;
        doc["data"] = String(lastPacket.data);
        doc["msg"] = "Data packet retrieved";
        sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleMpuReset(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    sensorMgr.resetMotionCount();

    doc["status"] = "ok";
    doc["msg"] = "MPU motion count reset done";
    sendResponse(doc, fromBle, fromMqtt);
}

void CommandHandler::handleRainReset(JsonDocument &doc, bool fromBle, bool fromMqtt)
{
    sensorMgr.resetRainCount();

    doc["status"] = "ok";
    doc["msg"] = "Rain count reset done";
    sendResponse(doc, fromBle, fromMqtt);
}