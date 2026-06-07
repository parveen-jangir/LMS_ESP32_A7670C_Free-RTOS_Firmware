#ifndef BH1750_H
#define BH1750_H

#include <Wire.h>
#include "SensorData.h"
#include "config.h"

class BH1750Sensor {
private:
    TwoWire* wire;
    uint8_t address;
    BH1750Data lastReading;
    bool isInitialized;
    float calibrationOffset;

public:
    BH1750Sensor(TwoWire* wireInterface = &Wire);
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    BH1750Data getLastReading() const;
    
    // Calibration
    void setOffset(float offset);
    float getOffset() const;
    
    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
};

#endif // BH1750_H