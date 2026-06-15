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
#include "A7670C/A7670C.h"
#include "config.h"

HardwareSerial gsmSerial(2);

SensorManager sensorManager;
GSM_OTA gsmOta(gsmSerial, Serial);
StorageManager STManager;
A7670C modem(gsmSerial, MODEM_PWR_PIN);
CommandHandler cmdHandler(sensorManager, STManager, gsmOta, modem);

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
    Serial.println(F("Initializing Sensor Manager..."));

    gsmSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    cmdHandler.begin();

    gsmOta.onProgress(onProgress);
    sensorManager.readAllSensors();
    sensorManager.printLastReadings();

    // setupLoRa();
    // sendLoraAlaram_old();
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop()
{
    // sendLoraAlaram_old();
    // if (digitalRead(MPU_INTERRUPT_PIN))
    // {
        sensorManager.readAllSensors();
        sensorManager.printLastReadings();
    // }
    // Serial.println(digitalRead(MPU_INTERRUPT_PIN));
    vTaskDelay(pdMS_TO_TICKS(10000));


    // Serial.printf("Pin 33: %d\n", digitalRead(33));
    // if(digitalRead(33) == LOW) {
        // Serial.println("Pin 33 is LOW - Triggering sensor read");
        // sensorManager.readAllSensors();
        // sensorManager.printLastReadings();
    // }
// vTaskDelay(pdMS_TO_TICKS(100)); // fast print
}
