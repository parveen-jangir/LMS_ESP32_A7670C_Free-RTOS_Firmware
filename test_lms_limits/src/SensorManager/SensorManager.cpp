#include "SensorManager.h"

SensorManager::SensorManager()
    : sensorTaskHandle(nullptr), dataMutex(nullptr),
      readIntervalMs(SENSOR_READ_INTERVAL_MS), taskRunning(false),
      bmp180Enabled(true), bh1750Enabled(true), mpu6050Enabled(true),
      dht22Enabled(true), soilMoistureEnabled(true), rainGaugeEnabled(true) {
    
    // Create sensor instances
    bmp180 = new BMP180Sensor();
    bh1750 = new BH1750Sensor(&Wire);
    mpu6050 = new MPU6050Sensor(&Wire);
    dht22 = new DHT22Sensor(DHT22_PIN);
    soilMoisture = new SoilMoistureSensor(SOIL_MOISTURE_PIN);
    rainGauge = new RainGaugeSensor(RAIN_GAUGE_PIN);
    
    // Create mutex
    dataMutex = xSemaphoreCreateMutex();
    
    // Initialize calibration with default values
    calibration.bmp180TempOffset = 0.0;
    calibration.bmp180PressureOffset = 0.0;
    calibration.bh1750Offset = 0.0;
    calibration.mpu6050AccelOffsetX = 0.0;
    calibration.mpu6050AccelOffsetY = 0.0;
    calibration.mpu6050AccelOffsetZ = 0.0;
    calibration.mpu6050GyroOffsetX = 0.0;
    calibration.mpu6050GyroOffsetY = 0.0;
    calibration.mpu6050GyroOffsetZ = 0.0;
    calibration.mpu6050TempOffset = 0.0;
    calibration.dht22TempOffset = 0.0;
    calibration.dht22HumidityOffset = 0.0;
    calibration.soilMoistureOffsetMin = 0.0;
    calibration.soilMoistureOffsetMax = 4095.0;
    calibration.rainGaugeTipVolume = 0.2794;
}

SensorManager::~SensorManager() {
    stopReadingTask();
    
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (bmp180) delete bmp180;
    if (bh1750) delete bh1750;
    if (mpu6050) delete mpu6050;
    if (dht22) delete dht22;
    if (soilMoisture) delete soilMoisture;
    if (rainGauge) delete rainGauge;
}

bool SensorManager::initialize() {
    if (DEBUG_ENABLED) Serial.println("\n[SensorManager] Initializing sensors...");
    
    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(100);
    
    // Initialize all sensors
    bool bmp180Init = bmp180->initialize();
    bool bh1750Init = bh1750->initialize();
    bool mpu6050Init = mpu6050->initialize();
    bool dht22Init = dht22->initialize();
    bool soilMoistureInit = soilMoisture->initialize();
    bool rainGaugeInit = rainGauge->initialize();
    
    if (DEBUG_ENABLED) {
        Serial.printf("[SensorManager] BMP180: %s\n", bmp180Init ? "OK" : "FAILED");
        Serial.printf("[SensorManager] BH1750: %s\n", bh1750Init ? "OK" : "FAILED");
        Serial.printf("[SensorManager] MPU6050: %s\n", mpu6050Init ? "OK" : "FAILED");
        Serial.printf("[SensorManager] DHT22: %s\n", dht22Init ? "OK" : "FAILED");
        Serial.printf("[SensorManager] SoilMoisture: %s\n", soilMoistureInit ? "OK" : "FAILED");
        Serial.printf("[SensorManager] RainGauge: %s\n", rainGaugeInit ? "OK" : "FAILED");
    }
    
    return (bmp180Init && bh1750Init && mpu6050Init && dht22Init && 
            soilMoistureInit && rainGaugeInit);
}

void SensorManager::sensorTaskWrapper(void* parameter) {
    SensorManager* manager = (SensorManager*)parameter;
    manager->sensorTask();
}

void SensorManager::sensorTask() {
    if (DEBUG_ENABLED) Serial.println("[SensorManager] Sensor task started");
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (taskRunning) {
        // Read all enabled sensors
        if (bmp180Enabled && bmp180) bmp180->readSensor();
        if (bh1750Enabled && bh1750) bh1750->readSensor();
        if (mpu6050Enabled && mpu6050) mpu6050->readSensor();
        if (dht22Enabled && dht22) dht22->readSensor();
        if (soilMoistureEnabled && soilMoisture) soilMoisture->readSensor();
        if (rainGaugeEnabled && rainGauge) rainGauge->readSensor();
        
        // Update current readings (thread-safe)
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            if (bmp180) currentReadings.bmp180 = bmp180->getLastReading();
            if (bh1750) currentReadings.bh1750 = bh1750->getLastReading();
            if (mpu6050) currentReadings.mpu6050 = mpu6050->getLastReading();
            if (dht22) currentReadings.dht22 = dht22->getLastReading();
            if (soilMoisture) currentReadings.soilMoisture = soilMoisture->getLastReading();
            if (rainGauge) currentReadings.rainGauge = rainGauge->getLastReading();
            currentReadings.readTimestamp = millis();
            
            xSemaphoreGive(dataMutex);
        }
        
        // Delay for the specified interval
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(readIntervalMs));
    }
    
    if (DEBUG_ENABLED) Serial.println("[SensorManager] Sensor task stopped");
    vTaskDelete(nullptr);
}

bool SensorManager::startReadingTask() {
    if (taskRunning || sensorTaskHandle != nullptr) {
        if (DEBUG_ENABLED) Serial.println("[SensorManager] Task already running");
        return false;
    }
    
    taskRunning = true;
    
    BaseType_t result = xTaskCreatePinnedToCore(
        sensorTaskWrapper,
        "SensorTask",
        SENSOR_TASK_STACK_SIZE,
        this,
        SENSOR_TASK_PRIORITY,
        &sensorTaskHandle,
        SENSOR_TASK_CORE
    );
    
    if (result != pdPASS) {
        taskRunning = false;
        if (DEBUG_ENABLED) Serial.println("[SensorManager] Failed to create sensor task");
        return false;
    }
    
    if (DEBUG_ENABLED) Serial.println("[SensorManager] Sensor task started");
    return true;
}

bool SensorManager::stopReadingTask() {
    if (!taskRunning || sensorTaskHandle == nullptr) {
        return false;
    }
    
    taskRunning = false;
    
    // Wait for task to delete itself
    vTaskDelay(pdMS_TO_TICKS(readIntervalMs + 100));
    sensorTaskHandle = nullptr;
    
    if (DEBUG_ENABLED) Serial.println("[SensorManager] Sensor task stopped");
    return true;
}

bool SensorManager::readAllSensors() {
    bool success = true;
    
    if (bmp180Enabled && bmp180) success &= bmp180->readSensor();
    if (bh1750Enabled && bh1750) success &= bh1750->readSensor();
    if (mpu6050Enabled && mpu6050) success &= mpu6050->readSensor();
    if (dht22Enabled && dht22) success &= dht22->readSensor();
    if (soilMoistureEnabled && soilMoisture) success &= soilMoisture->readSensor();
    if (rainGaugeEnabled && rainGauge) success &= rainGauge->readSensor();
    
    // Update current readings
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        if (bmp180) currentReadings.bmp180 = bmp180->getLastReading();
        if (bh1750) currentReadings.bh1750 = bh1750->getLastReading();
        if (mpu6050) currentReadings.mpu6050 = mpu6050->getLastReading();
        if (dht22) currentReadings.dht22 = dht22->getLastReading();
        if (soilMoisture) currentReadings.soilMoisture = soilMoisture->getLastReading();
        if (rainGauge) currentReadings.rainGauge = rainGauge->getLastReading();
        currentReadings.readTimestamp = millis();
        
        xSemaphoreGive(dataMutex);
    }
    
    return success;
}

AllSensorReadings SensorManager::getAllReadings() {
    AllSensorReadings readings;
    
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        readings = currentReadings;
        xSemaphoreGive(dataMutex);
    }
    
    return readings;
}

bool SensorManager::getAllReadingsJson(JsonDocument &doc)
{
    AllSensorReadings readings = getAllReadings();

    doc["type"] = "sensor_broadcast";
    doc["timestamp"] = readings.readTimestamp;
    doc["status"] = "ok";
    doc["msg"] = "Sensor readings broadcast";

    // BMP180
    JsonObject bmp180 = doc["bmp180"].to<JsonObject>();
    bmp180["temperature"] = readings.bmp180.temperature;
    bmp180["pressure"]    = readings.bmp180.pressure;
    bmp180["valid"]       = readings.bmp180.isValid;
    bmp180["timestamp"]   = readings.bmp180.timestamp;
    bmp180["state"]       = bool(bmp180Enabled); // true if valid, false if error

    // BH1750
    JsonObject bh1750 = doc["bh1750"].to<JsonObject>();
    bh1750["illuminance"]       = readings.bh1750.illuminance;
    bh1750["valid"]     = readings.bh1750.isValid;
    bh1750["timestamp"] = readings.bh1750.timestamp;
    bh1750["state"]     = bool(bh1750Enabled); // true if valid, false if error

    // MPU6050
    JsonObject mpu = doc["mpu6050"].to<JsonObject>();

    JsonObject accel = mpu["acceleration"].to<JsonObject>();
    accel["x"] = readings.mpu6050.accelX;
    accel["y"] = readings.mpu6050.accelY;
    accel["z"] = readings.mpu6050.accelZ;

    JsonObject gyro = mpu["gyroscope"].to<JsonObject>();
    gyro["x"] = readings.mpu6050.gyroX;
    gyro["y"] = readings.mpu6050.gyroY;
    gyro["z"] = readings.mpu6050.gyroZ;

    mpu["temperature"] = readings.mpu6050.temperature;
    mpu["valid"]       = readings.mpu6050.isValid;
    mpu["timestamp"]   = readings.mpu6050.timestamp;
    mpu["state"]       = bool(mpu6050Enabled); // true if valid, false if error
    mpu["movement_count"] = readings.mpu6050.movementCount;
    // DHT22
    JsonObject dht22 = doc["dht22"].to<JsonObject>();
    dht22["temperature"] = readings.dht22.temperature;
    dht22["humidity"]    = readings.dht22.humidity;
    dht22["valid"]       = readings.dht22.isValid;
    dht22["timestamp"]   = readings.dht22.timestamp;
    dht22["state"]       = bool(dht22Enabled); // true if valid, false if error

    // Soil Moisture
    JsonObject soil = doc["soil_moisture"].to<JsonObject>();
    soil["raw"]        = readings.soilMoisture.rawValue;
    soil["percentage"] = readings.soilMoisture.percentage;
    soil["valid"]      = readings.soilMoisture.isValid;
    soil["timestamp"]  = readings.soilMoisture.timestamp;
    soil["state"]      = bool(soilMoistureEnabled); // true if valid, false if error

    // Rain Gauge
    JsonObject rain = doc["rain_gauge"].to<JsonObject>();
    rain["tip_count"]      = readings.rainGauge.tipCount;
    rain["rainfall_mm"]    = readings.rainGauge.totalRainfall;
    rain["last_tip_time"]  = readings.rainGauge.lastTipTime;
    rain["valid"]          = readings.rainGauge.isValid;
    rain["timestamp"]      = readings.rainGauge.timestamp;
    rain["state"]          = bool(rainGaugeEnabled); // true if valid, false if error

    return true;
}

void SensorManager::sensorState(const char* sensorName, bool state) {
    if (strcmp(sensorName, "BMP180") == 0) {
        bmp180Enabled = state;
        if (bmp180)
        {
            state ? bmp180->enable() : bmp180->disable();
        }
    }
    else if (strcmp(sensorName, "BH1750") == 0) {
        bh1750Enabled = state;
        if (bh1750) {
            state ? bh1750->enable() : bh1750->disable();
        }
    }
    else if (strcmp(sensorName, "MPU6050") == 0) {
        mpu6050Enabled = state;
        if (mpu6050) {
            state ? mpu6050->enable() : mpu6050->disable();
        }
    }
    else if (strcmp(sensorName, "DHT22") == 0) {
        dht22Enabled = state;
        if (dht22) {
            state ? dht22->enable() : dht22->disable();
        }
    }
    else if (strcmp(sensorName, "SoilMoisture") == 0) {
        soilMoistureEnabled = state;
        if (soilMoisture) {
            state ? soilMoisture->enable() : soilMoisture->disable();
        }
    }
    else if (strcmp(sensorName, "RainGauge") == 0) {
        rainGaugeEnabled = state;
        if (rainGauge) {
            state ? rainGauge->enable() : rainGauge->disable();
        }
    }
    
    if (DEBUG_ENABLED) Serial.printf("[SensorManager] Enabled: %s\n", sensorName);
}

bool SensorManager::isSensorEnabled(const char* sensorName) const {
    if (strcmp(sensorName, "BMP180") == 0) return bmp180Enabled;
    if (strcmp(sensorName, "BH1750") == 0) return bh1750Enabled;
    if (strcmp(sensorName, "MPU6050") == 0) return mpu6050Enabled;
    if (strcmp(sensorName, "DHT22") == 0) return dht22Enabled;
    if (strcmp(sensorName, "SoilMoisture") == 0) return soilMoistureEnabled;
    if (strcmp(sensorName, "RainGauge") == 0) return rainGaugeEnabled;
    
    return false;
}

void SensorManager::setCalibration(const SensorCalibration& cal) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        calibration = cal;
        
        // Apply calibration to sensors
        if (bmp180) {
            bmp180->setTemperatureOffset(cal.bmp180TempOffset);
            bmp180->setPressureOffset(cal.bmp180PressureOffset);
        }
        if (bh1750) {
            bh1750->setOffset(cal.bh1750Offset);
        }
        if (mpu6050) {
            mpu6050->setAccelOffset(cal.mpu6050AccelOffsetX, cal.mpu6050AccelOffsetY, cal.mpu6050AccelOffsetZ);
            mpu6050->setGyroOffset(cal.mpu6050GyroOffsetX, cal.mpu6050GyroOffsetY, cal.mpu6050GyroOffsetZ);
            mpu6050->setTempOffset(cal.mpu6050TempOffset);
        }
        if (dht22) {
            dht22->setTemperatureOffset(cal.dht22TempOffset);
            dht22->setHumidityOffset(cal.dht22HumidityOffset);
        }
        if (soilMoisture) {
            soilMoisture->setCalibrationOffsets(cal.soilMoistureOffsetMin, cal.soilMoistureOffsetMax);
        }
        if (rainGauge) {
            rainGauge->setTipVolume(cal.rainGaugeTipVolume);
        }
        
        xSemaphoreGive(dataMutex);
    }
}

SensorCalibration SensorManager::getCalibration() const {
    SensorCalibration cal;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) {
        cal = calibration;
        xSemaphoreGive(dataMutex);
    }
    
    return cal;
}

void SensorManager::setBMP180CalibrationOffset(float tempOffset, float pressureOffset) {
    calibration.bmp180TempOffset = tempOffset;
    calibration.bmp180PressureOffset = pressureOffset;
    if (bmp180) {
        bmp180->setTemperatureOffset(tempOffset);
        bmp180->setPressureOffset(pressureOffset);
    }
}

void SensorManager::setBH1750CalibrationOffset(float offset) {
    calibration.bh1750Offset = offset;
    if (bh1750) bh1750->setOffset(offset);
}

void SensorManager::setMPU6050CalibrationOffset(float accelX, float accelY, float accelZ,
                                                 float gyroX, float gyroY, float gyroZ,
                                                 float tempOffset) {
    calibration.mpu6050AccelOffsetX = accelX;
    calibration.mpu6050AccelOffsetY = accelY;
    calibration.mpu6050AccelOffsetZ = accelZ;
    calibration.mpu6050GyroOffsetX = gyroX;
    calibration.mpu6050GyroOffsetY = gyroY;
    calibration.mpu6050GyroOffsetZ = gyroZ;
    calibration.mpu6050TempOffset = tempOffset;
    
    if (mpu6050) {
        mpu6050->setAccelOffset(accelX, accelY, accelZ);
        mpu6050->setGyroOffset(gyroX, gyroY, gyroZ);
        mpu6050->setTempOffset(tempOffset);
    }
}

void SensorManager::setDHT22CalibrationOffset(float tempOffset, float humidityOffset) {
    calibration.dht22TempOffset = tempOffset;
    calibration.dht22HumidityOffset = humidityOffset;
    if (dht22) {
        dht22->setTemperatureOffset(tempOffset);
        dht22->setHumidityOffset(humidityOffset);
    }
}

void SensorManager::setSoilMoistureCalibrationOffset(float dryValue, float wetValue) {
    calibration.soilMoistureOffsetMin = dryValue;
    calibration.soilMoistureOffsetMax = wetValue;
    if (soilMoisture) {
        soilMoisture->setCalibrationOffsets(dryValue, wetValue);
    }
}

void SensorManager::setRainGaugeTipVolume(float volume) {
    calibration.rainGaugeTipVolume = volume;
    if (rainGauge) rainGauge->setTipVolume(volume);
}

void SensorManager::setReadInterval(uint32_t intervalMs) {
    readIntervalMs = intervalMs;
}

uint32_t SensorManager::getReadInterval() const {
    return readIntervalMs;
}

void SensorManager::printSensorStatus() {
    Serial.println("\n========== SENSOR STATUS ==========");
    Serial.printf("BMP180:       %s (Errors: %d)\n", 
                  bmp180Enabled ? "ENABLED" : "DISABLED", 
                  bmp180 ? bmp180->getErrorCount() : -1);
    Serial.printf("BH1750:       %s (Errors: %d)\n", 
                  bh1750Enabled ? "ENABLED" : "DISABLED", 
                  bh1750 ? bh1750->getErrorCount() : -1);
    Serial.printf("MPU6050:      %s (Errors: %d)\n", 
                  mpu6050Enabled ? "ENABLED" : "DISABLED", 
                  mpu6050 ? mpu6050->getErrorCount() : -1);
    Serial.printf("DHT22:        %s (Errors: %d)\n", 
                  dht22Enabled ? "ENABLED" : "DISABLED", 
                  dht22 ? dht22->getErrorCount() : -1);
    Serial.printf("SoilMoisture: %s (Errors: %d)\n", 
                  soilMoistureEnabled ? "ENABLED" : "DISABLED", 
                  soilMoisture ? soilMoisture->getErrorCount() : -1);
    Serial.printf("RainGauge:    %s\n", 
                  rainGaugeEnabled ? "ENABLED" : "DISABLED");
    Serial.println("====================================\n");
}

void SensorManager::printLastReadings() {
    AllSensorReadings readings = getAllReadings();
    
    Serial.println("\n========== SENSOR READINGS ==========");
    Serial.printf("Timestamp: %lu ms\n\n", readings.readTimestamp);
    
    if (readings.bmp180.isValid) {
        Serial.printf("BMP180 - Temp: %.2f°C, Pressure: %.2f hPa\n", 
                      readings.bmp180.temperature, readings.bmp180.pressure);
    }
    
    if (readings.bh1750.isValid) {
        Serial.printf("BH1750 - Illuminance: %.2f Lux\n", readings.bh1750.illuminance);
    }
    
    if (readings.mpu6050.isValid) {
        Serial.printf("MPU6050 - Accel: X:%.2f, Y:%.2f, Z:%.2f m/s²\n",
                      readings.mpu6050.accelX, readings.mpu6050.accelY, readings.mpu6050.accelZ);
        Serial.printf("         - Gyro: X:%.2f, Y:%.2f, Z:%.2f °/s\n",
                      readings.mpu6050.gyroX, readings.mpu6050.gyroY, readings.mpu6050.gyroZ);
        Serial.printf("         - Temp: %.2f°C\n", readings.mpu6050.temperature);
    }
    
    if (readings.dht22.isValid) {
        Serial.printf("DHT22 - Temp: %.2f°C, Humidity: %.2f%%\n", 
                      readings.dht22.temperature, readings.dht22.humidity);
    }
    
    if (readings.soilMoisture.isValid) {
        Serial.printf("SoilMoisture - Raw: %d, Percentage: %.2f%%\n", 
                      readings.soilMoisture.rawValue, readings.soilMoisture.percentage);
    }
    
    if (readings.rainGauge.isValid) {
        Serial.printf("RainGauge - Tips: %lu, Total Rainfall: %.2f mm\n", 
                      readings.rainGauge.tipCount, readings.rainGauge.totalRainfall);
    }
    
    Serial.println("=======================================\n");
}

String SensorManager::generateApiUrl(const String &tripletId)
{
    AllSensorReadings r = getAllReadings();

    String url =
        "https://landslidemonitoring.in/ota.php?"
        "api_key=3WU63XFVOKEC1VBM";

    url += "&triplet=" + tripletId;

    // t1s1 = temp,humidity
    url += "&" + tripletId + "s1=" +
           String(r.dht22.temperature, 2) + "," +
           String(r.dht22.humidity, 2);

    // t1s2 = pressure
    url += "&" + tripletId + "s2=" +
           String(r.bmp180.pressure, 2);

    // t1s3 = rainfall
    url += "&" + tripletId + "s3=" +
           String(r.rainGauge.totalRainfall, 2);

    // t1s4 = light
    url += "&" + tripletId + "s4=" +
           String(r.bh1750.illuminance, 2);

    // MPU6050 data
    float roll  = 0;
    float pitch = 0;
    float yaw   = 0;
    uint32_t motionCount = 0;

    url += "&" + tripletId + "s5=" +
           String(r.mpu6050.accelX, 2) + "," +
           String(r.mpu6050.accelY, 2) + "," +
           String(r.mpu6050.accelZ, 2) + "," +
           String(r.mpu6050.gyroX, 2) + "," +
           String(r.mpu6050.gyroY, 2) + "," +
           String(r.mpu6050.gyroZ, 2) + "," +
           String(roll, 2) + "," +
           String(pitch, 2) + "," +
           String(yaw, 2) + "," +
           String(motionCount) + ",0,0";

    // soil temperature
    url += "&" + tripletId + "s6=" +
           String(r.bmp180.temperature, 2);

    // soil moisture
    url += "&" + tripletId + "s7=" +
           String(r.soilMoisture.rawValue);

    // reserved
    url += "&" + tripletId + "s8=0";

    // GPS
    url += "&" + tripletId + "s9=00.0000|00.0000";

    return url;
}

