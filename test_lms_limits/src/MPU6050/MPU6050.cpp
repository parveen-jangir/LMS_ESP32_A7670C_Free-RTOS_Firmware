#include "MPU6050.h"

volatile uint32_t MPU6050Sensor::MotionCount = 0;


MPU6050Sensor::MPU6050Sensor(TwoWire *wireInterface)
    : wire(wireInterface), address(MPU6050_ADDRESS), isInitialized(false),
      accelOffsetX(0), accelOffsetY(0), accelOffsetZ(0),
      gyroOffsetX(0), gyroOffsetY(0), gyroOffsetZ(0),
      tempOffset(0),
      accelOffsetRawX(0), accelOffsetRawY(0), accelOffsetRawZ(0),
      gyroOffsetRawX(0), gyroOffsetRawY(0), gyroOffsetRawZ(0),
      interruptPin(-1)
{
    lastReading = {0, 0, 0, 0, 0, 0, 0, 0, false, 0};
}

bool MPU6050Sensor::writeRegister(uint8_t reg, uint8_t value)
{
    wire->beginTransmission(address);
    wire->write(reg);
    wire->write(value);
    return wire->endTransmission() == 0;
}

bool MPU6050Sensor::writeRegister16(uint8_t reg, int16_t value)
{
    wire->beginTransmission(address);
    wire->write(reg);
    wire->write((uint8_t)((value >> 8) & 0xFF));
    wire->write((uint8_t)(value & 0xFF));
    return wire->endTransmission() == 0;
}

bool MPU6050Sensor::readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    wire->beginTransmission(address);
    wire->write(reg);
    if (wire->endTransmission(false) != 0)
        return false; // repeated start, no STOP
    if (wire->requestFrom(address, (uint8_t)length, (uint8_t)1) != length)
        return false;
    for (uint8_t i = 0; i < length; i++)
        buffer[i] = wire->read();
    return true;
}

bool MPU6050Sensor::readRegister16(uint8_t reg, int16_t *value)
{
    uint8_t buf[2];
    if (!readRegisters(reg, buf, 2))
        return false;
    *value = ((int16_t)buf[0] << 8) | buf[1];
    return true;
}

bool MPU6050Sensor::readRawMotion(int16_t *ax, int16_t *ay, int16_t *az,
                                  int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buffer[14];
    if (!readRegisters(0x3B, buffer, 14))
        return false;

    *ax = ((int16_t)buffer[0] << 8) | buffer[1];
    *ay = ((int16_t)buffer[2] << 8) | buffer[3];
    *az = ((int16_t)buffer[4] << 8) | buffer[5];
    // buffer[6],[7] = temperature, skipped
    *gx = ((int16_t)buffer[8] << 8) | buffer[9];
    *gy = ((int16_t)buffer[10] << 8) | buffer[11];
    *gz = ((int16_t)buffer[12] << 8) | buffer[13];

    return true;
}

bool MPU6050Sensor::setAccelOffsetRaw(int16_t x, int16_t y, int16_t z)
{
    // XA_OFFS_H/L (0x06-0x07), YA_OFFS (0x08-0x09), ZA_OFFS (0x0A-0x0B)
    if (!writeRegister16(0x06, x))
        return false;
    if (!writeRegister16(0x08, y))
        return false;
    if (!writeRegister16(0x0A, z))
        return false;
    accelOffsetRawX = x;
    accelOffsetRawY = y;
    accelOffsetRawZ = z;
    return true;
}

bool MPU6050Sensor::setGyroOffsetRaw(int16_t x, int16_t y, int16_t z)
{
    // XG_OFFS_USR (0x13-0x14), YG_OFFS (0x15-0x16), ZG_OFFS (0x17-0x18)
    if (!writeRegister16(0x13, x))
        return false;
    if (!writeRegister16(0x15, y))
        return false;
    if (!writeRegister16(0x17, z))
        return false;
    gyroOffsetRawX = x;
    gyroOffsetRawY = y;
    gyroOffsetRawZ = z;
    return true;
}
bool MPU6050Sensor::clearInterrupt()
{
    uint8_t intStatus;
    Serial.println("[MPU6050] Clearing interrupt...");
    return readRegisters(0x3A, &intStatus, 1);
}

bool MPU6050Sensor::initialize(int interruptPinNum)
{
    if (!wire)
        return false;

    interruptPin = interruptPinNum;

    wire->begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time for I2C bus to stabilize

    // Check if MPU6050 is present
    wire->beginTransmission(address);
    if (wire->endTransmission() != 0)
    {
        if (DEBUG_ENABLED)
            Serial.println("[MPU6050] Sensor not found!");
        return false;
    }

    // Wake up from sleep mode
    if (!writeRegister(0x6B, 0x00))
        return false;
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time to wake up

    // Run self-test before applying final configuration
    if (!selfTest())
    {
        if (DEBUG_ENABLED)
            Serial.println("[MPU6050] Self-test failed!");
        return false;
    }

    // Set accelerometer range to ±2g
    if (!writeRegister(0x1C, 0x00))
        return false;

    // Set gyro range to ±250°/s
    if (!writeRegister(0x1B, 0x00))
        return false;

    // Set Digital Low Pass Filter to ~44Hz (DLPF_CFG = 3 in CONFIG register 0x1A)
    if (!setDLPF(0x03)) return false;
    // Attach motion interrupt ISR if a valid pin was provided
    if (interruptPin >= 0)
    {
        pinMode(interruptPin, INPUT);
        attachInterrupt(digitalPinToInterrupt(interruptPin), onMotionISR, RISING);
        if (DEBUG_ENABLED)
        {
            Serial.print("[MPU6050] Motion ISR attached on pin ");
            Serial.println(interruptPin);
        }
    }
    enableMotionInterrupt(1, 1); // Example threshold/duration for motion interrupt
    isInitialized = true;
    lastReading.isValid = true;

    if (DEBUG_ENABLED)
        Serial.println("[MPU6050] Initialized successfully");
    return true;
}

bool MPU6050Sensor::selfTest()
{
    // Backup current config registers
    uint8_t gyroConfigBackup, accelConfigBackup;
    if (!readRegisters(0x1B, &gyroConfigBackup, 1))
        return false;
    if (!readRegisters(0x1C, &accelConfigBackup, 1))
        return false;

    // Enable self-test on all axes, set full scale ranges required for self-test
    if (!writeRegister(0x1B, 0xE0))
        return false;
    if (!writeRegister(0x1C, 0xF0))
        return false;

    vTaskDelay(pdMS_TO_TICKS(25)); // Allow oscillations to stabilize per datasheet

    uint8_t selfTestData[4];
    if (!readRegisters(0x0D, selfTestData, 4))
    {
        writeRegister(0x1B, gyroConfigBackup);
        writeRegister(0x1C, accelConfigBackup);
        return false;
    }

    // Restore original configuration
    bool restoreOk = writeRegister(0x1B, gyroConfigBackup) && writeRegister(0x1C, accelConfigBackup);

    vTaskDelay(pdMS_TO_TICKS(25)); // Allow sensor to settle back to normal mode

    if (!restoreOk)
        return false;

    bool nonZero = false;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (selfTestData[i] != 0)
            nonZero = true;
    }

    return nonZero;
}

bool MPU6050Sensor::setDLPF(uint8_t dlpfMode)
{
    // CONFIG register 0x1A, bits [2:0] = DLPF_CFG
    return writeRegister(0x1A, dlpfMode & 0x07);
}

bool MPU6050Sensor::enableMotionInterrupt(uint8_t threshold, uint8_t duration)
{
    if (!writeRegister(0x1F, threshold))
        return false; // MOT_THR
    if (!writeRegister(0x20, duration))
        return false; // MOT_DUR

    uint8_t accelConfig;
    if (!readRegisters(0x1C, &accelConfig, 1))
        return false;
    accelConfig = (accelConfig & 0xF8) | 0x01; // ACCEL_HPF = 1 (5Hz)
    if (!writeRegister(0x1C, accelConfig))
        return false;

    if (!writeRegister(0x37, 0x30))
        return false; // INT_LEVEL=0 (active high), LATCH_INT_EN=0 (50us pulse), INT_RD_CLEAR=0
    if (!writeRegister(0x38, 0x40))
        return false; // INT_ENABLE: MOT_EN

    vTaskDelay(pdMS_TO_TICKS(10)); // allow HPF to settle

    return true;
}

bool MPU6050Sensor::calibrate(uint8_t loopCount, uint16_t samplesPerLoop)
{
    if (!wire)
        return false;

    // Start from zero hardware offsets
    int16_t ax_offset = 0, ay_offset = 0, az_offset = 0;
    int16_t gx_offset = 0, gy_offset = 0, gz_offset = 0;

    // Read existing offsets as starting point (Rowberg approach divides by 8 / 4 for accel/gyro
    // due to register resolution vs raw LSB resolution differences)
    int16_t curAXOffset, curAYOffset, curAZOffset;
    int16_t curGXOffset, curGYOffset, curGZOffset;
    if (!readRegister16(0x06, &curAXOffset))
        return false;
    if (!readRegister16(0x08, &curAYOffset))
        return false;
    if (!readRegister16(0x0A, &curAZOffset))
        return false;
    if (!readRegister16(0x13, &curGXOffset))
        return false;
    if (!readRegister16(0x15, &curGYOffset))
        return false;
    if (!readRegister16(0x17, &curGZOffset))
        return false;

    ax_offset = curAXOffset / 8;
    ay_offset = curAYOffset / 8;
    az_offset = curAZOffset / 8;
    gx_offset = curGXOffset / 4;
    gy_offset = curGYOffset / 4;
    gz_offset = curGZOffset / 4;

    const int16_t ACCEL_1G = 16384; // LSB for ±2g range

    for (uint8_t loop = 0; loop < loopCount; loop++)
    {
        // Apply current offset estimate to hardware registers
        if (!setAccelOffsetRaw(ax_offset * 8, ay_offset * 8, az_offset * 8))
            return false;
        if (!setGyroOffsetRaw(gx_offset * 4, gy_offset * 4, gz_offset * 4))
            return false;

        vTaskDelay(pdMS_TO_TICKS(2)); // let registers settle

        long axSum = 0, aySum = 0, azSum = 0;
        long gxSum = 0, gySum = 0, gzSum = 0;

        for (uint16_t i = 0; i < samplesPerLoop; i++)
        {
            int16_t ax, ay, az, gx, gy, gz;
            if (!readRawMotion(&ax, &ay, &az, &gx, &gy, &gz))
            {
                return false;
            }
            axSum += ax;
            aySum += ay;
            azSum += az;
            gxSum += gx;
            gySum += gy;
            gzSum += gz;

            if (i % 100 == 0)
                vTaskDelay(pdMS_TO_TICKS(1)); // yield to avoid starving FreeRTOS scheduler
        }

        long axMean = axSum / samplesPerLoop;
        long ayMean = aySum / samplesPerLoop;
        long azMean = azSum / samplesPerLoop;
        long gxMean = gxSum / samplesPerLoop;
        long gyMean = gySum / samplesPerLoop;
        long gzMean = gzSum / samplesPerLoop;

        // Adjust offset estimates: drive accel X,Y -> 0, Z -> +1g, gyro all -> 0
        ax_offset -= (int16_t)(axMean / 8);
        ay_offset -= (int16_t)(ayMean / 8);
        az_offset += (int16_t)((ACCEL_1G - azMean) / 8);

        gx_offset -= (int16_t)(gxMean / 4);
        gy_offset -= (int16_t)(gyMean / 4);
        gz_offset -= (int16_t)(gzMean / 4);

        if (DEBUG_ENABLED)
        {
            Serial.printf("[MPU6050] Cal loop %d: A(%ld,%ld,%ld) G(%ld,%ld,%ld)\n",
                          loop, axMean, ayMean, azMean, gxMean, gyMean, gzMean);
        }
    }

    // Final write of converged offsets
    if (!setAccelOffsetRaw(ax_offset * 8, ay_offset * 8, az_offset * 8))
        return false;
    if (!setGyroOffsetRaw(gx_offset * 4, gy_offset * 4, gz_offset * 4))
        return false;

    if (DEBUG_ENABLED)
        Serial.println("[MPU6050] Calibration complete");

    return true;
}

portMUX_TYPE MPU6050Sensor::motionMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR MPU6050Sensor::onMotionISR() {
    portENTER_CRITICAL_ISR(&motionMux);
    MotionCount++;
    portEXIT_CRITICAL_ISR(&motionMux);
}

uint32_t MPU6050Sensor::getMotionCount() {
    uint32_t count;
    portENTER_CRITICAL(&motionMux);
    count = MotionCount;
    portEXIT_CRITICAL(&motionMux);
    return count;
}

void MPU6050Sensor::resetMotionCount() {
    portENTER_CRITICAL(&motionMux);
    MotionCount = 0;
    portEXIT_CRITICAL(&motionMux);
}

bool MPU6050Sensor::readSensor()
{
    if (!isInitialized)
        return false;

if (digitalRead(interruptPin) == HIGH) {
        clearInterrupt(); // reads INT_STATUS, releases latched pin
    }

    lastReading.timestamp = millis();

    int16_t accelX_raw, accelY_raw, accelZ_raw;
    int16_t gyroX_raw, gyroY_raw, gyroZ_raw;
    float roll, pitch, yaw;

    uint8_t buffer[14];
    if (!readRegisters(0x3B, buffer, 14))
    {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }

    accelX_raw = ((int16_t)buffer[0] << 8) | buffer[1];
    accelY_raw = ((int16_t)buffer[2] << 8) | buffer[3];
    accelZ_raw = ((int16_t)buffer[4] << 8) | buffer[5];

    int16_t temp_raw = ((int16_t)buffer[6] << 8) | buffer[7];

    gyroX_raw = ((int16_t)buffer[8] << 8) | buffer[9];
    gyroY_raw = ((int16_t)buffer[10] << 8) | buffer[11];
    gyroZ_raw = ((int16_t)buffer[12] << 8) | buffer[13];

    lastReading.accelX = (accelX_raw * ACCEL_SCALE) + accelOffsetX;
    lastReading.accelY = (accelY_raw * ACCEL_SCALE) + accelOffsetY;
    lastReading.accelZ = (accelZ_raw * ACCEL_SCALE) + accelOffsetZ;

    lastReading.gyroX = (gyroX_raw * GYRO_SCALE) + gyroOffsetX;
    lastReading.gyroY = (gyroY_raw * GYRO_SCALE) + gyroOffsetY;
    lastReading.gyroZ = (gyroZ_raw * GYRO_SCALE) + gyroOffsetZ;

    lastReading.temperature = (temp_raw * TEMP_SCALE) + (TEMP_OFFSET_VAL) + tempOffset;
    lastReading.movementCount = getMotionCount();
    lastReading.isValid = true;
    lastReading.errorCount = 0;

    lastReading.roll =
        atan2(lastReading.accelY, lastReading.accelZ) *
        180.0f / PI;

    lastReading.pitch =
        atan2(-lastReading.accelX,
              sqrt(lastReading.accelY * lastReading.accelY +
                   lastReading.accelZ * lastReading.accelZ)) *
        180.0f / PI;

    lastReading.yaw = 0.0f; // Placeholder, replace with actual yaw calculation if needed

    return true;
}

void MPU6050Sensor::enable()
{
    isInitialized = true;
}

void MPU6050Sensor::disable()
{
    isInitialized = false;
}

bool MPU6050Sensor::isEnabled() const
{
    return isInitialized;
}

MPU6050Data MPU6050Sensor::getLastReading() const
{
    return lastReading;
}

void MPU6050Sensor::setAccelOffset(float x, float y, float z)
{
    accelOffsetX = x;
    accelOffsetY = y;
    accelOffsetZ = z;
}

void MPU6050Sensor::setGyroOffset(float x, float y, float z)
{
    gyroOffsetX = x;
    gyroOffsetY = y;
    gyroOffsetZ = z;
}

void MPU6050Sensor::setTempOffset(float offset)
{
    tempOffset = offset;
}

bool MPU6050Sensor::isValid() const
{
    return lastReading.isValid;
}

uint8_t MPU6050Sensor::getErrorCount() const
{
    return lastReading.errorCount;
}