/*
 * GSM_OTA — Example sketch for ESP32-S3 + A7670C
 *
 * Wiring:
 *   ESP32-S3 RX  (GPIO16) --> A7670C TX
 *   ESP32-S3 TX  (GPIO17) --> A7670C RX
 *   GND <--> GND
 *
 * Serial Monitor:
 *   Type  "update"  → start OTA
 *   Anything else   → forwarded to GSM module (passthrough)
 */
#include <Arduino.h>
#include "GSM_OTA/GSM_OTA.h"

// ─── Config ───────────────────────────────────────────────────────────────────
#define GSM_RX_PIN   16
#define GSM_TX_PIN   17
#define GSM_BAUD     115200

// ⚠️  Use raw.githubusercontent.com — NOT github.com/…/raw/refs/…
//     The github.com URL redirects (302) and A7670C cannot follow redirects.
const char* OTA_URL = "https://raw.githubusercontent.com/parveen-jangir/mqtt_bin_file/main/test_esp_ota.bin";
// const char* OTA_URL = "https://raw.githubusercontent.com/parveen-jangir/mqtt_bin_file/refs/heads/main/esp32_s3_test_ota.bin";
const char* APN     = "airtelgprs.com";

HardwareSerial gsm(2);  // Use UART2 for GSM

// ─── Library instance ─────────────────────────────────────────────────────────
GSM_OTA ota(gsm, Serial);

// ─── Optional callbacks ───────────────────────────────────────────────────────
void onProgress(int percent, int written, int total)
{
    // Draw a simple progress bar on Serial
    Serial.printf("  [");
    int filled = percent / 5;                // 20 chars wide
    for (int i = 0; i < 20; i++) Serial.print(i < filled ? '=' : ' ');
    Serial.printf("] %d%%  (%d/%d bytes)\n", percent, written, total);
}

void onLog(const char* msg)
{
    // All library logs come here too (already printed by the library itself
    // if debugEnabled=true, so you can use this to forward to MQTT / SD card)
    (void)msg;
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Init GSM UART
    ota.begin(GSM_BAUD, GSM_RX_PIN, GSM_TX_PIN);

    // Configure
    ota.setAPN(APN);
    ota.setChunkSize(512);           // optional – 512 is default
    ota.setDebugEnabled(true);       // print logs to Serial

    // Register callbacks (optional)
    ota.onProgress(onProgress);
    ota.onLog(onLog);

    Serial.println("================================");
    Serial.println(" ESP32-S3 GSM-OTA Example");
    Serial.println(" Type 'update' → start OTA");
    Serial.println(" Anything else → GSM passthrough");
    Serial.println("================================");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop()
{
    // ── Serial → GSM passthrough / OTA trigger ────────────────────────────────
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("update")) {
            Serial.println("\n>> OTA triggered");
            OTAResult result = ota.performOTA(OTA_URL);

            if (result == OTA_SUCCESS) {
                Serial.println(">> OTA done! Rebooting in 3 s...");
                delay(3000);
                esp_restart();
            } else {
                Serial.printf(">> OTA FAILED: %s (code %d)\n",
                              GSM_OTA::resultToString(result), (int)result);
            }

        } else {
            gsm.println(input);   // forward to GSM
        }
    }

    // ── GSM → Serial passthrough ──────────────────────────────────────────────
    while (gsm.available()) {
        Serial.write(gsm.read());
    }
}
