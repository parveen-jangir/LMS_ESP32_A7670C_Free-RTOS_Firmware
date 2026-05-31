#include "IMU6050.h"

MPU6050 mpu;

extern uint8_t MPU_Accel_Sensitivity = MPU6050_ACCEL_FS_2;
extern uint8_t MPU_Gyro_Sensitivity = MPU6050_GYRO_FS_250;
extern uint8_t MPU_DLPF = 4;
extern float ACCEL_SCALE = 1.0f / 16384.0f; // For ±2g range
extern float GYRO_SCALE = 1.0f / 131.0f;    // For ±250°/s range
extern float TEMP_SCALE = 1.0f / 340.0f;    // Celsius per LSB
extern const float TEMP_OFFSET_VAL = 36.53; // 0°C register value offset
extern volatile uint8_t intStatus = 0; // Clear the interrupt status
IMU6050Sensor::IMU6050Sensor(TwoWire *wireInterface)
    : wire(wireInterface), address(MPU6050_ADDRESS), isInitialized(false),
      accelOffsetX(0), accelOffsetY(0), accelOffsetZ(0),
      gyroOffsetX(0), gyroOffsetY(0), gyroOffsetZ(0),
      tempOffset(0)
{
    lastReading = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0, false, 0};
}

void IRAM_ATTR IMU6050Sensor::IMU_isrHandler()
{
    delayMicroseconds(10); // Debounce delay
    if (digitalRead(MPU_INTERRUPT_PIN) == HIGH)
    {
        intStatus = mpu.getIntStatus(); // Clear the interrupt status
    }
}

bool IMU6050Sensor::initialize()
{
    // Initialize MPU6050 interrupt pin
    pinMode(MPU_INTERRUPT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(MPU_INTERRUPT_PIN), IMU_isrHandler, RISING);

    if (!wire)
        return false;

    wire->begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(100);

    mpu.initialize();

    // Set DLPF to setting 3 (42Hz Bandwidth)
    mpu.setDLPFMode(MPU_DLPF);
    // Set Gyroscope Sensitivity
    mpu.setFullScaleGyroRange(MPU_Gyro_Sensitivity);
    // Set Accelerometer Sensitivity
    mpu.setFullScaleAccelRange(MPU_Accel_Sensitivity);

    calibrate();

    // ── INT pin config ────────────────────────────────
    mpu.setInterruptMode(0);       // 0 = Active HIGH,  1 = Active LOW
    mpu.setInterruptDrive(0);      // 0 = Push-Pull,    1 = Open-Drain
    mpu.setInterruptLatch(0);      // 0 = 50µs pulse,   1 = Hold until cleared
    mpu.setInterruptLatchClear(1); // 0 = clear only on INT_STATUS read, 1 = clear on ANY register read

    // ── Sample rate for motion detector ──────────────
    // Internal rate 1kHz (DLPF on) / (1+9) = 100Hz
    // Motion detector checks at 100Hz
    mpu.setRate(9);

    // ── Hardware motion detector ──────────────────────
    mpu.setMotionDetectionThreshold(1); // 2mg — minimum
    mpu.setMotionDetectionDuration(1);  // 1ms — minimum
    mpu.setIntMotionEnabled(true);
    Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

    isInitialized = true;
    lastReading.isValid = true;

    if (DEBUG_ENABLED)
        Serial.println(F("[MPU6050] Initialized successfully"));
    return true;
}

bool IMU6050Sensor::calibrate()
{
    // Need to stroe and load data form NVS for calibration offsets to avoid doing it every time at startup, but for now just do it once at startup

    // Perform calibration using MPU6050 library's built-in functions one time at startup
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    return true;
}

bool IMU6050Sensor::readSensor()
{
    if (!isInitialized)
        return false;

    mpu.getMotion6(&aa.x, &aa.y, &aa.z, &gg.x, &gg.y, &gg.z);

    // Read raw acceleration data
    int16_t accelX_raw = aa.x;
    int16_t accelY_raw = aa.y;
    int16_t accelZ_raw = aa.z;

    // Skip temperature (not needed right now)
    int16_t temp_raw = mpu.getTemperature();

    // Read raw gyro data
    int16_t gyroX_raw = gg.x;
    int16_t gyroY_raw = gg.y;
    int16_t gyroZ_raw = gg.z;

    // Convert to physical units with calibration offset
    lastReading.accelX = (accelX_raw * ACCEL_SCALE) + accelOffsetX;
    lastReading.accelY = (accelY_raw * ACCEL_SCALE) + accelOffsetY;
    lastReading.accelZ = (accelZ_raw * ACCEL_SCALE) + accelOffsetZ;

    lastReading.gyroX = (gyroX_raw * GYRO_SCALE) + gyroOffsetX;
    lastReading.gyroY = (gyroY_raw * GYRO_SCALE) + gyroOffsetY;
    lastReading.gyroZ = (gyroZ_raw * GYRO_SCALE) + gyroOffsetZ;

    lastReading.temperature = ((temp_raw * TEMP_SCALE) + TEMP_OFFSET_VAL) + tempOffset;

    lastReading.isValid = true;
    lastReading.errorCount = 0;

    return true;
}

void IMU6050Sensor::changeAccelSensitivity(uint8_t MPU_Accel_Sensitivity)
{
    mpu.setFullScaleAccelRange(MPU_Accel_Sensitivity);

    uint8_t range = mpu.getFullScaleAccelRange();
    if (DEBUG_ENABLED)
    {
        Serial.printf("[MPU6050] Accelerometer sensitivity changed to ±%dg\n", range);
    }

    switch (range)
    {
    case MPU6050_ACCEL_FS_2:
        Serial.println("[MPU6050] Accelerometer scale set to ±2g");
        ACCEL_SCALE = 1.0f / 16384.0f;
        break;
    case MPU6050_ACCEL_FS_4:
        Serial.println("[MPU6050] Accelerometer scale set to ±4g");
        ACCEL_SCALE = 1.0f / 8192.0f;
        break;
    case MPU6050_ACCEL_FS_8:
        Serial.println("[MPU6050] Accelerometer scale set to ±8g");
        ACCEL_SCALE = 1.0f / 4096.0f;
        break;
    case MPU6050_ACCEL_FS_16:
        Serial.println("[MPU6050] Accelerometer scale set to ±16g");
        ACCEL_SCALE = 1.0f / 2048.0f;
        break;
    default:
        Serial.println("[MPU6050] Unknown accelerometer sensitivity setting");
    }
}

void IMU6050Sensor::changeGyroSensitivity(uint8_t MPU_Gyro_Sensitivity)
{
    mpu.setFullScaleGyroRange(MPU_Gyro_Sensitivity);

    uint8_t range = mpu.getFullScaleGyroRange();
    if (DEBUG_ENABLED)
    {
        Serial.printf("[MPU6050] Gyroscope sensitivity changed to ±%d°/s\n", range);
    }

    switch (range)
    {
    case MPU6050_GYRO_FS_250:
        Serial.println("[MPU6050] Gyroscope scale set to ±250°/s");
        GYRO_SCALE = 1.0f / 131.0f;
        break;
    case MPU6050_GYRO_FS_500:
        Serial.println("[MPU6050] Gyroscope scale set to ±500°/s");
        GYRO_SCALE = 1.0f / 65.5f;
        break;
    case MPU6050_GYRO_FS_1000:
        Serial.println("[MPU6050] Gyroscope scale set to ±1000°/s");
        GYRO_SCALE = 1.0f / 32.8f;
        break;
    case MPU6050_GYRO_FS_2000:
        Serial.println("[MPU6050] Gyroscope scale set to ±2000°/s");
        GYRO_SCALE = 1.0f / 16.4f;
        break;
    default:
        Serial.println("[MPU6050] Unknown gyroscope sensitivity setting");
    }
}

void IMU6050Sensor::changeDLPFcoff(uint8_t MPU_DLPF)
{
    mpu.setDLPFMode(MPU_DLPF);

    uint8_t dlpfSetting = mpu.getDLPFMode();
    if (DEBUG_ENABLED)
    {
        Serial.printf("[MPU6050] DLPF setting changed to %d\n", dlpfSetting);
    }

    switch (dlpfSetting)
    {
    case MPU6050_DLPF_BW_256:
        Serial.println("[MPU6050] DLPF bandwidth set to 256Hz");
        break;
    case MPU6050_DLPF_BW_188:
        Serial.println("[MPU6050] DLPF bandwidth set to 188Hz");
        break;
    case MPU6050_DLPF_BW_98:
        Serial.println("[MPU6050] DLPF bandwidth set to 98Hz");
        break;
    case MPU6050_DLPF_BW_42:
        Serial.println("[MPU6050] DLPF bandwidth set to 42Hz");
        break;
    case MPU6050_DLPF_BW_20:
        Serial.println("[MPU6050] DLPF bandwidth set to 20Hz");
        break;
    case MPU6050_DLPF_BW_10:
        Serial.println("[MPU6050] DLPF bandwidth set to 10Hz");
        break;
    case MPU6050_DLPF_BW_5:
        Serial.println("[MPU6050] DLPF bandwidth set to 5Hz");
        break;
    default:
        Serial.println("[MPU6050] Unknown DLPF setting");
    }
}

void IMU6050Sensor::enable()
{
    isInitialized = true;
}

void IMU6050Sensor::disable()
{
    isInitialized = false;
}

bool IMU6050Sensor::isEnabled() const
{
    return isInitialized;
}

MPU6050Data IMU6050Sensor::getLastReading() const
{
    return lastReading;
}

void IMU6050Sensor::setAccelOffset(float x, float y, float z)
{
    accelOffsetX = x;
    accelOffsetY = y;
    accelOffsetZ = z;
}

void IMU6050Sensor::setGyroOffset(float x, float y, float z)
{
    gyroOffsetX = x;
    gyroOffsetY = y;
    gyroOffsetZ = z;
}

void IMU6050Sensor::setTempOffset(float offset)
{
    tempOffset = offset;
}

bool IMU6050Sensor::isValid() const
{
    return lastReading.isValid;
}

uint8_t IMU6050Sensor::getErrorCount() const
{
    return lastReading.errorCount;
}