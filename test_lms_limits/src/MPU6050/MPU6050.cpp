#include "MPU6050.h"

MPU6050Sensor::MPU6050Sensor(TwoWire* wireInterface)
    : wire(wireInterface), address(MPU6050_ADDRESS), isInitialized(false),
      accelOffsetX(0), accelOffsetY(0), accelOffsetZ(0),
      gyroOffsetX(0), gyroOffsetY(0), gyroOffsetZ(0),
      tempOffset(0) {
    lastReading = {0, 0, 0, 0, 0, 0, 0, 0, false, 0};
}

bool MPU6050Sensor::initialize() {
    if (!wire) return false;
    
    wire->begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(100);
    
    // Check if MPU6050 is present
    wire->beginTransmission(address);
    if (wire->endTransmission() != 0) {
        if (DEBUG_ENABLED) Serial.println("[MPU6050] Sensor not found!");
        return false;
    }
    
    // Wake up from sleep mode
    wire->beginTransmission(address);
    wire->write(0x6B);  // PWR_MGMT_1 register
    wire->write(0x00);  // Clear sleep bit
    if (wire->endTransmission() != 0) return false;
    
    delay(100);
    
    // Set accelerometer range to ±2g
    wire->beginTransmission(address);
    wire->write(0x1C);  // ACCEL_CONFIG register
    wire->write(0x00);  // ±2g
    if (wire->endTransmission() != 0) return false;
    
    // Set gyro range to ±250°/s
    wire->beginTransmission(address);
    wire->write(0x1B);  // GYRO_CONFIG register
    wire->write(0x00);  // ±250°/s
    if (wire->endTransmission() != 0) return false;
    
    isInitialized = true;
    lastReading.isValid = true;
    
    if (DEBUG_ENABLED) Serial.println("[MPU6050] Initialized successfully");
    return true;
}

bool MPU6050Sensor::readSensor() {
    if (!isInitialized) return false;
    
    lastReading.timestamp = millis();
    
    // Read 14 bytes starting from ACCEL_XOUT_H (0x3B)
    wire->beginTransmission(address);
    wire->write(0x3B);
    if (wire->endTransmission() != 0) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    if (wire->requestFrom(address, (uint8_t)14, (uint8_t)1) != 14) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    // Read raw acceleration data
    int16_t accelX_raw = ((int16_t)wire->read() << 8) | wire->read();
    int16_t accelY_raw = ((int16_t)wire->read() << 8) | wire->read();
    int16_t accelZ_raw = ((int16_t)wire->read() << 8) | wire->read();
    
    // Skip temperature (not needed right now)
    int16_t temp_raw = ((int16_t)wire->read() << 8) | wire->read();
    
    // Read raw gyro data
    int16_t gyroX_raw = ((int16_t)wire->read() << 8) | wire->read();
    int16_t gyroY_raw = ((int16_t)wire->read() << 8) | wire->read();
    int16_t gyroZ_raw = ((int16_t)wire->read() << 8) | wire->read();
    
    // Convert to physical units with calibration offset
    lastReading.accelX = (accelX_raw * ACCEL_SCALE) + accelOffsetX;
    lastReading.accelY = (accelY_raw * ACCEL_SCALE) + accelOffsetY;
    lastReading.accelZ = (accelZ_raw * ACCEL_SCALE) + accelOffsetZ;
    
    lastReading.gyroX = (gyroX_raw * GYRO_SCALE) + gyroOffsetX;
    lastReading.gyroY = (gyroY_raw * GYRO_SCALE) + gyroOffsetY;
    lastReading.gyroZ = (gyroZ_raw * GYRO_SCALE) + gyroOffsetZ;
    
    lastReading.temperature = (temp_raw / TEMP_SCALE) + (TEMP_OFFSET_VAL / TEMP_SCALE) + tempOffset;
    
    lastReading.isValid = true;
    lastReading.errorCount = 0;
    
    return true;
}

void MPU6050Sensor::enable() {
    isInitialized = true;
}

void MPU6050Sensor::disable() {
    isInitialized = false;
}

bool MPU6050Sensor::isEnabled() const {
    return isInitialized;
}

MPU6050Data MPU6050Sensor::getLastReading() const {
    return lastReading;
}

void MPU6050Sensor::setAccelOffset(float x, float y, float z) {
    accelOffsetX = x;
    accelOffsetY = y;
    accelOffsetZ = z;
}

void MPU6050Sensor::setGyroOffset(float x, float y, float z) {
    gyroOffsetX = x;
    gyroOffsetY = y;
    gyroOffsetZ = z;
}

void MPU6050Sensor::setTempOffset(float offset) {
    tempOffset = offset;
}

bool MPU6050Sensor::isValid() const {
    return lastReading.isValid;
}

uint8_t MPU6050Sensor::getErrorCount() const {
    return lastReading.errorCount;
}