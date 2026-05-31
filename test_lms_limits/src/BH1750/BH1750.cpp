#include "BH1750.h"

BH1750Sensor::BH1750Sensor()
    : isInitialized(false), calibrationOffset(BH1750_DEFAULT_OFFSET)
{
    lastReading = {0.0F, false, 0};
}

bool BH1750Sensor::initialize()
{

    bool _bh1750Ready = lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE); // CONTINUOUS_HIGH_RES_MODE_2
    if (!_bh1750Ready)
    {
        Serial.println(F("[BH1750] Sensor not found!"));
    }
    if (_bh1750Ready && DEBUG_ENABLED)
    {
        Serial.println(F("[BH1750] Initialized successfully."));
    }

    isInitialized = true;
    lastReading.isValid = true;

    return true;
}

bool BH1750Sensor::readSensor()
{
    if (!isInitialized)
        return false;

    // Request measurement
    float lux = NAN;
    while (!lightMeter.measurementReady(true))
    {
        yield();
    }
    lux = lightMeter.readLightLevel();

    if (lux == -1)
    {
        lastReading.errorCount++;
        lastReading.isValid = false;
        if (DEBUG_ENABLED)
        {
            Serial.println(F("[BH1750] Failed to read sensor data!"));
        }
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    // Read data (2 bytes)
    if (lux == -2)
    {
        lastReading.errorCount++;
        lastReading.isValid = false;
        if (DEBUG_ENABLED)
        {
            Serial.println(F("[BH1750] Failed to read sensor data!"));
        }
        return false;
    }

    lastReading.illuminance = lux + calibrationOffset;
    lastReading.isValid = true;
    lastReading.errorCount = 0;

    return true;
}

void BH1750Sensor::enable()
{
    isInitialized = true;
}

void BH1750Sensor::disable()
{
    isInitialized = false;
}

bool BH1750Sensor::isEnabled() const
{
    return isInitialized;
}

BH1750Data BH1750Sensor::getLastReading() const
{
    return lastReading;
}

void BH1750Sensor::setOffset(float offset)
{
    calibrationOffset = offset;
}

float BH1750Sensor::getOffset() const
{
    return calibrationOffset;
}

bool BH1750Sensor::isValid() const
{
    return lastReading.isValid;
}

uint8_t BH1750Sensor::getErrorCount() const
{
    return lastReading.errorCount;
}