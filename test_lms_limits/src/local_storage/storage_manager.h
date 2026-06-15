#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <Preferences.h>

class StorageManager
{
public:
    bool isSensorState(String sensorName);
    bool setSensorState(String sensorName, bool isEnabled);

    bool begin(bool formatOnFail = true);

    bool loadJson(const String &path, JsonDocument &doc);
    bool deleteFile(const String &path);

    bool clearAllData();

    // SPIFFS Info
    void printSPIFFSInfo();

    size_t totalSpace();
    size_t usedSpace();
    size_t freeSpace();

private:
    bool writeJsonToFile(const String &path, const JsonDocument &doc);
    
    Preferences pref;
};

#endif

/*
*To add
1. Calibration offset of MPU's Acc and Gyro
2. Status of calibration
3. Calibaration of Soil Max and Min
*/ 
