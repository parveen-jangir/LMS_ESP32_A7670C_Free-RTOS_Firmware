#pragma once
// ============================================================
// sensor_manager.h — Sensor abstraction layer
// Manages all 5 sensors: BMP180, BH1750, MPU6050, DHT22, Soil
// ============================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include <Adafruit_BMP085.h>
#include <BH1750.h>
#include "DHT.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

// ── Sensor IDs ───────────────────────────────────────────────
enum SensorID
{
    SENSOR_BMP180 = 0,
    SENSOR_BH1750 = 1,
    SENSOR_MPU6050 = 2,
    SENSOR_DHT22 = 3,
    SENSOR_SOIL = 4,
    SENSOR_COUNT = 5
};

enum SCP_ID
{
    MPU = 0,
    Soil = 1,
    Weather = 2
};

// ── Sensor health states ──────────────────────────────────────
enum SensorStatus
{
    STATUS_ONLINE = 0,
    STATUS_OFFLINE = 1, // disabled by user
    STATUS_ERROR = 2    // init or read failure
};

// ── Per-sensor state ──────────────────────────────────────────
struct SensorState
{
    const char *name;
    bool enabled;
    SensorStatus status;
    bool initialized;
};

// ── Forward declarations for sensor data structs ─────────────
struct BMP180
{
    float temperature;
    int32_t pressure;
};
struct BH1750Data
{
    float lux;
};
struct MPU6050Data
{
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float roll;
    float yaw;
    float pitch;
    float temp;
    uint32_t count;
};
struct DHT22Data
{
    float temperature;
    float humidity;
};
struct SoilData
{
    int raw_mv;
    float percent;
};

// ============================================================
class SensorManager
{
public:
    SensorManager();

    // Call once in setup()
    void begin();
    bool MPU_init();

    // Call every loop() — non-blocking, respects SENSOR_UPDATE_MS
    void update();
    bool getMPUData();
    bool calibrateMPU();

    SensorStatus getStatus(SensorID id) const;

    // Fill a JSON object with latest data for one sensor
    // Returns false if sensor is disabled or errored
    bool getSensorDataJson(SensorID id, JsonObject &obj);

    // Fill a JSON object with status of all sensors
    void getAllStatusJson(JsonObject &obj);

    // Latest readings (public for direct access if needed)
    BMP180 bmp180;
    BH1750Data bh1750;
    MPU6050Data mpu6050;
    DHT22Data dht22;
    SoilData soil;

private:
    SensorState _state[SENSOR_COUNT];
    uint32_t _lastUpdate;

    // SCP (short circuit protection) check — returns false if fuse blown
    bool _checkSCP(SCP_ID id);
};

// Global instance declared in sensor_manager.cpp
extern SensorManager sensorManager;
