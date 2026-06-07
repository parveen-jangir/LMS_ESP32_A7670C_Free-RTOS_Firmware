// ============================================================
// StorageManager.cpp
// ============================================================

#include "storage_manager.h"

bool StorageManager::begin(bool formatOnFail)
{
    if (!SPIFFS.begin(formatOnFail))
    {
        Serial.println("[SPIFFS] Mount Failed");
        return false;
    }

    // Create required directories

    if (!SPIFFS.exists("/systemLog"))
    {
        SPIFFS.mkdir("/systemLog");
    }

    Serial.println("[SPIFFS] Mounted Successfully");

    return true;
}

bool StorageManager::writeJsonToFile(const String &path, const JsonDocument &doc)
{
    File file = SPIFFS.open(path, FILE_WRITE);

    if (!file)
    {
        Serial.println("[SPIFFS] Failed To Open File For Writing");
        return false;
    }

    if (serializeJson(doc, file) == 0)
    {
        Serial.println("[SPIFFS] Failed To Write JSON");

        file.close();

        return false;
    }

    file.close();

    Serial.print("[SPIFFS] Saved: ");
    Serial.println(path);

    return true;
}

bool StorageManager::loadJson(const String &path, JsonDocument &doc)
{
    if (!SPIFFS.exists(path))
    {
        Serial.print("[SPIFFS] File Not Found: ");
        Serial.println(path);

        return false;
    }

    File file = SPIFFS.open(path, FILE_READ);

    if (!file)
    {
        Serial.println("[SPIFFS] Failed To Open File");

        return false;
    }

    DeserializationError error =
        deserializeJson(doc, file);

    file.close();

    if (error)
    {
        Serial.print("[JSON] Parse Failed: ");
        Serial.println(error.c_str());

        return false;
    }

    return true;
}

bool StorageManager::deleteFile(const String &path)
{
    if (!SPIFFS.exists(path))
    {
        Serial.println("[SPIFFS] File Does Not Exist");

        return false;
    }

    bool result = SPIFFS.remove(path);

    if (result)
    {
        Serial.print("[SPIFFS] Deleted: ");
        Serial.println(path);
    }
    else
    {
        Serial.println("[SPIFFS] Delete Failed");
    }

    return result;
}

size_t StorageManager::totalSpace()
{
    return SPIFFS.totalBytes();
}

size_t StorageManager::usedSpace()
{
    return SPIFFS.usedBytes();
}

size_t StorageManager::freeSpace()
{
    return totalSpace() - usedSpace();
}

/* ============================================================
   PRINT SPIFFS INFO
   ============================================================ */
void StorageManager::printSPIFFSInfo()
{
    Serial.println("\n========== SPIFFS INFO ==========");

    Serial.print("Total Space : ");
    Serial.println(totalSpace());

    Serial.print("Used Space  : ");
    Serial.println(usedSpace());

    Serial.print("Free Space  : ");
    Serial.println(freeSpace());

    Serial.println(F("================================="));
}

//--------------------------------------------------
// Get Sensor State
//--------------------------------------------------
bool StorageManager::isSensorState(String sensorName)
{
    // Open pref namespace
    if(!pref.begin("sensor_stat", true)) // true = read only
    {
        return false;
    }

    // Create key name
    String key = "sensor_" + sensorName;

    // Read value
    bool status = pref.getBool(key.c_str(), false);

    // Close pref
    pref.end();

    return status;
}

//--------------------------------------------------
// Set Sensor State
//--------------------------------------------------
bool StorageManager::setSensorState(String sensorName, bool isEnabled)
{
    // Open pref namespace
    if (!pref.begin("sensor_stat", false)) // false = read/write
    {
        return false;
    }

    // Create key name
    String key = "sensor_" + sensorName;

    // Save value
    bool result = pref.putBool(key.c_str(), isEnabled);

    // Close pref
    pref.end();

    return result;
}

bool StorageManager::clearAllData(){

    Serial.println("[SPIFFS] Formatting...");

    if (!SPIFFS.format())
    {
        Serial.println("[SPIFFS] Format Failed");

        return false;
    }

    Serial.println("[SPIFFS] Format Success");

    //--------------------------------------------------
    // REMOUNT SPIFFS
    //--------------------------------------------------

    if (!SPIFFS.begin(true))
    {
        Serial.println("[SPIFFS] Remount Failed");

        return false;
    }

    //--------------------------------------------------
    // RECREATE DIRECTORIES
    //--------------------------------------------------

    SPIFFS.mkdir("/systemLog");

    Serial.println("[SPIFFS] Directories Recreated");

    return true;
}