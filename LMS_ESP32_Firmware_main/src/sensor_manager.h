#pragma once
// ============================================================
// sensor_manager.h — Sensor abstraction layer
// Manages all 5 sensors: BMP180, BH1750, MPU6050, DHT22, Soil
// ============================================================

#include <Arduino.h>
#include <ArduinoJson.h>

// ── Sensor IDs ───────────────────────────────────────────────
enum SensorID {
    SENSOR_BMP180  = 0,
    SENSOR_BH1750  = 1,
    SENSOR_MPU6050 = 2,
    SENSOR_DHT22   = 3,
    SENSOR_SOIL    = 4,
    SENSOR_COUNT   = 5
};

// ── Sensor health states ──────────────────────────────────────
enum SensorStatus {
    STATUS_ONLINE  = 0,
    STATUS_OFFLINE = 1,   // disabled by user
    STATUS_ERROR   = 2    // init or read failure
};

// ── Per-sensor state ──────────────────────────────────────────
struct SensorState {
    const char*  name;
    bool         enabled;
    SensorStatus status;
    bool         initialized;
};

// ── Forward declarations for sensor data structs ─────────────
struct BMP180Data  { float temperature; float pressure; };
struct BH1750Data  { float lux; };
struct MPU6050Data { float ax; float ay; float az;
                     float gx; float gy; float gz;
                     float temp; };
struct DHT22Data   { float temperature; float humidity; };
struct SoilData    { int   raw; float percent; };

// ============================================================
class SensorManager {
public:
    SensorManager();

    // Call once in setup()
    void begin();

    // Call every loop() — non-blocking, respects SENSOR_UPDATE_MS
    void update();

    // Enable or disable a sensor (called from WS command)
    void setEnabled(SensorID id, bool enabled);
    bool isEnabled(SensorID id) const;
    SensorStatus getStatus(SensorID id) const;

    // Fill a JSON object with latest data for one sensor
    // Returns false if sensor is disabled or errored
    bool getSensorDataJson(SensorID id, JsonObject& obj);

    // Fill a JSON object with status of all sensors
    void getAllStatusJson(JsonObject& obj);

    // Latest readings (public for direct access if needed)
    BMP180Data  bmp180;
    BH1750Data  bh1750;
    MPU6050Data mpu6050;
    DHT22Data   dht22;
    SoilData    soil;

private:
    SensorState _state[SENSOR_COUNT];
    uint32_t    _lastUpdate;

    // Individual init functions — return true on success
    bool _initBMP180();
    bool _initBH1750();
    bool _initMPU6050();
    bool _initDHT22();
    bool _initSoil();

    // Individual read functions — return true on success
    bool _readBMP180();
    bool _readBH1750();
    bool _readMPU6050();
    bool _readDHT22();
    bool _readSoil();

    // SCP (short circuit protection) check — returns false if fuse blown
    bool _checkSCP(SensorID id);
};

// Global instance declared in sensor_manager.cpp
extern SensorManager sensorManager;
