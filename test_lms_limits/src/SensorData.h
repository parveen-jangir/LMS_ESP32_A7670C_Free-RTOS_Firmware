#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>
#include <time.h>
#include <Arduino.h>

// ==================== Sensor Reading Structures ====================

struct BMP180Data {
    float temperature;      // Celsius
    float pressure;         // hPa
    uint32_t timestamp;
    bool isValid;
    uint8_t errorCount;
};

struct BH1750Data {
    float illuminance;      // Lux
    uint32_t timestamp;
    bool isValid;
    uint8_t errorCount;
};

struct MPU6050Data {
    float accelX, accelY, accelZ;  // m/s²
    float gyroX, gyroY, gyroZ;     // deg/s
    float temperature;              // Celsius
    uint32_t timestamp;
    bool isValid;
    uint8_t errorCount;
};

struct DHT22Data {
    float temperature;      // Celsius
    float humidity;         // %RH
    uint32_t timestamp;
    bool isValid;
    uint8_t errorCount;
};

struct SoilMoistureData {
    uint16_t rawValue;      // 0-4095 (ADC)
    float percentage;       // 0-100%
    uint32_t timestamp;
    bool isValid;
    uint8_t errorCount;
};

struct RainGaugeData {
    uint32_t tipCount;      // Total number of tips
    float totalRainfall;    // mm (0.2794mm per tip, configurable)
    uint32_t lastTipTime;   // Timestamp of last tip
    uint32_t timestamp;
    bool isValid;
};

// ==================== Calibration Structures ====================

struct SensorCalibration {
    float bmp180TempOffset;
    float bmp180PressureOffset;
    
    float bh1750Offset;
    
    float mpu6050AccelOffsetX, mpu6050AccelOffsetY, mpu6050AccelOffsetZ;
    float mpu6050GyroOffsetX, mpu6050GyroOffsetY, mpu6050GyroOffsetZ;
    float mpu6050TempOffset;
    
    float dht22TempOffset;
    float dht22HumidityOffset;
    
    float soilMoistureOffsetMin;  // ADC value at dry
    float soilMoistureOffsetMax;  // ADC value at wet
    
    float rainGaugeTipVolume;     // mm per tip
};

// ==================== Unified Sensor Reading ====================

struct AllSensorReadings {
    BMP180Data bmp180;
    BH1750Data bh1750;
    MPU6050Data mpu6050;
    DHT22Data dht22;
    SoilMoistureData soilMoisture;
    RainGaugeData rainGauge;
    uint32_t readTimestamp;
};

#endif // SENSOR_DATA_H