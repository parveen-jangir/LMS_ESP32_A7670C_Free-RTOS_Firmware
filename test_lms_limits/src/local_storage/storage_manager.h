#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <Preferences.h>
#include "DataLogger/DataLogger.h"
#include "config.h"

class StorageManager
{
public:
    StorageManager( DataLogger &dataLogger);

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

    void saveTid(const String &tid);
    String getTid();

private:
    DataLogger &dataLogger;
    bool writeJsonToFile(const String &path, const JsonDocument &doc);
    
    Preferences pref;
};

#endif