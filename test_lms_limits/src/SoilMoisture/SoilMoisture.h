#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include <Arduino.h>
#include "SensorData.h"
#include "config.h"

class SoilMoistureSensor {
private:
    uint8_t pin;
    SoilMoistureData lastReading;
    bool isInitialized;
    uint32_t calibrationOffsetMin;  // ADC value at dry (0%)
    uint32_t calibrationOffsetMax;  // ADC value at wet (100%)

public:
    SoilMoistureSensor(uint8_t sensorPin = SOIL_MOISTURE_PIN);
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    SoilMoistureData getLastReading() const;
    
    // Calibration
    void setCalibrationOffsets(float dryValue, float wetValue);
    void setCalibrationOffsetMin(float minValue);
    void setCalibrationOffsetMax(float maxValue);
    float getCalibrationOffsetMin() const;
    float getCalibrationOffsetMax() const;
    
    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
};

#endif // SOIL_MOISTURE_H