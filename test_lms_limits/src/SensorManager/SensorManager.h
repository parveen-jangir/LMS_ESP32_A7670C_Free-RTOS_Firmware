#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Wire.h>

#include "config.h"
#include "SensorData.h"
#include "BMP180/BMP180.h"
#include "BH1750/BH1750.h"
#include "MPU6050/MPU6050.h"
#include "DHT22/DHT22.h"
#include "SoilMoisture/SoilMoisture.h"
#include "RainGauge/RainGauge.h"
#include <ArduinoJson.h>
#include "DataLogger/DataLogger.h"

class SensorManager {
private:
    // Sensor instances
    BMP180Sensor* bmp180;
    BH1750Sensor* bh1750;
    MPU6050Sensor* mpu6050;
    DHT22Sensor* dht22;
    SoilMoistureSensor* soilMoisture;
    RainGaugeSensor* rainGauge;
    DataLogger &dataLogger;
    
    // FreeRTOS
    SemaphoreHandle_t dataMutex;
    
    // Data storage
    AllSensorReadings currentReadings;
    SensorCalibration calibration;
    
    // Configuration
    uint32_t readIntervalMs;
    bool taskRunning;
    
    // Static task wrapper
    static void sensorTaskWrapper(void* parameter);
    void sensorTask();
    
    // Sensor enable/disable state
    bool bmp180Enabled, bh1750Enabled, mpu6050Enabled;
    bool dht22Enabled, soilMoistureEnabled, rainGaugeEnabled;

public:

    TaskHandle_t sensorTaskHandle;
    SensorManager(DataLogger &dataLogger);
    ~SensorManager();
    
    // Initialization
    bool initialize();
    
    // Sensor enable/disable
    void sensorState(const char* sensorName, bool state);
    // void disableSensor(const char* sensorName);
    bool isSensorEnabled(const char* sensorName) const;
    
    // Read all sensors
    bool readAllSensors();
    AllSensorReadings getAllReadings();
    bool getAllReadingsJson(JsonDocument &doc);
    
    // Calibration management
    void setCalibration(const SensorCalibration& cal);
    SensorCalibration getCalibration() const;
    
    void setBMP180CalibrationOffset(float tempOffset, float pressureOffset);
    void setBH1750CalibrationOffset(float offset);
    void setMPU6050CalibrationOffset(float accelX, float accelY, float accelZ,
                                      float gyroX, float gyroY, float gyroZ,
                                      float tempOffset);
    void setDHT22CalibrationOffset(float tempOffset, float humidityOffset);
    void setSoilMoistureCalibrationOffset(float dryValue, float wetValue);
    void setRainGaugeTipVolume(float volume);
    String generateApiUrl(const String &tripletId);
    
    // Task control
    bool startReadingTask();
    bool stopReadingTask();
    void setReadInterval(uint32_t intervalMs);
    uint32_t getReadInterval() const;
    
    // Status
    void printSensorStatus();
    void printLastReadings();
    void confSensorWithDefaults();
    void resetMotionCount();
    void resetRainCount();
};

#endif // SENSOR_MANAGER_H