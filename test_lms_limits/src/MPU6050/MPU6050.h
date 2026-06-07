#ifndef MPU6050_H
#define MPU6050_H

#include <Wire.h>
#include "SensorData.h"
#include "config.h"

class MPU6050Sensor {
private:
    TwoWire* wire;
    uint8_t address;
    MPU6050Data lastReading;
    bool isInitialized;
    
    // Calibration offsets
    float accelOffsetX, accelOffsetY, accelOffsetZ;
    float gyroOffsetX, gyroOffsetY, gyroOffsetZ;
    float tempOffset;
    
    // Sensitivity scales
    const float ACCEL_SCALE = 9.81f / 16384.0f;  // For ±2g range
    const float GYRO_SCALE = 1.0f / 131.0f;      // For ±250°/s range
    const float TEMP_SCALE = 1.0f / 340.0f;      // Celsius per LSB
    const int16_t TEMP_OFFSET_VAL = -13200;      // 0°C register value offset

public:
    MPU6050Sensor(TwoWire* wireInterface = &Wire);
    
    // Initialize sensor
    bool initialize();
    
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Read sensor data
    bool readSensor();
    MPU6050Data getLastReading() const;
    
    // Calibration
    void setAccelOffset(float x, float y, float z);
    void setGyroOffset(float x, float y, float z);
    void setTempOffset(float offset);
    
    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
};

#endif // MPU6050_H