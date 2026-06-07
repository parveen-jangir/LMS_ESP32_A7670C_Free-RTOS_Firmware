#include "command_handler.h"

void CommandHandler::mqttCallback(const String &topic, const String &payload)
{
    Serial.println("  📨  MQTT MESSAGE RECEIVED");
    Serial.println("  Topic   : " + topic);
    Serial.println("  Payload : " + payload);
}

void CommandHandler::onSmsReceived(SmsMessage msg) {
    Serial.println("  📩  INCOMING SMS");
    Serial.println("  From      : " + msg.sender);
    Serial.println("  Timestamp : " + msg.timestamp);
    Serial.println("  Status    : " + msg.status);
    Serial.println("  Text      : " + msg.text);
}

void CommandHandler::onIncomingCall(const String& number) {
    Serial.println("  📞  INCOMING CALL");
    Serial.println("  From : " + (number.isEmpty() ? "Unknown / withheld" : number));
    Serial.println();
    Serial.println("  Type  A  → Answer");
    Serial.println("  Type  H  → Reject / Hang up");
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

  Serial.println("Device MAC: " + _deviceMac);
}

void CommandHandler::buildTopic()
{
  _topic = String("lms/devices/") + _deviceMac + "/configuration/callback";

  Serial.println("MQTT Topic: " + _topic);
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
    if (workerHandle) {
        vTaskDelete(workerHandle);
    }
    if (cmdQueue) {
        vQueueDelete(cmdQueue);
    }
}

void CommandHandler::begin()
{
    xTaskCreate(
        workerTask, 
        "CmdWorker", 
        COMMAND_TASK_STACK_SIZE, 
        this, 
        COMMAND_TASK_PRIORITY, 
        &workerHandle
    );

    // configSensors();

    modem.onMqttMessage(mqttCallback);
    modem.onSms(onSmsReceived);
    modem.onRing(onIncomingCall);

    // Enable AT traffic debug output
    modem.setDebug(Serial);
    modem.setDebugEnabled(true);   // ← set false to hide raw AT traffic

    buildDeviceMac();
    buildTopic();

    if (!modem.begin()) {
        Serial.println("[GSM] Failed initialization");
    } else {
        Serial.println("[GSM] Modem initialized");
        setupMqtt();
    }

    while (true)
    {
        modem.loop();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void printResult(A7670C_Result r) {
    switch (r) {
        case A7670C_Result::OK:         Serial.println("OK");         break;
        case A7670C_Result::ERROR:      Serial.println("ERROR");      break;
        case A7670C_Result::TIMEOUT:    Serial.println("TIMEOUT");    break;
        case A7670C_Result::HTTP_ERROR: Serial.println("HTTP_ERROR"); break;
        case A7670C_Result::MQTT_ERROR: Serial.println("MQTT_ERROR"); break;
        default:                        Serial.println("UNKNOWN");    break;
    }
}

void CommandHandler::setupMqtt()
{
    A7670C_Result res = modem.mqttConfigure(_deviceMac, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    res = modem.mqttConnect();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    res = modem.mqttSubscribe(_topic, 0);

    Serial.print("[CMD] MQTT Configure Result:");
    printResult(res);
}

void CommandHandler::workerTask(void *param)
{
    CommandHandler *handler = static_cast<CommandHandler*>(param);
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
        // doc["status"] = mqtt.isConnected() ? "ONLINE" : "OFFLINE";
        sendResponse(doc, pkt.formBle, pkt.formMqtt);
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
    if (toBle)
    {
        // Serialise to a stack buffer and send via BLE notify.
        // char buf[MAX_PAYLOAD_LEN + 1];
        // size_t len = serializeJson(response, buf, sizeof(buf));
        // if (len == 0)
        // {
        //     Serial.println(F("[CMD] sendResponse(BLE): serialise failed"));
        //     return;
        // }
        // if (!ble.notifyStr(buf))
        // {
        //     Serial.println(F("[CMD] sendResponse(BLE): notify failed"));
        // }
    }
    else if (toMqtt)
    {
        // if (!mqtt.publish(response))
        // {
        //     Serial.println(F("[CMD] sendResponse(MQTT): publish failed"));
        // }
    }

    Serial.print(F("[CMD] sendResponse: "));
    serializeJson(response, Serial);
}

void CommandHandler::configSensors()
{
    // Default calibration setup
    Serial.println("[SENS] CONFIGURING SENSORS...");
    
    // BMP180 calibration
    sensorMgr.setBMP180CalibrationOffset(0.0, 0.0);
    
    // BH1750 calibration
    sensorMgr.setBH1750CalibrationOffset(0.0);
    
    // MPU6050 calibration
    sensorMgr.setMPU6050CalibrationOffset(
        0.0, 0.0, 0.0,  // Accel offsets
        0.0, 0.0, 0.0,  // Gyro offsets
        0.0              // Temp offset
    );
    
    // DHT22 calibration
    sensorMgr.setDHT22CalibrationOffset(0.0, 0.0);
    
    // Soil Moisture calibration (0=dry, 4095=wet)
    sensorMgr.setSoilMoistureCalibrationOffset(0, 4095);
    
    // Rain Gauge calibration (0.2794 mm per tip)
    sensorMgr.setRainGaugeTipVolume(0.2794);
    
    // Set read interval
    sensorMgr.setReadInterval(5000);
    
    Serial.println("[SENS] CONFIG DONE");
    // Start continuous reading task
    if (!sensorMgr.startReadingTask()) {
        Serial.println("[SENS][ERROR] Failed to start sensor task!");
    }

    sensorMgr.printSensorStatus();
}

