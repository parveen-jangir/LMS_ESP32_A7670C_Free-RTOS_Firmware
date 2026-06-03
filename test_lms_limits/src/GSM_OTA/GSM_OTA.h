#pragma once

/*
 * GSM_OTA — OTA firmware update for ESP32-S3 via A7670C GSM module
 *
 * Usage:
 *   #include "GSM_OTA.h"
 *
 *   GSM_OTA ota(Serial2, Serial);          // (gsmSerial, debugSerial)
 *   ota.begin(115200, RX_PIN, TX_PIN);     // init UART
 *   ota.setAPN("airtelgprs.com");
 *
 *   OTAResult r = ota.performOTA("https://raw.githubusercontent.com/.../fw.bin");
 *   if (r == OTA_SUCCESS) esp_restart();
 */

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ─── Result codes ─────────────────────────────────────────────────────────────
enum OTAResult {
    OTA_SUCCESS            =  0,
    OTA_ERR_APN            = -1,
    OTA_ERR_NETOPEN        = -2,
    OTA_ERR_URL            = -3,
    OTA_ERR_HTTP_GET       = -4,
    OTA_ERR_HTTP_CODE      = -5,   // non-200 response
    OTA_ERR_NO_PARTITION   = -6,
    OTA_ERR_OTA_BEGIN      = -7,
    OTA_ERR_CHUNK_READ     = -8,
    OTA_ERR_OTA_WRITE      = -9,
    OTA_ERR_OTA_END        = -10,  // validation failed
    OTA_ERR_SET_BOOT       = -11,
};

// ─── Callback types ───────────────────────────────────────────────────────────
typedef void (*OTAProgressCallback)(int percent, int bytesWritten, int totalBytes);
typedef void (*OTALogCallback)(const char* message);

// ─── Default config ───────────────────────────────────────────────────────────
#define GSM_OTA_DEFAULT_APN         "airtelgprs.com"
#define GSM_OTA_CMD_TIMEOUT_MS      10000
#define GSM_OTA_HTTP_TIMEOUT_MS     60000
#define GSM_OTA_CHUNK_SIZE          512
#define GSM_OTA_TRAIL_BYTES         10   // bytes to discard after binary chunk

// ─── Class ────────────────────────────────────────────────────────────────────
class GSM_OTA {
public:
    // Constructor – pass HardwareSerial references
    GSM_OTA(HardwareSerial& gsmSerial, HardwareSerial& debugSerial);

    // Initialise GSM UART (call in setup())
    // void begin(uint32_t baud, int8_t rxPin, int8_t txPin);

    // Configuration
    void setAPN(const char* apn);
    void setChunkSize(uint16_t size);          // default 512
    void setCmdTimeout(uint32_t ms);           // default 10000
    void setHttpTimeout(uint32_t ms);          // default 60000
    void setDebugEnabled(bool enable);         // default true

    // Callbacks (optional)
    void onProgress(OTAProgressCallback cb);   // called every chunk
    void onLog(OTALogCallback cb);             // called for log messages

    // ── Main API ──────────────────────────────────────────────────────────────
    /*
     * performOTA(url)
     *   Downloads firmware from 'url' and flashes it to the next OTA partition.
     *   Does NOT reboot — caller decides when to call esp_restart().
     *
     *   Returns OTA_SUCCESS (0) on success, negative OTAResult on failure.
     *
     *   Example:
     *     OTAResult r = ota.performOTA("https://raw.githubusercontent.com/…/fw.bin");
     *     if (r == OTA_SUCCESS) { Serial.println("Done!"); esp_restart(); }
     */
    OTAResult performOTA(const char* url);

    // Human-readable error string
    static const char* resultToString(OTAResult r);

private:
    HardwareSerial& _gsm;
    HardwareSerial& _dbg;

    char     _apn[64];
    uint16_t _chunkSize;
    uint32_t _cmdTimeout;
    uint32_t _httpTimeout;
    bool     _debugEnabled;

    OTAProgressCallback _progressCb;
    OTALogCallback      _logCb;

    // Internal helpers
    void     _log(const char* fmt, ...);
    void     _gsmFlush();
    String   _gsmReadUntil(const char* token, uint32_t timeout_ms);
    bool     _sendAT(const char* cmd,
                     const char* expected   = "OK",
                     uint32_t    timeout_ms = 0);   // 0 = use _cmdTimeout
    int32_t  _parseContentLength(const String& s);
    int32_t  _readChunkRaw(uint8_t* buf, int32_t ask);
};
