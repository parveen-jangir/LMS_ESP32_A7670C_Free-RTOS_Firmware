/*
 * ESP32 OTA Update via A7670C GSM Module
 *
 * - Type "update" in Serial Monitor to trigger OTA
 * - Anything else is forwarded to the GSM module (passthrough)
 *
 * Wiring (adjust pins as needed):
 *   ESP32 TX2 (GPIO17) --> A7670C RX
 *   ESP32 RX2 (GPIO16) --> A7670C TX
 *   GND <--> GND
 */

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ─── CONFIG ──────────────────────────────────────────────────────────────────
#define GSM_SERIAL   Serial2
#define GSM_BAUD     115200
#define GSM_RX_PIN   16
#define GSM_TX_PIN   17
#define DBG_SERIAL   Serial
#define DBG_BAUD     115200

const char* APN    = "airtelgprs.com";
const char* FW_URL = "https://raw.githubusercontent.com/parveen-jangir/mqtt_bin_file/main/test_esp_ota.bin";

#define CMD_TIMEOUT   10000   // ms – normal AT commands
#define HTTP_TIMEOUT  60000   // ms – wait for +HTTPACTION URC
#define READ_CHUNK    512     // bytes per HTTPREAD request
// ─────────────────────────────────────────────────────────────────────────────

// ─── LOW-LEVEL GSM HELPERS ───────────────────────────────────────────────────

/* Drain anything sitting in the HW UART buffer. */
static void gsmFlush()
{
    delay(20);
    while (GSM_SERIAL.available()) GSM_SERIAL.read();
}

/* Read bytes from GSM until 'token' is seen OR timeout expires.
   Returns the accumulated string. */
static String gsmReadUntil(const char* token, uint32_t timeout_ms = CMD_TIMEOUT)
{
    String buf;
    buf.reserve(256);
    uint32_t t = millis();
    while (millis() - t < timeout_ms) {
        while (GSM_SERIAL.available()) {
            buf += (char)GSM_SERIAL.read();
            t = millis(); // reset idle timer on every byte
        }
        if (buf.indexOf(token) != -1) break;
    }
    return buf;
}

/* Send AT command, wait for 'expected'. Returns true on match. */
static bool sendAT(const char* cmd,
                   const char* expected  = "OK",
                   uint32_t    timeout_ms = CMD_TIMEOUT)
{
    gsmFlush();
    GSM_SERIAL.println(cmd);
    DBG_SERIAL.print("[CMD] "); DBG_SERIAL.println(cmd);
    String resp = gsmReadUntil(expected, timeout_ms);
    DBG_SERIAL.print("[RSP] "); DBG_SERIAL.println(resp);
    return resp.indexOf(expected) != -1;
}

/* Parse the content-length from  "+HTTPACTION: 0,200,275648" */
static int32_t parseContentLength(const String& s)
{
    int idx = s.indexOf("+HTTPACTION:");
    if (idx == -1) return -1;
    int end = s.indexOf('\n', idx);
    String line = s.substring(idx, end);
    int lc = line.lastIndexOf(',');
    if (lc == -1) return -1;
    return line.substring(lc + 1).toInt();
}

// ─── BINARY CHUNK READER ─────────────────────────────────────────────────────
/*
 * A7670C HTTPREAD response frame looks like this (ALL bytes):
 *
 *   "+HTTPREAD: <len>\r\n"   <-- ASCII header  (variable length)
 *   <len raw binary bytes>   <-- exact firmware data  ← we want ONLY this
 *   "\r\nOK\r\n"             <-- 6 trailing ASCII bytes we must discard
 *
 * Strategy:
 *   1. Send AT+HTTPREAD=0,<ask>
 *   2. Read bytes until we see "+HTTPREAD:" followed by a '\n'  → header done
 *   3. Parse <len> from that header (module may give less than we asked)
 *   4. Read exactly <len> raw bytes into buf[]  → pure firmware data
 *   5. Read and discard the trailing "\r\nOK\r\n" (6 bytes)
 */
static int32_t readChunkRaw(uint8_t* buf, int32_t ask)
{
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d", (int)ask);
    gsmFlush();
    GSM_SERIAL.println(cmd);
    DBG_SERIAL.print("[CMD] "); DBG_SERIAL.println(cmd);

    // ── Step 1: consume bytes until end of "+HTTPREAD: <len>\r\n" ────────────
    String header;
    header.reserve(40);
    uint32_t t = millis();
    bool headerDone = false;
    while (millis() - t < CMD_TIMEOUT) {
        if (GSM_SERIAL.available()) {
            char c = (char)GSM_SERIAL.read();
            header += c;
            t = millis();
            // Header ends at the '\n' AFTER "+HTTPREAD:"
            if (header.indexOf("+HTTPREAD:") != -1 && c == '\n') {
                headerDone = true;
                break;
            }
            // ERROR response?
            if (header.indexOf("ERROR") != -1) {
                DBG_SERIAL.println("[ERR] HTTPREAD error: " + header);
                return -1;
            }
        }
    }
    if (!headerDone) {
        DBG_SERIAL.println("[ERR] HTTPREAD header timeout. Got: " + header);
        return -1;
    }

    // Parse actual chunk length reported by module
    int ci = header.lastIndexOf(':');
    int32_t chunkLen = header.substring(ci + 1).toInt();
    if (chunkLen <= 0) {
        DBG_SERIAL.println("[ERR] Bad chunk length in: " + header);
        return -1;
    }

    // ── Step 2: read exactly chunkLen raw binary bytes ────────────────────────
    int32_t got = 0;
    t = millis();
    while (got < chunkLen) {
        if (millis() - t > CMD_TIMEOUT) {
            DBG_SERIAL.printf("[ERR] Binary read timeout at %d/%d\n", got, chunkLen);
            return -1;
        }
        if (GSM_SERIAL.available()) {
            buf[got++] = (uint8_t)GSM_SERIAL.read();
            t = millis();
        }
    }

    // ── Step 3: discard trailing "\r\nOK\r\n" (6 bytes) ─────────────────────
    // Read up to 10 bytes to be safe (handles "\r\nOK\r\n" or just "OK\r\n")
    uint8_t trail[10];
    int trailGot = 0;
    t = millis();
    while (trailGot < 6 && millis() - t < 2000) {
        if (GSM_SERIAL.available()) {
            trail[trailGot++] = GSM_SERIAL.read();
            t = millis();
        }
    }

    return chunkLen;   // actual bytes written into buf[]
}

// ─── OTA MAIN ────────────────────────────────────────────────────────────────

static void doOTA()
{
    DBG_SERIAL.println("\n===== OTA UPDATE STARTED =====");

    // 1. APN
    {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
        if (!sendAT(cmd)) { DBG_SERIAL.println("[OTA] APN config failed"); return; }
    }

    // 2. HTTPINIT (ignore error if already inited)
    sendAT("AT+HTTPINIT");

    // 3. NETOPEN
    if (!sendAT("AT+NETOPEN", "+NETOPEN: 0", 15000)) {
        DBG_SERIAL.println("[OTA] NETOPEN warning – may already be open, continuing...");
    }

    // 4. SSL
    sendAT("AT+HTTPPARA=\"SSLCFG\",0");
    sendAT("AT+CSSLCFG=\"sslversion\",0,4");

    // 5. URL
    {
        char cmd[220];
        snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", FW_URL);
        if (!sendAT(cmd)) { DBG_SERIAL.println("[OTA] URL set failed"); return; }
    }

    // 6. GET
    DBG_SERIAL.println("[OTA] Sending HTTP GET …");
    gsmFlush();
    GSM_SERIAL.println("AT+HTTPACTION=0");
    DBG_SERIAL.println("[CMD] AT+HTTPACTION=0");

    String actionResp = gsmReadUntil("+HTTPACTION:", HTTP_TIMEOUT);
    // read a little more to get the rest of the line
    actionResp += gsmReadUntil("\n", 2000);
    DBG_SERIAL.println("[RSP] " + actionResp);

    if (actionResp.indexOf(",200,") == -1) {
        DBG_SERIAL.println("[OTA] HTTP GET failed");
        return;
    }

    int32_t totalBytes = parseContentLength(actionResp);
    if (totalBytes <= 0) {
        DBG_SERIAL.println("[OTA] Could not parse firmware size");
        return;
    }
    DBG_SERIAL.printf("[OTA] Firmware size: %d bytes\n", totalBytes);

    // 7. Begin OTA
    const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
    if (!part) { DBG_SERIAL.println("[OTA] No OTA partition!"); return; }
    DBG_SERIAL.printf("[OTA] Target partition: %s @ 0x%x\n",
                      part->label, part->address);

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, (size_t)totalBytes, &ota_handle);
    if (err != ESP_OK) {
        DBG_SERIAL.printf("[OTA] esp_ota_begin: %s\n", esp_err_to_name(err));
        return;
    }

    // 8. Stream firmware chunks
    static uint8_t chunkBuf[READ_CHUNK];   // static → lives in DRAM, not stack
    int32_t bytesWritten = 0;

    while (bytesWritten < totalBytes) {
        int32_t ask = min((int32_t)READ_CHUNK, totalBytes - bytesWritten);

        int32_t got = readChunkRaw(chunkBuf, ask);
        if (got <= 0) {
            DBG_SERIAL.println("[OTA] Chunk read failed – aborting");
            esp_ota_abort(ota_handle);
            return;
        }

        err = esp_ota_write(ota_handle, chunkBuf, (size_t)got);
        if (err != ESP_OK) {
            DBG_SERIAL.printf("[OTA] esp_ota_write: %s\n", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            return;
        }

        bytesWritten += got;
        int pct = (int)((bytesWritten * 100LL) / totalBytes);
        DBG_SERIAL.printf("[OTA] %d%%  (%d / %d)\n", pct, bytesWritten, totalBytes);
    }

    // 9. Validate + commit
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        DBG_SERIAL.printf("[OTA] esp_ota_end: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        DBG_SERIAL.printf("[OTA] set_boot_partition: %s\n", esp_err_to_name(err));
        return;
    }

    // 10. Cleanup
    sendAT("AT+HTTPTERM");
    sendAT("AT+NETCLOSE");

    DBG_SERIAL.println("[OTA] ✓ SUCCESS – rebooting in 3 s …");
    delay(3000);
    esp_restart();
}

// ─── SERIAL PASSTHROUGH ──────────────────────────────────────────────────────

static void passthroughLoop()
{
    if (DBG_SERIAL.available()) {
        String in = DBG_SERIAL.readStringUntil('\n');
        in.trim();
        if (in.equalsIgnoreCase("update")) {
            doOTA();
        } else {
            GSM_SERIAL.println(in);
        }
    }
    while (GSM_SERIAL.available()) {
        DBG_SERIAL.write(GSM_SERIAL.read());
    }
}

// ─── SETUP / LOOP ────────────────────────────────────────────────────────────

void setup()
{
    DBG_SERIAL.begin(DBG_BAUD);
    GSM_SERIAL.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
    delay(1000);
    DBG_SERIAL.println("================================");
    DBG_SERIAL.println(" ESP32 GSM-OTA Bridge Ready");
    DBG_SERIAL.println(" Type 'update' to flash OTA");
    DBG_SERIAL.println(" Anything else → forwarded to GSM");
    DBG_SERIAL.println("================================");
}

void loop()
{
    passthroughLoop();
}
