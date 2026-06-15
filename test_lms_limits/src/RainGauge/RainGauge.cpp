#include "RainGauge.h"

// Static member to store instance pointer for ISR
static RainGaugeSensor* rainGaugeInstance = nullptr;

void IRAM_ATTR RainGaugeSensor::isrHandler() {
    if (rainGaugeInstance == nullptr) return;

    // Use FreeRTOS ticks, NOT millis() inside the ISR
    uint32_t currentTick = xTaskGetTickCountFromISR();
    
    if ((currentTick - rainGaugeInstance->lastTipTick) > pdMS_TO_TICKS(100)) {
        rainGaugeInstance->isrTipCount++;
        rainGaugeInstance->lastTipTick = currentTick;
    }
}

RainGaugeSensor::RainGaugeSensor(uint8_t sensorPin)
    : pin(sensorPin), isInitialized(false), tipVolume(0.2794),
      isrTipCount(0), lastTipTick(0) {
    lastReading = {0, 0, 0, 0, true};
    rainGaugeInstance = this;
}

bool RainGaugeSensor::initialize() {
    // Set pin with internal pullup
    pinMode(pin, INPUT);
    
    // CRITICAL: Wait for the pin voltage to stabilize before attaching the interrupt
    delay(50);
    
    // Reset counters right before attaching
    isrTipCount = 0;
    lastTipTick = xTaskGetTickCount();
    
    // Attach interrupt on falling edge
    attachInterrupt(digitalPinToInterrupt(pin), isrHandler, FALLING);
    
    isInitialized = true;
    lastReading.isValid = true;
    lastReading.timestamp = millis();
    
    if (DEBUG_ENABLED) Serial.println("[RainGauge] Initialized successfully");
    return true;
}

bool RainGaugeSensor::readSensor() {
    if (!isInitialized) return false;
    
    // Atomically read the volatile 32-bit counter (No spinlock needed)
    uint32_t currentTips = isrTipCount;
    
    // Perform float math OUTSIDE the ISR
    lastReading.tipCount = currentTips;
    lastReading.totalRainfall = (float)currentTips * tipVolume;
    
    // Convert FreeRTOS ticks back to roughly millis for the timestamp
    lastReading.lastTipTime = lastTipTick * portTICK_PERIOD_MS;
    lastReading.timestamp = millis();
    lastReading.isValid = true;
    
    return true;
}

void RainGaugeSensor::enable() {
    isInitialized = true;
}

void RainGaugeSensor::disable() {
    isInitialized = false;
}

bool RainGaugeSensor::isEnabled() const {
    return isInitialized;
}

RainGaugeData RainGaugeSensor::getLastReading() const {
    return lastReading;
}

void RainGaugeSensor::setTipVolume(float volume) {
    tipVolume = volume;
    lastReading.totalRainfall = (float)isrTipCount * tipVolume;
}

float RainGaugeSensor::getTipVolume() const {
    return tipVolume;
}

void RainGaugeSensor::resetTipCount() {
    isrTipCount = 0;
    lastReading.tipCount = 0;
    lastReading.totalRainfall = 0.0f;
    lastReading.timestamp = millis();
}

bool RainGaugeSensor::isValid() const {
    return lastReading.isValid;
}