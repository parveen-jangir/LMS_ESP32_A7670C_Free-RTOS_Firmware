#ifndef IMU6050_H
#define IMU6050_H

#include <Wire.h>
#include "SensorData.h"
#include "config.h"
#include "I2Cdev.h"
#include "MPU6050.h"

class IMU6050Sensor
{
private:
    TwoWire *wire;
    uint8_t address;
    MPU6050Data lastReading;
    bool isInitialized;

    // Calibration offsets
    float accelOffsetX, accelOffsetY, accelOffsetZ;
    float gyroOffsetX, gyroOffsetY, gyroOffsetZ;
    float tempOffset;
    
    VectorInt16 aa; // [x, y, z]            accel sensor measurements
    VectorInt16 gg;

    static void IRAM_ATTR IMU_isrHandler();

    // Sensitivity scales
    // const float ACCEL_SCALE = 1.0f / 16384.0f; // For ±2g range
    // const float GYRO_SCALE = 1.0f / 131.0f;    // For ±250°/s range
    // const float TEMP_SCALE = 1.0f / 340.0f;    // Celsius per LSB
    // const int16_t TEMP_OFFSET_VAL = -13200;    // 0°C register value offset

public:
    IMU6050Sensor(TwoWire *wireInterface = &Wire);

    // Interrupt Handling

    // Initialize sensor
    bool initialize();
    bool calibrate();
    // Sensor state control
    void enable();
    void disable();
    bool isEnabled() const;
    void changeAccelSensitivity(uint8_t MPU_Accel_Sensitivity);
    void changeGyroSensitivity(uint8_t MPU_Gyro_Sensitivity);
    void changeDLPFcoff(uint8_t MPU_DLPF);

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