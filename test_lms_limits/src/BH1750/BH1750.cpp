#include "BH1750.h"

BH1750Sensor::BH1750Sensor(TwoWire* wireInterface)
    : wire(wireInterface), address(BH1750_ADDRESS), isInitialized(false),
      calibrationOffset(BH1750_DEFAULT_OFFSET) {
    lastReading = {0, 0, false, 0};
}

bool BH1750Sensor::initialize() {
    if (!wire) return false;
    
    wire->begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(100);
    
    // Check if BH1750 is present
    wire->beginTransmission(address);
    if (wire->endTransmission() != 0) {
        if (DEBUG_ENABLED) Serial.println("[BH1750] Sensor not found!");
        return false;
    }
    
    // Power on and set measurement mode (0x10 = High Resolution Mode)
    wire->beginTransmission(address);
    wire->write(0x01);  // Power on
    wire->endTransmission();
    delay(10);
    
    wire->beginTransmission(address);
    wire->write(0x10);  // High resolution mode 1
    wire->endTransmission();
    
    isInitialized = true;
    lastReading.isValid = true;
    
    if (DEBUG_ENABLED) Serial.println("[BH1750] Initialized successfully");
    return true;
}

bool BH1750Sensor::readSensor() {
    if (!isInitialized) return false;
    
    lastReading.timestamp = millis();
    
    // Request measurement
    wire->beginTransmission(address);
    wire->write(0x10);  // High resolution mode 1
    if (wire->endTransmission() != 0) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Read data (2 bytes)
    if (wire->requestFrom(address, (uint8_t)2, (uint8_t)1) != 2) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    uint16_t raw = ((uint16_t)wire->read() << 8) | wire->read();
    
    // Convert to lux (formula: raw / 1.2)
    lastReading.illuminance = (raw / 1.2f) + calibrationOffset;
    lastReading.isValid = true;
    lastReading.errorCount = 0;
    
    return true;
}

void BH1750Sensor::enable() {
    isInitialized = true;
}

void BH1750Sensor::disable() {
    isInitialized = false;
}

bool BH1750Sensor::isEnabled() const {
    return isInitialized;
}

BH1750Data BH1750Sensor::getLastReading() const {
    return lastReading;
}

void BH1750Sensor::setOffset(float offset) {
    calibrationOffset = offset;
}

float BH1750Sensor::getOffset() const {
    return calibrationOffset;
}

bool BH1750Sensor::isValid() const {
    return lastReading.isValid;
}

uint8_t BH1750Sensor::getErrorCount() const {
    return lastReading.errorCount;
}