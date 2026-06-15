#include "SoilMoisture.h"

SoilMoistureSensor::SoilMoistureSensor(uint8_t sensorPin)
    : pin(sensorPin), isInitialized(false),
      calibrationOffsetMin(1050), calibrationOffsetMax(2480) {
    lastReading = {0, 0, 0, false, 0};
}

bool SoilMoistureSensor::initialize() {
    // Configure ADC
    // analogSetAttenuation(ADC_ATTENUATION);
    analogReadResolution(ADC_RESOLUTION);
    
    pinMode(pin, INPUT);
    delay(100);
    
    isInitialized = true;
    lastReading.isValid = true;
    
    if (DEBUG_ENABLED) Serial.println("[SoilMoisture] Initialized successfully");
    return true;
}

bool SoilMoistureSensor::readSensor() {
    if (!isInitialized) return false;
    
    lastReading.timestamp = millis();
    
    // Read ADC value
    lastReading.rawValue = analogReadMilliVolts(pin);
    
    // Convert to percentage (0-100%)
    // Clamp between min and max calibration values
    uint16_t clamped = constrain(lastReading.rawValue, 
                                  0, 
                                  3300);
    

    float temp = (float)calibrationOffsetMax - (float)calibrationOffsetMin;
    
        float numerator = (float)calibrationOffsetMax - (float)clamped;

    lastReading.percentage = (numerator / temp) * 100.0f;
    lastReading.isValid = true;
    lastReading.errorCount = 0;
    
    return true;
}

void SoilMoistureSensor::enable() {
    isInitialized = true;
}

void SoilMoistureSensor::disable() {
    isInitialized = false;
}

bool SoilMoistureSensor::isEnabled() const {
    return isInitialized;
}

SoilMoistureData SoilMoistureSensor::getLastReading() const {
    return lastReading;
}

void SoilMoistureSensor::setCalibrationOffsets(float dryValue, float wetValue) {
    calibrationOffsetMin = dryValue;
    calibrationOffsetMax = wetValue;
}

void SoilMoistureSensor::setCalibrationOffsetMin(float minValue) {
    calibrationOffsetMin = minValue;
}

void SoilMoistureSensor::setCalibrationOffsetMax(float maxValue) {
    calibrationOffsetMax = maxValue;
}

float SoilMoistureSensor::getCalibrationOffsetMin() const {
    return calibrationOffsetMin;
}

float SoilMoistureSensor::getCalibrationOffsetMax() const {
    return calibrationOffsetMax;
}

bool SoilMoistureSensor::isValid() const {
    return lastReading.isValid;
}

uint8_t SoilMoistureSensor::getErrorCount() const {
    return lastReading.errorCount;
}