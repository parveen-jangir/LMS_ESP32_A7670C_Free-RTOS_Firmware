#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "SensorManager/SensorManager.h"
#include "local_storage/storage_manager.h"
#include "command_handler/command_handler.h"
#include "GSM_OTA/GSM_OTA.h"
#include "LORA_Handler/LORA_Handler.h"
#include "A7670C_handler/A7670C.h"
#include "config.h"
#include "DataLogger/DataLogger.h"
#include "power_handler/power_handler.h"

HardwareSerial gsmSerial(2);

DataLogger dataLogger;
powerMonitor power;
StorageManager STManager(dataLogger);
SensorManager sensorManager(dataLogger);
GSM_OTA gsmOta(gsmSerial, Serial, dataLogger);
A7670C modem(gsmSerial, MODEM_PWR_PIN, dataLogger);
CommandHandler cmdHandler(sensorManager, STManager, gsmOta, modem, dataLogger, power);

// static int lastReportedPercent = 0;

void onProgress(int percent, int written, int total)
{
    // Draw a simple progress bar on Serial
    Serial.printf("  [");
    int filled = percent / 5; // 20 chars wide
    for (int i = 0; i < 20; i++)
        Serial.print(i < filled ? '=' : ' ');
    Serial.printf("] %d%%  (%d/%d bytes)\n", percent, written, total);

    // uint8_t milestone = (percent / 5) * 5;

    // if (milestone >= 5 && milestone <= 100 && milestone > lastReportedPercent)
    // {
    //     lastReportedPercent = milestone;

    //     JsonDocument doc;
    //     doc["type"] = "ota_progress";
    //     doc["progress"] = milestone;

    //     cmdHandler.sendResponse(doc, true, true);
    // }
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    Serial.println("\n=== LANDSLIDE MONITORING SYSTEM ===");

    gsmSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    if (!dataLogger.begin("/system.log", "/system.idx", 65536))
    {
        Serial.println("[LOGGER] Failed to initialize");
    }
    else
    {
        Serial.println("[LOGGER] Initialized");
        dataLogger.log('I', "[BOOT] Device booted");
    }

    if (sensorManager.initialize())
    {
        Serial.println(F("[MAIN] Sensor initialized"));
        dataLogger.log('I', "[MAIN] Sensor initialized");
        sensorManager.confSensorWithDefaults();
        sensorManager.readAllSensors();
        sensorManager.printLastReadings();
    }
    else
    {
        Serial.println(F("[MAIN] Sensor init failed"));
        dataLogger.log('E', "[MAIN] Sensor init failed");
    }

    if (!power.begin()) {
        Serial.println("[POWER] Failed to init");
        dataLogger.log('E', "[POWER] Failed to init");
    }

    modem.begin();

    cmdHandler.begin();

    gsmOta.onProgress(onProgress);

    // setupLoRa();
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(20000));
    

}

