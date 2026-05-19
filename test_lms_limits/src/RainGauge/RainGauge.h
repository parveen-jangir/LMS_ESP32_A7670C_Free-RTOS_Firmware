#ifndef RAIN_GAUGE_H
#define RAIN_GAUGE_H

#include <Arduino.h>
#include "SensorData.h"
#include "config.h"

class RainGaugeSensor {
private:
    uint8_t pin;
    uint8_t interruptPin;
    RainGaugeData lastReading;
    bool isInitialized;
    float tipVolume;  // mm per tip
    unsigned long lastTipDebounceTime;
    
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
    
    // Read sensor data (updates with interrupt)
    bool readSensor();
    RainGaugeData getLastReading() const;
    
    // Calibration
    void setTipVolume(float volume);  // mm per tip
    float getTipVolume() const;
    
    // Reset counter
    void resetTipCount();
    
    // Status
    bool isValid() const;
};

#endif // RAIN_GAUGE_H