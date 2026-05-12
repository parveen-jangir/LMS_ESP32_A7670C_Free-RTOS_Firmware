// ============================================================
// main.cpp — ESP32 Multi-Sensor Hub
// Framework: Arduino / PlatformIO
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "config.h"
#include "sensor_manager.h"


SensorManager sensor;

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== ESP32 Multi-Sensor Hub ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    sensor.begin();
    sensor.update();


    Serial.println("[MAIN] Setup complete — entering loop");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  
       sensor.update();
       delay(500);

    // Yield to RTOS — avoid WDT resets
    yield();
}
