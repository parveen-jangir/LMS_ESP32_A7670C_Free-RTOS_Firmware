/*
 * DataLogger – Example Arduino Sketch
 *
 * Demonstrates:
 *   - Initialising the logger
 *   - Writing log entries
 *   - Reading a packet and "uploading" it
 *   - Printing logger info
 *
 * Replace the upload stub with your MQTT / HTTP / GSM send call.
 */

#include <Arduino.h>
#include "DataLogger.h"

DataLogger logger;

// ---------------------------------------------------------------------------
// Helper: build a compact timestamp string "MMDD,HHMMSS"
// Replace with your NTP / RTC source.
// ---------------------------------------------------------------------------
String makeTimestamp()
{
    // Dummy fixed timestamp for this example
    return "0610,153025";
}

// ---------------------------------------------------------------------------
// Stub: send data to server; returns true on ACK
// ---------------------------------------------------------------------------
bool uploadPacket(const String& data, size_t size)
{
    Serial.printf("[UPLOAD] %u bytes:\n%s\n", size, data.c_str());
    // TODO: replace with mqtt.publish / http.POST / gsm.send …
    return true;   // pretend server ACK'd
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(500);

    if (!logger.begin("/system.log", "/system.idx", 102400))
    {
        Serial.println("DataLogger init failed!");
        while (true) delay(1000);
    }
    Serial.println("DataLogger ready.");

    // Write a few test entries
    logger.log(makeTimestamp(), 'I', "Device booted");
    logger.log(makeTimestamp(), 'I', "WiFi Connected");
    logger.log(makeTimestamp(), 'E', "MQTT Timeout");
    logger.log(makeTimestamp(), 'W', "Battery low");
    logger.log(makeTimestamp(), 'D', "Sensor reading: 42");

    // Print current state
    LoggerInfo info = logger.getInfo();
    Serial.printf("Size: %u / %u  |  Uploaded: %u  |  Pending: %u  |  Full: %s\n",
        info.currentSize, info.maxSize,
        info.uploadedOffset, info.pendingBytes,
        info.full ? "YES" : "NO");
}

// ---------------------------------------------------------------------------
// loop – drain pending logs one packet at a time
// ---------------------------------------------------------------------------
void loop()
{
    PacketData pkt = logger.getPacket(512);

    if (pkt.packetSize > 0)
    {
        Serial.printf("Packet [%u..%u]  %u bytes\n",
            pkt.startOffset, pkt.endOffset, pkt.packetSize);

        bool ack = uploadPacket(pkt.data, pkt.packetSize);

        if (ack)
        {
            logger.markUploaded(pkt.endOffset);
            Serial.println("Marked uploaded.");
        }
        else
        {
            Serial.println("Upload failed, will retry.");
        }
    }
    else
    {
        Serial.println("No pending logs.");
        delay(10000);   // wait before checking again
    }

    delay(2000);
}
