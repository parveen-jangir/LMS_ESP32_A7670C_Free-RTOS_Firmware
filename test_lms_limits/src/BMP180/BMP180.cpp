#include "BMP180.h"

BMP180Sensor::BMP180Sensor()
    : isInitialized(false), calibrationTempOffset(0.0), 
      calibrationPressureOffset(0.0) {
    
    // Create Adafruit BMP085 instance
    bmp = new Adafruit_BMP085();
    lastReading = {0.0F, 0.0F, false, 0};
}

BMP180Sensor::~BMP180Sensor() {
    if (bmp) {
        delete bmp;
        bmp = nullptr;
    }
}

bool BMP180Sensor::initialize() {
    if (!bmp) return false;
    
    // Initialize I2C if not already done
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(100);
    
    // Initialize BMP085/180 sensor
    if (!bmp->begin()) {
        if (DEBUG_ENABLED) Serial.println(F("[BMP180] Sensor initialization failed!"));
        return false;
    }
    
    isInitialized = true;
    lastReading.isValid = true;
    
    if (DEBUG_ENABLED) Serial.println(F("[BMP180] Initialized successfully"));
    return true;
}

bool BMP180Sensor::readSensor() {
    if (!isInitialized || !bmp) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    
    try {
        // Read temperature
        float temperature = bmp->readTemperature();
        
        // Read pressure in Pascals and convert to hPa
        int32_t pressurePa = bmp->readPressure();
        float pressure = pressurePa / 100.0f;
        
        // Check for valid readings (BMP180 returns 0 on error in some cases)
        if (temperature == 0 && pressure == 0) {
            lastReading.errorCount++;
            lastReading.isValid = false;
            if (DEBUG_ENABLED) Serial.println(F("[BMP180] Invalid sensor readings"));
            return false;
        }
        
        // Apply calibration offsets
        lastReading.temperature = temperature + calibrationTempOffset;
        lastReading.pressure = pressure + calibrationPressureOffset;
        
        lastReading.isValid = true;
        lastReading.errorCount = 0;
        
        return true;
    }
    catch (...) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        if (DEBUG_ENABLED) Serial.println(F("[BMP180] Exception during sensor read"));
        return false;
    }
}

void BMP180Sensor::enable() {
    isInitialized = true;
}

void BMP180Sensor::disable() {
    isInitialized = false;
}

bool BMP180Sensor::isEnabled() const {
    return isInitialized;
}

BMP180Data BMP180Sensor::getLastReading() const {
    return lastReading;
}

void BMP180Sensor::setTemperatureOffset(float offset) {
    calibrationTempOffset = offset;
}

void BMP180Sensor::setPressureOffset(float offset) {
    calibrationPressureOffset = offset;
}

float BMP180Sensor::getTemperatureOffset() const {
    return calibrationTempOffset;
}

float BMP180Sensor::getPressureOffset() const {
    return calibrationPressureOffset;
}

bool BMP180Sensor::isValid() const {
    return lastReading.isValid;
}

uint8_t BMP180Sensor::getErrorCount() const {
    return lastReading.errorCount;
}