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

    // Calibration offsets (applied in software, post-scaling)
    float accelOffsetX, accelOffsetY, accelOffsetZ;
    float gyroOffsetX, gyroOffsetY, gyroOffsetZ;
    float tempOffset;

    // Raw hardware offset registers (Jeff Rowberg style, written to sensor)
    int16_t accelOffsetRawX, accelOffsetRawY, accelOffsetRawZ;
    int16_t gyroOffsetRawX, gyroOffsetRawY, gyroOffsetRawZ;

    int interruptPin;

    // Wake/motion interrupt counter (ISR-updated, must be volatile)
    static volatile uint32_t MotionCount;
    bool MotionDetected;

    // Sensitivity scales
    const float ACCEL_SCALE = 9.81f / 16384.0f;  // For ±2g range
    const float GYRO_SCALE = 1.0f / 131.0f;      // For ±250°/s range
    const float TEMP_SCALE = 1.0f / 340.0f;      // Celsius per LSB
    const float TEMP_OFFSET_VAL = 36.53;         // 0°C register value offset
    static portMUX_TYPE motionMux;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool writeRegister16(uint8_t reg, int16_t value);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length);
    bool readRegister16(uint8_t reg, int16_t* value);

    // Read raw 6-axis data (no offsets/scale applied) - used during calibration
    bool readRawMotion(int16_t* ax, int16_t* ay, int16_t* az,
                        int16_t* gx, int16_t* gy, int16_t* gz);

    // Set hardware offset registers (Jeff Rowberg style)
    bool setAccelOffsetRaw(int16_t x, int16_t y, int16_t z);
    bool setGyroOffsetRaw(int16_t x, int16_t y, int16_t z);

public:
    MPU6050Sensor(TwoWire* wireInterface = &Wire);

    // Initialize sensor. interruptPin: GPIO connected to MPU INT (set < 0 to skip ISR attach)
    bool initialize(int interruptPin = MPU_INTERRUPT_PIN);

    // Self-test: returns true if all axes pass factory self-test
    bool selfTest();

    // Configure Digital Low Pass Filter (DLPF). 44Hz -> DLPF_CFG = 3
    bool setDLPF(uint8_t dlpfMode);

    // Configure motion-detection interrupt on INT pin
    bool enableMotionInterrupt(uint8_t threshold, uint8_t duration);

    // Jeff Rowberg style auto-calibration: iteratively finds hardware offsets
    // so accel reads ~0,0,1g and gyro reads ~0,0,0 at rest.
    // Sensor must be still and level during this call (blocking, ~few seconds).
    bool calibrate(uint8_t loopCount = 6, uint16_t samplesPerLoop = 1000);

    // Call this from main loop / task context (NOT from ISR) when INT fires
    static void IRAM_ATTR onMotionISR();
    static uint32_t getMotionCount();
    static void resetMotionCount();
    
    // Sensor state control
    bool clearInterrupt();
    void enable();
    void disable();
    bool isEnabled() const;

    // Read sensor data
    bool readSensor();
    bool isMotionDetected();
    MPU6050Data getLastReading() const;

    // Software calibration offsets (applied post-scale, on top of hardware offsets)
    void setAccelOffset(float x, float y, float z);
    void setGyroOffset(float x, float y, float z);
    void setTempOffset(float offset);

    // Status
    bool isValid() const;
    uint8_t getErrorCount() const;
};
#endif // MPU6050_H