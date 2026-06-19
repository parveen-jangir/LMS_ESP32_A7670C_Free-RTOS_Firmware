#include <SPI.h>
// #include <RH_RF95.h>
#include <LoRa.h>

// Singleton instance of the radio driver
// RH_RF95 SX1278;  // <--- usually RH_RF95 rf95(10, 2); on Uno - check your pins
#define LORA_SS 10
#define LORA_RST 9
#define LORA_DIO0 2

#define ALARM 8
#define LIGHT 9
#define LED 6
#define LED1 5

void onSequence() {
  digitalWrite(LED, HIGH);
  digitalWrite(LED1, LOW);
  delay(200);
  digitalWrite(LED, LOW);
  digitalWrite(LED1, HIGH);
  delay(200);
  digitalWrite(LED, HIGH);
  digitalWrite(LED1, HIGH);
  delay(300);
  digitalWrite(LED, LOW);
  digitalWrite(LED1, LOW);
  delay(300);
  digitalWrite(LED, HIGH);
  digitalWrite(LED1, HIGH);
  delay(300);
  digitalWrite(LED, LOW);
  digitalWrite(LED1, LOW);
}

void Trigger_hooter() {
  Serial.println("→ Control signal received!");
  onSequence();
  digitalWrite(LED, HIGH);
  digitalWrite(LED1, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);
  digitalWrite(LED1, LOW);
  delay(100);

  // Beep + light pattern (4 long + short sequence)
  for (int i = 0; i < 4; i++) {
    digitalWrite(ALARM, HIGH);
    digitalWrite(LIGHT, HIGH);
    delay(2000);
    digitalWrite(ALARM, LOW);
    digitalWrite(LIGHT, LOW);
    delay(100);
  }

  // Final short beep + light off
  digitalWrite(ALARM, HIGH);
  digitalWrite(LIGHT, LOW);
  delay(500);
  digitalWrite(ALARM, LOW);
  digitalWrite(LIGHT, LOW);
  delay(500);
  digitalWrite(LED, LOW);
  digitalWrite(LED1, LOW);

  Serial.println("→ Sequence completed");
}

void setup() {

  Serial.begin(9600);
  delay(100);

  pinMode(LED, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(ALARM, OUTPUT);
  pinMode(LIGHT, OUTPUT);

  digitalWrite(LED, LOW);
  digitalWrite(LED1, LOW);
  digitalWrite(ALARM, LOW);
  digitalWrite(LIGHT, LOW);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  // Alaram
  onSequence();

  Serial.println();
  Serial.println("LoRa Receiver starting...");

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed !!! Check wiring / module");
    while (true) {
      digitalWrite(LED, !digitalRead(LED));
      delay(200);
    }
  }

  Serial.println("LoRa init OK");
  Serial.println("Waiting for packets...");
  digitalWrite(LED, HIGH);
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String incoming = "";

    // Read packet payload
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Clean up any hidden newlines or carriage returns
    incoming.trim();

    Serial.print("Received packet: '");
    Serial.print(incoming);
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
    
    String expectedPayload = "Alaram";
    
    // Now this will match correctly
    if (incoming.equals(expectedPayload)) {
      Trigger_hooter();
    } else {
      Serial.println("Unkonwn command");
    }
  }
}