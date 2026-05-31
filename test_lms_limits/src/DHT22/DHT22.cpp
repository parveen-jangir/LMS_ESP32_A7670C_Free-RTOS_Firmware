#include "DHT22.h"

DHT22Sensor::DHT22Sensor(uint8_t sensorPin)
    : pin(sensorPin), isInitialized(false),
      calibrationTempOffset(0.0), calibrationHumidityOffset(0.0),
      lastReadTime(0) {
    
    // Create DHT_Unified instance
    dht = new DHT_Unified(pin, DHT_TYPE);
    lastReading = {0.0F, 0.0F, false, 0};
}

DHT22Sensor::~DHT22Sensor() {
    if (dht) {
        delete dht;
        dht = nullptr;
    }
}

bool DHT22Sensor::initialize() {
    if (!dht) return false;
    
    // Initialize the DHT sensor
    dht->begin();
    
    delay(100);
    
    // Get sensor details
    sensor_t sensor;
    dht->temperature().getSensor(&sensor);
    
    if (DEBUG_ENABLED) {
        Serial.println("[DHT22] Sensor Information:");
        Serial.printf("[DHT22]   Name: %s\n"), sensor.name;
        Serial.printf("[DHT22]   Driver Ver: %d\n"), sensor.version;
        Serial.printf("[DHT22]   Unique ID: %lu\n"), sensor.sensor_id;
        Serial.printf("[DHT22]   Max Value: %.2f\n"), sensor.max_value;
        Serial.printf("[DHT22]   Min Value: %.2f\n"), sensor.min_value;
        Serial.printf("[DHT22]   Resolution: %.2f\n"), sensor.resolution;
        Serial.println("[DHT22]   Min Delay: 2000000 µs (2 seconds)");
    }
    
    isInitialized = true;
    lastReading.isValid = true;
    
    if (DEBUG_ENABLED) Serial.println(F("[DHT22] Initialized successfully"));
    return true;
}

bool DHT22Sensor::readSensor() {
    if (!isInitialized || !dht) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        return false;
    }
    
    // Enforce minimum 2s between reads
    unsigned long currentTime = millis();
    if ((currentTime - lastReadTime) < DHT_READING_DELAY_MS) {
        // Return last valid reading if available
        return lastReading.isValid;
    }
    
    lastReadTime = currentTime;
    
    try {
        // Read temperature
        sensors_event_t temperatureEvent;
        dht->temperature().getEvent(&temperatureEvent);
        
        // Read humidity
        sensors_event_t humidityEvent;
        dht->humidity().getEvent(&humidityEvent);
        
        // Check for valid readings
        if (isnan(temperatureEvent.temperature) || isnan(humidityEvent.relative_humidity)) {
            lastReading.errorCount++;
            lastReading.isValid = false;
            if (DEBUG_ENABLED) Serial.println("[DHT22] Invalid sensor readings (NaN)");
            return false;
        }
        
        // Apply calibration offsets
        lastReading.temperature = temperatureEvent.temperature + calibrationTempOffset;
        lastReading.humidity = humidityEvent.relative_humidity + calibrationHumidityOffset;
        
        // Clamp humidity between 0-100%
        if (lastReading.humidity < 0.0f) lastReading.humidity = 0.0f;
        if (lastReading.humidity > 100.0f) lastReading.humidity = 100.0f;
        
        lastReading.isValid = true;
        lastReading.errorCount = 0;
        
        return true;
    }
    catch (...) {
        lastReading.errorCount++;
        lastReading.isValid = false;
        if (DEBUG_ENABLED) Serial.println("[DHT22] Exception during sensor read");
        return false;
    }
}

void DHT22Sensor::enable() {
    isInitialized = true;
}

void DHT22Sensor::disable() {
    isInitialized = false;
}

bool DHT22Sensor::isEnabled() const {
    return isInitialized;
}

DHT22Data DHT22Sensor::getLastReading() const {
    return lastReading;
}

void DHT22Sensor::setTemperatureOffset(float offset) {
    calibrationTempOffset = offset;
}

void DHT22Sensor::setHumidityOffset(float offset) {
    calibrationHumidityOffset = offset;
}

float DHT22Sensor::getTemperatureOffset() const {
    return calibrationTempOffset;
}

float DHT22Sensor::getHumidityOffset() const {
    return calibrationHumidityOffset;
}

bool DHT22Sensor::isValid() const {
    return lastReading.isValid;
}

uint8_t DHT22Sensor::getErrorCount() const {
    return lastReading.errorCount;
}