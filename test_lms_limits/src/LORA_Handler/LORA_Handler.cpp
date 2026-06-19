#include "LORA_Handler.h"

bool LoRaEnabled = false;
bool ACK_Recv = false;
uint8_t payload[] = {'1', 0x00};

void LoraCallback(int packetSize)
{
    Serial.print("[LORA] Received packet with RSSI: ");
    Serial.println(LoRa.packetRssi());
    String msg = "";

    for (int i = 0; i < packetSize; i++)
    {
        char c = (char)LoRa.read();
        Serial.print(c);
        msg += c;
    }

    if (msg == "ACK")
    {
        ACK_Recv = true;
    }
}

bool setupLoRa()
{
    LoRa.setPins(LORA_NSS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
    uint8_t count = 0;
    while (!LoRa.begin(LORA_FREQUENCY))
    {
        Serial.print("[LORA] Connecting...");
        Serial.println(count);
        vTaskDelay(pdMS_TO_TICKS(500));
        if (++count > 10)
        {
            Serial.println("[LORA] init failed. Check your connections.");
            return false;
        }
    }

    // Setting Lora Callback function on recived data
    LoRa.onReceive(LoraCallback);
    LoRaEnabled = true;
    Serial.println("[LORA] init succeeded.");
    return true;
}

void sendLoraAlaram_old() // Triggierg hooter with old code
{
    Serial.println("[LORA] Sending Alaram OLD...");
    uint8_t payloadLen = sizeof(payload);
    LoRa.beginPacket();
    LoRa.write(RH_TO);
    LoRa.write(RH_FROM);
    LoRa.write(RH_ID);
    LoRa.write(RH_FLAGS);
    LoRa.write(payload, payloadLen);
    LoRa.endPacket();
}

bool sendLoraAlaram()
{
    bool loRaSetup = setupLoRa();

    Serial.println("[LORA] Sending Alaram...");
    LoRa.beginPacket();
    LoRa.println("Alaram"); // Trigger all the Hooter
    LoRa.endPacket();

    vTaskDelay(pdMS_TO_TICKS(100)); // cooldown period to avoid flooding
     LoRa.beginPacket();
    LoRa.println("Alaram"); // Trigger all the Hooter
    LoRa.endPacket();

    LoRa.end();

    return loRaSetup; // Assuming the send operation is always successful for this example
}

// Test function to send a message with the Hooters ID
// ACK
// delay of 1 sec to set recivere
// Hooters ID:<id>
// Then recive ACK form hooter
// which makes ACK_Recv to true

bool TestHooters(uint8_t id)
{
    LoRa.beginPacket();
    LoRa.print("ACK"); // Sending ACK to trigger response from hooter
    LoRa.endPacket();

    vTaskDelay(pdMS_TO_TICKS(1000)); // cooldown period to avoid flooding

    LoRa.beginPacket();
    LoRa.print("ID:");
    LoRa.println(id);
    LoRa.endPacket();

    uint8_t count = 0;
    while (count < 10)
    {
        if (ACK_Recv == true)
        {
            break; // break if ack becones true
        }
        else
        {
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // small delay to avoid busy waiting
    }

    if (ACK_Recv == false) // if ack not recived
    {
        return false;
    }

    ACK_Recv = false; // resetting variable
    return true;
}

bool TestAlaram(uint8_t id)
{
    LoRa.beginPacket();
    LoRa.print("Test_Alaram"); // Setting Test Alaram to 1
    LoRa.endPacket();

    vTaskDelay(pdMS_TO_TICKS(1000)); // cooldown period to avoid flooding

    LoRa.beginPacket();
    LoRa.print("ID:");
    LoRa.println(id);
    LoRa.endPacket();

    uint8_t count = 0;
    while (count < 10)
    {
        if (ACK_Recv == true)
        {
            break; // break if ack becones true
        }
        else
        {
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // small delay to avoid busy waiting
    }

    if (ACK_Recv == false) // if ack not recived
    {
        return false;
    }

    ACK_Recv = false; // resetting variable
    return true;
}

// Set_ID
// delay of 1 sec to set recivere
// ID:<id>
// Then recive ACK form hooter
// which makes ACK_Recv to true
bool SetHooterID(uint8_t id)
{
    Serial.print("[LORA] Setting Hooter ID to: ");
    Serial.println(id);

    LoRa.beginPacket();
    LoRa.print("Set_ID"); // Sending Command to set ID
    LoRa.endPacket();

    LoRa.beginPacket();
    LoRa.print("ID:");
    LoRa.println(id);
    LoRa.endPacket();

    uint8_t count = 0;
    while (count < 10)
    {
        if (ACK_Recv == true)
        {
            break; // break if ack becones true
        }
        else
        {
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // small delay to avoid busy waiting
    }

    if (ACK_Recv == false) // if ack not recived
    {
        return false;
    }

    ACK_Recv = false; // resetting variable
    return true;
}
