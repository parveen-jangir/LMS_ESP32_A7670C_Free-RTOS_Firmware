// ============================================================
// main.cpp — ESP32 Multi-Sensor Hub
// Framework: Arduino / PlatformIO
// ============================================================

#include "sensor_manager.h"

// ── Wi-Fi state ───────────────────────────────────────────────
static bool _wifiConnected = false;
static bool _bmpReady = false;
static bool _bh1750Ready = false;

Quaternion q;                       // [w, x, y, z]         quaternion container
VectorInt16 aa;                     // [x, y, z]            accel sensor measurements
VectorInt16 gg;                     // [x, y, z]            gyro sensor measurements
VectorFloat gravity;                // [x, y, z]            gravity vector
float ypr[3];                       // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
uint16_t packetSize = 42;           // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;                 // count of all bytes currently in FIFO
uint8_t fifoBuffer[64];             // FIFO storage buffer
uint8_t mpuIntStatus;               // holds actual interrupt status byte from MPU
bool dmpReady = false;              // set true if DMP init was successful
uint8_t devStatus;                  // return status after each device operation (0 = success, !0 = error)
volatile bool mpuInterrupt = false; // indicates whether MPU interrupt pin has gone high

Adafruit_BMP085 bmp;
BH1750 lightMeter;
DHT dht(PIN_DHT, DHT_TYPE);
MPU6050 mpu;

void dmpDataReady()
{
    mpuInterrupt = true;
}

SensorManager::SensorManager()
{
}

void SensorManager::begin()
{
    // Initialize all sensors and update their status
    dht.begin();
    Serial.println(F("DHT sensor initialized."));

    _bmpReady = bmp.begin();
    if (!_bmpReady)
    {
        Serial.println(F("Could not find a valid BMP180 sensor!"));
    }
    else
    {
        Serial.println(F("BMP180 sensor initialized successfully."));
    }

    _bh1750Ready = lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
    if (!_bh1750Ready)
    {
        Serial.println(F("Could not find a valid BH1750 sensor!"));
    }
    else
    {
        Serial.println(F("BH1750 sensor initialized successfully."));
    }
}

void SensorManager::update()
{
    dht22.humidity = dht.readHumidity();
    dht22.temperature = dht.readTemperature();
    float hic = NAN;
    if (isnan(dht22.humidity) || isnan(dht22.temperature))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
    }
    else
    {
        // Compute heat index in Celsius only when the DHT reading is valid.
        hic = dht.computeHeatIndex(dht22.temperature, dht22.humidity, false);
    }

    if (_bmpReady)
    {
        bmp180.pressure = bmp.readPressure();
        if (bmp180.pressure == 0)
        {
            Serial.println(F("Failed to read from BMP180 sensor!"));
            bmp180.pressure = -1;
        }
    }
    else
    {
        bmp180.pressure = -1;
    }

    float lux = NAN;
    if (_bh1750Ready)
    {
        while (!lightMeter.measurementReady(true))
        {
            yield();
        }
        lux = lightMeter.readLightLevel();
    }

    float soilVoltage = analogReadMilliVolts(PIN_SOIL) / 1000.0f;
    Serial.printf("DHT22: Temp=%.1f°C  Humidity=%.1f%%  HeatIndex=%.1f°C\n", dht22.temperature, dht22.humidity, hic);
    if (bmp180.pressure != -1)
    {
        Serial.printf("BMP180: Pressure=%.2f hPa\n", bmp180.pressure / 100.0);
    }
    else
    {
        Serial.println("BMP180: Pressure=ERROR");
    }
    if (!isnan(lux))
    {
        Serial.printf("BH1750: Light=%.2f lx\n", lux);
    }
    else
    {
        Serial.println("BH1750: Light=ERROR");
    }
    Serial.printf("Soil: Voltage=%.3f V\n", soilVoltage);
}

bool SensorManager::_checkSCP(SCP_ID id)
{
    switch (id)
    {
    case MPU:
        uint32_t mpuSCP = analogReadMilliVolts(PIN_SCP_MPU);
        if (mpuSCP <= SCP_LOW_Voltage)
        {
            Serial.println(F("MPU SCP triggered! Sensor may be damaged."));
            return false;
        }
        else
        {
            return true;
        }

    case Soil:
        uint32_t soilSCP = analogReadMilliVolts(PIN_SCP_SOIL);
        if (soilSCP <= SCP_LOW_Voltage)
        {
            Serial.println(F("Soil SCP triggered! Sensor may be damaged."));
            return false;
        }
        else
        {
            return true;
        }
    case Weather:
        uint32_t weatherSCP = analogReadMilliVolts(PIN_SCP_WEATHER);
        if (weatherSCP <= SCP_LOW_Voltage)
        {
            Serial.println(F("Weather SCP triggered! Sensor may be damaged."));
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool SensorManager::MPU_init()
{
    mpu.initialize();
    devStatus = mpu.dmpInitialize();

    if (!mpu.testConnection())
    {
        Serial.println("MPU6050 connection failed! Retrying...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        return false;
    }

    // Set DLPF to setting 3 (42Hz Bandwidth)
    mpu.setDLPFMode(4);
    // Set Gyroscope Sensitivity
    mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
    // Set Accelerometer Sensitivity
    mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);

    Serial.printf("Acc sensitivity is set to %d and Gyro sensitivity is set to %d", mpu.getFullScaleAccelRange(), mpu.getFullScaleGyroRange());
    if (devStatus == 0)
    {
        Serial.println("Calibrating MPU6050... Please keep the sensor still.");
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        Serial.println("MPU6050 calibration complete. Offsets:");
        mpu.PrintActiveOffsets();

        Serial.println("Enabling DMP...");
        mpu.setDMPEnabled(true);

        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(MPU6050_INT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(MPU6050_INT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    }
    else
    {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
    return true;
}

bool SensorManager::getMPUData()
{
    if (!dmpReady)
    {
        Serial.println(F("DMP not ready! Call MPU_init() first."));
        return false;
    }

    // wait for MPU interrupt or extra packet(s) available
    while (!mpuInterrupt && fifoCount < packetSize)
    {
        // other program behavior stuff here
        // .
        // .
        // .
        // if you are really paranoid you can frequently test in the loop to see
        // if mpuInterrupt is true, and if so, "break;" from the while() loop to
        // immediately process the MPU data
    }

    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    // get current FIFO count
    fifoCount = mpu.getFIFOCount();

    if ((mpuIntStatus & 0x10) || fifoCount == 1024)
    {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));

        return false;
    }
    else if (mpuIntStatus & 0x02)
    {
        while (fifoCount < packetSize)
            fifoCount = mpu.getFIFOCount();

        uint8_t fifoBuffer[64];
        mpu.getFIFOBytes(fifoBuffer, packetSize);

        fifoCount -= packetSize;

        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        mpu.dmpGetGyro(&gg, fifoBuffer);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        
        // Convert raw readings to physical units
        mpu6050.ax = (float)aa.x / 16384.0f;
        mpu6050.ay = (float)aa.y / 16384.0f;
        mpu6050.az = (float)aa.z / 16384.0f;

        mpu6050.gx = (float)gg.x / 131.0f;
        mpu6050.gy = (float)gg.y / 131.0f;
        mpu6050.gz = (float)gg.z / 131.0f;

        mpu6050.roll = ypr[2] * 180/M_PI;
        mpu6050.pitch = ypr[1] * 180/M_PI;
        mpu6050.yaw = ypr[0] * 180/M_PI;

        mpu6050.temp = mpu.getTemperature() / 340.0f + 36.53f;

        return true;
    }
    return false;
}

bool SensorManager::calibrateMPU()
{
    
}