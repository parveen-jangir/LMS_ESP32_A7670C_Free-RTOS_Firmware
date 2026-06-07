#include "RainGauge.h"

// Static member to store instance pointer for ISR
static RainGaugeSensor* rainGaugeInstance = nullptr;

void IRAM_ATTR RainGaugeSensor::isrHandler() {
    if (rainGaugeInstance) {
        unsigned long now = millis();
        // Debounce
        if ((now - rainGaugeInstance->lastTipDebounceTime) > RAIN_GAUGE_DEBOUNCE_MS) {
            rainGaugeInstance->lastReading.tipCount++;
            rainGaugeInstance->lastReading.totalRainfall = 
                rainGaugeInstance->lastReading.tipCount * rainGaugeInstance->tipVolume;
            rainGaugeInstance->lastTipDebounceTime = now;
            rainGaugeInstance->lastReading.lastTipTime = now;
        }
    }
}

RainGaugeSensor::RainGaugeSensor(uint8_t sensorPin)
    : pin(sensorPin), isInitialized(false), tipVolume(0.2794),
      lastTipDebounceTime(0) {
    lastReading = {0, 0, 0, 0, true};
    rainGaugeInstance = this;
}

bool RainGaugeSensor::initialize() {
    pinMode(pin, INPUT_PULLUP);
    
    // Attach interrupt on falling edge (tipping bucket)
    attachInterrupt(digitalPinToInterrupt(pin), isrHandler, FALLING);
    
    isInitialized = true;
    lastReading.isValid = true;
    lastReading.timestamp = millis();
    
    if (DEBUG_ENABLED) Serial.println("[RainGauge] Initialized successfully");
    return true;
}

bool RainGaugeSensor::readSensor() {
    if (!isInitialized) return false;
    
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
    lastReading.totalRainfall = lastReading.tipCount * tipVolume;
}

float RainGaugeSensor::getTipVolume() const {
    return tipVolume;
}

void RainGaugeSensor::resetTipCount() {
    lastReading.tipCount = 0;
    lastReading.totalRainfall = 0;
    lastReading.timestamp = millis();
}

bool RainGaugeSensor::isValid() const {
    return lastReading.isValid;
}