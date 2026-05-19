#ifndef BMP180_H
#define BMP180_H

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "SensorData.h"
#include "config.h"

class BMP180Sensor {
private:
    Adafruit_BMP085* bmp;
    BMP180Data lastReading;
    bool isInitialized;
    float calibrationTempOffset;
    float calibrationPressureOffset;

public:
    BMP180Sensor();
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    BMP180Data getLastReading() const;
    
    // Calibration
    void setTemperatureOffset(float offset);
    void setPressureOffset(float offset);
    float getTemperatureOffset() const;
    float getPressureOffset() const;
    
    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
    
    // Destructor
    ~BMP180Sensor();
};

#endif // BMP180_H