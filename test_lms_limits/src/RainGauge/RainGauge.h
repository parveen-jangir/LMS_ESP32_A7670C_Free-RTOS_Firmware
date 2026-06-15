#ifndef RAIN_GAUGE_H
#define RAIN_GAUGE_H

#include <Arduino.h>
#include "SensorData.h"
#include "config.h"

class RainGaugeSensor {
private:
    uint8_t pin;
    RainGaugeData lastReading;
    bool isInitialized;
    float tipVolume;  // mm per tip
    
    // Lock-Free Volatile State 
    volatile uint32_t isrTipCount;
    volatile uint32_t lastTipTick; // Using FreeRTOS ticks for safety
    
    // Static method for ISR
    static void IRAM_ATTR isrHandler();

public:
    RainGaugeSensor(uint8_t sensorPin = RAIN_GAUGE_PIN);
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    RainGaugeData getLastReading() const;
    
    // Calibration
    void setTipVolume(float volume);
    float getTipVolume() const;
    
    // Reset counter
    void resetTipCount();
    
    // Status
    bool isValid() const;
};

#endif // RAIN_GAUGE_H