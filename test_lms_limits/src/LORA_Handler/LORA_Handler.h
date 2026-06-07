#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "LoRa.h"
#include "SPI.h"
#include "config.h"

// === LORA ALARAM OLD CONFIG ===
#define RH_TO 0xFF
#define RH_FROM 0xFF
#define RH_ID 0x00
#define RH_FLAGS 0x00

bool setupLoRa();
void sendLoraAlaram();
void sendLoraAlaram_old();

bool TestAlaram(uint8_t id);
bool TestHooters(uint8_t id);
bool SetHooterID(uint8_t id);

#endif // LORA_HANDLER_H