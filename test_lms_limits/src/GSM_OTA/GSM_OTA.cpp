#include "GSM_OTA.h"
#include <stdarg.h>

// ─── Constructor ──────────────────────────────────────────────────────────────
GSM_OTA::GSM_OTA(HardwareSerial& gsmSerial, HardwareSerial& debugSerial, DataLogger& dataLogger)
    : _gsm(gsmSerial),
      _dbg(debugSerial),
      _dataLogger(dataLogger),
      _chunkSize(GSM_OTA_CHUNK_SIZE),
      _cmdTimeout(GSM_OTA_CMD_TIMEOUT_MS),
      _httpTimeout(GSM_OTA_HTTP_TIMEOUT_MS),
      _debugEnabled(true),
      _progressCb(nullptr),
      _logCb(nullptr)
{
    strncpy(_apn, GSM_OTA_DEFAULT_APN, sizeof(_apn) - 1);
}

// ─── Public config ────────────────────────────────────────────────────────────
void GSM_OTA::begin(uint32_t baud, int8_t rxPin, int8_t txPin)
{
    _gsm.begin(baud, SERIAL_8N1, rxPin, txPin);
    delay(500);
}

void GSM_OTA::setAPN(const char* apn)            { strncpy(_apn, apn, sizeof(_apn)-1); }
void GSM_OTA::setChunkSize(uint16_t size)         { _chunkSize = size; }
void GSM_OTA::setCmdTimeout(uint32_t ms)          { _cmdTimeout = ms; }
void GSM_OTA::setHttpTimeout(uint32_t ms)         { _httpTimeout = ms; }
void GSM_OTA::setDebugEnabled(bool enable)        { _debugEnabled = enable; }
void GSM_OTA::onProgress(OTAProgressCallback cb)  { _progressCb = cb; }
void GSM_OTA::onLog(OTALogCallback cb)            { _logCb = cb; }

// ─── Error strings ────────────────────────────────────────────────────────────
const char* GSM_OTA::resultToString(OTAResult r)
{
    switch (r) {
        case OTA_SUCCESS:          return "Success";
        case OTA_ERR_APN:          return "APN config failed";
        case OTA_ERR_NETOPEN:      return "NETOPEN failed";
        case OTA_ERR_URL:          return "URL set failed";
        case OTA_ERR_HTTP_GET:     return "HTTP GET no response";
        case OTA_ERR_HTTP_CODE:    return "HTTP non-200 response (redirect? use direct URL)";
        case OTA_ERR_NO_PARTITION: return "No OTA partition found";
        case OTA_ERR_OTA_BEGIN:    return "esp_ota_begin failed";
        case OTA_ERR_CHUNK_READ:   return "Chunk read failed";
        case OTA_ERR_OTA_WRITE:    return "esp_ota_write failed";
        case OTA_ERR_OTA_END:      return "esp_ota_end / validation failed";
        case OTA_ERR_SET_BOOT:     return "esp_ota_set_boot_partition failed";
        default:                   return "Unknown error";
    }
}

// ─── Internal: logging ────────────────────────────────────────────────────────
void GSM_OTA::_log(const char* fmt, ...)
{
    if (!_debugEnabled && !_logCb) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (_debugEnabled) {
        _dbg.print("[GSM_OTA] ");
        _dbg.println(buf);
    }
    if (_logCb) _logCb(buf);
}

// ─── Internal: GSM helpers ────────────────────────────────────────────────────
void GSM_OTA::_gsmFlush()
{
    delay(20);
    while (_gsm.available()) _gsm.read();
}

String GSM_OTA::_gsmReadUntil(const char* token, uint32_t timeout_ms)
{
    String buf;
    buf.reserve(256);
    uint32_t t = millis();
    while (millis() - t < timeout_ms) {
        while (_gsm.available()) {
            buf += (char)_gsm.read();
            t = millis();
        }
        if (buf.indexOf(token) != -1) break;
        else delay(10);
    }
    return buf;
}

bool GSM_OTA::_sendAT(const char* cmd, const char* expected, uint32_t timeout_ms)
{
    if (timeout_ms == 0) timeout_ms = _cmdTimeout;
    _gsmFlush();
    _gsm.println(cmd);
    _log("AT >> %s", cmd);
    String resp = _gsmReadUntil(expected, timeout_ms);
    // also wait a bit for ERROR in case 'expected' arrived before ERROR
    if (resp.indexOf(expected) == -1) {
        _log("AT << (no match) %s", resp.c_str());
        return false;
    }
    _log("AT << %s", resp.c_str());
    return true;
}

int32_t GSM_OTA::_parseContentLength(const String& s)
{
    int idx = s.indexOf("+HTTPACTION:");
    if (idx == -1) return -1;
    int end  = s.indexOf('\n', idx);
    String line = s.substring(idx, end);
    int lc = line.lastIndexOf(',');
    if (lc == -1) return -1;
    return line.substring(lc + 1).toInt();
}

/*
 * _readChunkRaw — read one HTTPREAD frame cleanly.
 *
 * A7670C frame format:
 *   "+HTTPREAD: <len>\r\n"   ASCII header
 *   <len bytes>              raw binary  ← we want only this
 *   "\r\nOK\r\n"             6-byte trailer  ← discard
 */
int32_t GSM_OTA::_readChunkRaw(uint8_t* buf, int32_t ask)
{
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d", (int)ask);
    _gsmFlush();
    _gsm.println(cmd);

    // ── Phase 1: header ───────────────────────────────────────────────────────
    String header;
    header.reserve(40);
    uint32_t t = millis();
    bool headerDone = false;

    while (millis() - t < _cmdTimeout) {
        if (_gsm.available()) {
            char c = (char)_gsm.read();
            header += c;
            t = millis();
            if (header.indexOf("+HTTPREAD:") != -1 && c == '\n') {
                headerDone = true;
                break;
            }
            if (header.indexOf("ERROR") != -1) {
                _log("HTTPREAD error: %s", header.c_str());
                return -1;
            }
        }
    }
    if (!headerDone) {
        _log("HTTPREAD header timeout. buf=%s", header.c_str());
        return -1;
    }

    int ci = header.lastIndexOf(':');
    int32_t chunkLen = header.substring(ci + 1).toInt();
    if (chunkLen <= 0) {
        _log("Bad chunk length: %s", header.c_str());
        return -1;
    }

    // ── Phase 2: binary data ──────────────────────────────────────────────────
    int32_t got = 0;
    t = millis();
    while (got < chunkLen) {
        if (millis() - t > _cmdTimeout) {
            _log("Binary read timeout at %d/%d", got, chunkLen);
            return -1;
        }
        if (_gsm.available()) {
            buf[got++] = (uint8_t)_gsm.read();
            t = millis();
        }
    }

    // ── Phase 3: discard trailer "\r\nOK\r\n" ────────────────────────────────
    int trailGot = 0;
    t = millis();
    while (trailGot < GSM_OTA_TRAIL_BYTES && millis() - t < 2000) {
        if (_gsm.available()) {
            _gsm.read();
            trailGot++;
            t = millis();
        }
    }

    return chunkLen;
}

// ─── performOTA ──────────────────────────────────────────────────────────────
OTAResult GSM_OTA::performOTA(const char* url)
{
    _log("===== OTA START =====");
    _log("URL: %s", url);

    // 1. APN
    {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", _apn);
        if (!_sendAT(cmd)) return OTA_ERR_APN;
    }

    // 2. HTTP init (ignore if already inited)
    if (!_sendAT("AT+HTTPINIT"))
        _log("HTTPINIT warning – may already be open");

    // 3. Network open
    if (!_sendAT("AT+NETOPEN", "+NETOPEN: 0", 15000))
        _log("NETOPEN warning – may already be open, continuing");

    // 4. SSL
    _sendAT("AT+HTTPPARA=\"SSLCFG\",0");
    _sendAT("AT+CSSLCFG=\"sslversion\",0,4");

    // 5. URL
    {
        char cmd[300];
        snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
        if (!_sendAT(cmd)) return OTA_ERR_URL;
    }

    // 6. HTTP GET
    _log("Sending HTTP GET...");
    _gsmFlush();
    _gsm.println("AT+HTTPACTION=0");
    _log("AT >> AT+HTTPACTION=0");

    String actionResp = _gsmReadUntil("+HTTPACTION:", _httpTimeout);
    actionResp += _gsmReadUntil("\n", 2000);
    _log("AT << %s", actionResp.c_str());

    if (actionResp.indexOf("+HTTPACTION:") == -1) return OTA_ERR_HTTP_GET;
    if (actionResp.indexOf(",200,") == -1) {
        _log("Non-200 HTTP response. Use a direct URL (no redirects).");
        return OTA_ERR_HTTP_CODE;
    }

    int32_t totalBytes = _parseContentLength(actionResp);
    if (totalBytes <= 0) {
        _log("Could not parse content length");
        return OTA_ERR_HTTP_GET;
    }
    _log("Firmware size: %d bytes", totalBytes);

    // 7. OTA partition
    const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        _log("No OTA partition found – check partition scheme");
        return OTA_ERR_NO_PARTITION;
    }
    _log("Target partition: %s @ 0x%x", part->label, part->address);

    esp_ota_handle_t otaHandle = 0;
    esp_err_t err = esp_ota_begin(part, (size_t)totalBytes, &otaHandle);
    if (err != ESP_OK) {
        _log("esp_ota_begin: %s", esp_err_to_name(err));
        return OTA_ERR_OTA_BEGIN;
    }

    // 8. Stream chunks
    uint8_t* chunkBuf = (uint8_t*)malloc(_chunkSize);
    if (!chunkBuf) {
        _log("malloc failed for chunk buffer");
        esp_ota_abort(otaHandle);
        return OTA_ERR_CHUNK_READ;
    }

    int32_t bytesWritten = 0;
    while (bytesWritten < totalBytes) {
        int32_t ask = min((int32_t)_chunkSize, totalBytes - bytesWritten);
        int32_t got = _readChunkRaw(chunkBuf, ask);

        if (got <= 0) {
            free(chunkBuf);
            esp_ota_abort(otaHandle);
            return OTA_ERR_CHUNK_READ;
        }

        err = esp_ota_write(otaHandle, chunkBuf, (size_t)got);
        if (err != ESP_OK) {
            _log("esp_ota_write: %s", esp_err_to_name(err));
            free(chunkBuf);
            esp_ota_abort(otaHandle);
            return OTA_ERR_OTA_WRITE;
        }

        bytesWritten += got;
        int pct = (int)((bytesWritten * 100LL) / totalBytes);
        _log("Progress: %d%%  (%d / %d)", pct, bytesWritten, totalBytes);

        if (_progressCb) _progressCb(pct, bytesWritten, totalBytes);
    }

    free(chunkBuf);

    // 9. Validate
    err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        _log("esp_ota_end (validation): %s", esp_err_to_name(err));
        return OTA_ERR_OTA_END;
    }

    // 10. Set boot partition
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        _log("esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return OTA_ERR_SET_BOOT;
    }

    // 11. Cleanup
    _sendAT("AT+HTTPTERM");
    _sendAT("AT+NETCLOSE");

    _log("===== OTA SUCCESS =====");
    return OTA_SUCCESS;
}
