#ifndef DHT22_H
#define DHT22_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "SensorData.h"
#include "config.h"

class DHT22Sensor {
private:
    DHT_Unified* dht;
    uint8_t pin;
    DHT22Data lastReading;
    bool isInitialized;
    float calibrationTempOffset;
    float calibrationHumidityOffset;
    unsigned long lastReadTime;
    
public:
    DHT22Sensor(uint8_t sensorPin = DHT22_PIN);
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    DHT22Data getLastReading() const;
    
    // Calibration
    void setTemperatureOffset(float offset);
    void setHumidityOffset(float offset);
    float getTemperatureOffset() const;
    float getHumidityOffset() const;
    
    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
    
    // Destructor
    ~DHT22Sensor();
};

#endif // DHT22_H