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

void setup() {
    Serial.begin(115200);
    delay(200);
    
    Serial.println("\n=== LANDSLIDE MONITORING SYSTEM ===");
    Serial.println(F("Initializing Sensor Manager..."));

    gsmSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    cmdHandler.begin();

    if (sensorManager.initialize()) {
        Serial.println(F("[MAIN] Sensor initialized"));
        sensorManager.printSensorStatus();
        cmdHandler.configSensors();
    } else {
        Serial.println(F("[MAIN] Sensor initialization failed"));
    }

    // setupLoRa();
    // sendLoraAlaram_old();

}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop()
{
    // sendLoraAlaram_old();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
