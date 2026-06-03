/**
 * SerialTester.ino
 * ═══════════════════════════════════════════════════════════════════════════════
 * Interactive Serial Monitor test tool for the A7670C library.
 *
 * Open Serial Monitor at 115200 baud, "Both NL & CR" line endings.
 * A numbered menu lets you test every feature one at a time.
 *
 * Wiring:
 *   ESP32 GPIO16 (RX1)  →  A7670C TXD
 *   ESP32 GPIO17 (TX1)  →  A7670C RXD
 *   ESP32 GPIO4         →  A7670C PWRKEY
 *   GND                 →  GND (common)
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include "A7670C/A7670C.h"

// ── Pin config ────────────────────────────────────────────────────────────────
#define GSM_RX_PIN   16
#define GSM_TX_PIN   17
#define GSM_PWR_PIN   2

// ── Defaults (edit these or override at runtime via the menu) ─────────────────
String CFG_APN         = "airtelgprs.com";      // Your carrier APN
String CFG_SMS_NUMBER  = "+919876543210";        // Number for SMS/call tests
String CFG_HTTP_URL    = "http://httpbin.org/get";
String CFG_POST_URL    = "http://httpbin.org/post";
String CFG_POST_BODY   = "{\"device\":\"ESP32\",\"test\":true}";
String CFG_MQTT_BROKER = "broker.hivemq.com";
uint16_t CFG_MQTT_PORT = 1883;
String CFG_MQTT_ID     = "ESP32_A7670C_Tester";
String CFG_MQTT_USER   = "";
String CFG_MQTT_PASS   = "";
String CFG_MQTT_PUB    = "a7670c/test/pub";
String CFG_MQTT_SUB    = "a7670c/test/sub";
String CFG_MQTT_MSG    = "{\"hello\":\"world\"}";

// ── Module instance ───────────────────────────────────────────────────────────
A7670C gsm(Serial2, GSM_RX_PIN, GSM_TX_PIN, GSM_PWR_PIN);

// ═══════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void printSep(char c = '─', int n = 60) {
    for (int i = 0; i < n; i++) Serial.print(c);
    Serial.println();
}

void printOK(bool ok) {
    Serial.println(ok ? "  ✔  PASS" : "  ✘  FAIL");
}

void printResult(A7670C_Result r) {
    switch (r) {
        case A7670C_Result::OK:         Serial.println("  ✔  OK");         break;
        case A7670C_Result::ERROR:      Serial.println("  ✘  ERROR");      break;
        case A7670C_Result::TIMEOUT:    Serial.println("  ✘  TIMEOUT");    break;
        case A7670C_Result::HTTP_ERROR: Serial.println("  ✘  HTTP_ERROR"); break;
        case A7670C_Result::MQTT_ERROR: Serial.println("  ✘  MQTT_ERROR"); break;
        default:                        Serial.println("  ?  UNKNOWN");    break;
    }
}

// Read a full line from Serial (blocks until Enter pressed)
String serialReadLine(const String& prompt = "") {
    if (prompt.length()) {
        Serial.print(prompt);
        Serial.print(" > ");
    }
    String s = "";
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') break;
            if (c == '\r') continue;
            s += c;
            Serial.print(c);   // local echo
        }
        gsm.loop();   // keep URCs alive while waiting
    }
    Serial.println();
    return s;
}

// Prompt to override a config string; press Enter to keep current value
void promptCfg(const String& label, String& value) {
    Serial.print("  " + label + " [" + value + "]: ");
    String input = "";
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') break;
            if (c == '\r') continue;
            input += c;
            Serial.print(c);
        }
        gsm.loop();
    }
    Serial.println();
    if (input.length() > 0) value = input;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  CALLBACKS (show output in serial monitor)
// ═══════════════════════════════════════════════════════════════════════════════

void onSmsReceived(SmsMessage msg) {
    printSep('★');
    Serial.println("  📩  INCOMING SMS");
    Serial.println("  From      : " + msg.sender);
    Serial.println("  Timestamp : " + msg.timestamp);
    Serial.println("  Status    : " + msg.status);
    Serial.println("  Text      : " + msg.text);
    printSep('★');
}

void onMqttMessage(const String& topic, const String& payload) {
    printSep('★');
    Serial.println("  📨  MQTT MESSAGE RECEIVED");
    Serial.println("  Topic   : " + topic);
    Serial.println("  Payload : " + payload);
    printSep('★');
}

void onIncomingCall(const String& number) {
    printSep('★');
    Serial.println("  📞  INCOMING CALL");
    Serial.println("  From : " + (number.isEmpty() ? "Unknown / withheld" : number));
    Serial.println();
    Serial.println("  Type  A  → Answer");
    Serial.println("  Type  H  → Reject / Hang up");
    printSep('★');
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MENU FUNCTIONS  (one per test)
// ═══════════════════════════════════════════════════════════════════════════════

// ── 0. Main menu ──────────────────────────────────────────────────────────────
void printMenu() {
    printSep('═');
    Serial.println("  A7670C SERIAL TESTER  –  Main Menu");
    printSep('═');
    Serial.println("  [1]  Module status & info");
    Serial.println("  [2]  Signal strength (RSSI / CSQ)");
    Serial.println("  [3]  Supply voltage");
    Serial.println("  [4]  Enable internet (PDP context ON)");
    Serial.println("  [5]  Disable internet (PDP context OFF)");
    Serial.println("  [6]  HTTP GET");
    Serial.println("  [7]  HTTP POST");
    Serial.println("  [8]  MQTT configure + connect");
    Serial.println("  [9]  MQTT publish");
    Serial.println("  [10] MQTT subscribe");
    Serial.println("  [11] MQTT disconnect");
    Serial.println("  [12] Send SMS");
    Serial.println("  [13] Read SMS (by index)");
    Serial.println("  [14] List all SMS");
    Serial.println("  [15] Delete SMS");
    Serial.println("  [16] Dial voice call");
    Serial.println("  [17] Answer call  (use when ringing)");
    Serial.println("  [18] Hang up call");
    Serial.println("  [19] Raw AT command");
    Serial.println("  [20] Power OFF module");
    Serial.println("  [21] Reset module");
    Serial.println("  [22] Edit config (APN / numbers / URLs)");
    printSep();
    Serial.print("  Enter number: ");
}

// ── 1. Module status ──────────────────────────────────────────────────────────
void testModuleInfo() {
    printSep();
    Serial.println("  [1] MODULE STATUS & INFO");
    printSep();
    bool alive = gsm.isAlive();
    Serial.print("  Alive       : "); printOK(alive);
    if (!alive) { Serial.println("  Module not responding!"); return; }

    Serial.println("  IMEI        : " + gsm.getIMEI());
    Serial.println("  ICCID       : " + gsm.getICCID());
    Serial.println("  Operator    : " + gsm.getOperator());
    Serial.println("  Firmware    : " + gsm.getFirmwareVersion());

    RegStatus rs = gsm.getNetworkStatus();
    String rsStr;
    switch(rs) {
        case RegStatus::NOT_REGISTERED:   rsStr = "Not registered";   break;
        case RegStatus::REGISTERED_HOME:  rsStr = "Home network ✔";  break;
        case RegStatus::SEARCHING:        rsStr = "Searching...";     break;
        case RegStatus::DENIED:           rsStr = "Registration denied"; break;
        case RegStatus::ROAMING:          rsStr = "Roaming ✔";       break;
        default:                          rsStr = "Unknown";          break;
    }
    Serial.println("  Network     : " + rsStr);
    Serial.println("  Internet    : " + String(gsm.isInternetEnabled() ? "ON ✔" : "OFF"));
}

// ── 2. Signal strength ────────────────────────────────────────────────────────
void testSignal() {
    printSep();
    Serial.println("  [2] SIGNAL STRENGTH");
    printSep();
    SignalInfo s = gsm.getSignalStrength();
    Serial.printf("  CSQ   : %d  (0–31; 99=no signal)\r\n", s.csq);
    Serial.printf("  RSSI  : %d dBm\r\n", s.rssi);
    Serial.printf("  BER   : %d  (0–7; 99=unknown)\r\n", s.ber);

    // Quality bar
    Serial.print("  Level : [");
    int bars = (s.csq == 99) ? 0 : map(s.csq, 0, 31, 0, 20);
    for (int i = 0; i < 20; i++) Serial.print(i < bars ? "█" : "░");
    Serial.print("] ");
    if      (s.csq == 99)   Serial.println("No signal");
    else if (s.csq <= 5)    Serial.println("Poor");
    else if (s.csq <= 10)   Serial.println("Fair");
    else if (s.csq <= 20)   Serial.println("Good");
    else                    Serial.println("Excellent");
}

// ── 4. Enable internet ────────────────────────────────────────────────────────
void testEnableInternet() {
    printSep();
    Serial.println("  [4] ENABLE INTERNET");
    printSep();
    promptCfg("APN", CFG_APN);
    Serial.println("  Activating PDP context for APN: " + CFG_APN + "...");
    A7670C_Result r = gsm.enableInternet(CFG_APN);
    printResult(r);
    if (r == A7670C_Result::OK)
        Serial.println("  isInternetEnabled(): " + String(gsm.isInternetEnabled() ? "true ✔" : "false"));
}

// ── 5. Disable internet ───────────────────────────────────────────────────────
void testDisableInternet() {
    printSep();
    Serial.println("  [5] DISABLE INTERNET");
    printSep();
    A7670C_Result r = gsm.disableInternet();
    printResult(r);
}

// ── 6. HTTP GET ───────────────────────────────────────────────────────────────
void testHttpGet() {
    printSep();
    Serial.println("  [6] HTTP GET");
    printSep();
    promptCfg("URL", CFG_HTTP_URL);
    Serial.println("  Sending GET request...");
    HttpResponse resp = gsm.httpGet(CFG_HTTP_URL);
    Serial.printf("  Status Code : %d\r\n", resp.statusCode);
    Serial.print("  Success     : "); printOK(resp.success);
    Serial.println("  ── Body (first 500 chars) ──────────────────────");
    Serial.println(resp.body.substring(0, 500));
    if (resp.body.length() > 500)
        Serial.printf("  ... (%d chars total)\r\n", resp.body.length());
}

// ── 7. HTTP POST ──────────────────────────────────────────────────────────────
void testHttpPost() {
    printSep();
    Serial.println("  [7] HTTP POST");
    printSep();
    promptCfg("POST URL", CFG_POST_URL);
    promptCfg("Body JSON", CFG_POST_BODY);
    Serial.println("  Sending POST request...");
    HttpResponse resp = gsm.httpPost(CFG_POST_URL, CFG_POST_BODY);
    Serial.printf("  Status Code : %d\r\n", resp.statusCode);
    Serial.print("  Success     : "); printOK(resp.success);
    Serial.println("  ── Body (first 500 chars) ──────────────────────");
    Serial.println(resp.body.substring(0, 500));
}

// ── 8. MQTT configure + connect ───────────────────────────────────────────────
void testMqttConnect() {
    printSep();
    Serial.println("  [8] MQTT CONFIGURE + CONNECT");
    printSep();
    promptCfg("Broker host", CFG_MQTT_BROKER);
    String portStr = String(CFG_MQTT_PORT);
    promptCfg("Port", portStr);
    CFG_MQTT_PORT = portStr.toInt();
    promptCfg("Client ID", CFG_MQTT_ID);
    promptCfg("Username (blank=anon)", CFG_MQTT_USER);
    if (CFG_MQTT_USER.length())
        promptCfg("Password", CFG_MQTT_PASS);

    Serial.println("  Configuring...");
    A7670C_Result r = gsm.mqttConfigure(
        CFG_MQTT_ID, CFG_MQTT_BROKER, CFG_MQTT_PORT,
        CFG_MQTT_USER, CFG_MQTT_PASS);
    Serial.print("  Configure: "); printResult(r);
    if (r != A7670C_Result::OK) return;

    Serial.println("  Connecting...");
    r = gsm.mqttConnect();
    Serial.print("  Connect  : "); printResult(r);
    Serial.println("  Connected: " + String(gsm.mqttIsConnected() ? "YES ✔" : "NO"));
}

// ── 9. MQTT publish ───────────────────────────────────────────────────────────
void testMqttPublish() {
    printSep();
    Serial.println("  [9] MQTT PUBLISH");
    printSep();
    if (!gsm.mqttIsConnected()) {
        Serial.println("  ⚠  MQTT not connected — run [8] first.");
        return;
    }
    promptCfg("Topic", CFG_MQTT_PUB);
    promptCfg("Payload", CFG_MQTT_MSG);
    Serial.println("  Publishing...");
    A7670C_Result r = gsm.mqttPublish(CFG_MQTT_PUB, CFG_MQTT_MSG, 0, false);
    printResult(r);
}

// ── 10. MQTT subscribe ────────────────────────────────────────────────────────
void testMqttSubscribe() {
    printSep();
    Serial.println("  [10] MQTT SUBSCRIBE");
    printSep();
    if (!gsm.mqttIsConnected()) {
        Serial.println("  ⚠  MQTT not connected — run [8] first.");
        return;
    }
    promptCfg("Topic to subscribe", CFG_MQTT_SUB);
    Serial.println("  Subscribing...");
    A7670C_Result r = gsm.mqttSubscribe(CFG_MQTT_SUB, 0);
    printResult(r);
    if (r == A7670C_Result::OK)
        Serial.println("  Waiting for messages on topic: " + CFG_MQTT_SUB);
    Serial.println("  (Incoming messages will print automatically — keep loop() running)");
}

// ── 11. MQTT disconnect ───────────────────────────────────────────────────────
void testMqttDisconnect() {
    printSep();
    Serial.println("  [11] MQTT DISCONNECT");
    printSep();
    A7670C_Result r = gsm.mqttDisconnect();
    printResult(r);
    Serial.println("  Connected: " + String(gsm.mqttIsConnected() ? "YES" : "NO ✔"));
}

// ── 12. Send SMS ──────────────────────────────────────────────────────────────
void testSmsSend() {
    printSep();
    Serial.println("  [12] SEND SMS");
    printSep();
    promptCfg("Destination number", CFG_SMS_NUMBER);
    String text = "A7670C test SMS from ESP32!";
    promptCfg("Message text", text);
    Serial.println("  Sending...");
    A7670C_Result r = gsm.smsSend(CFG_SMS_NUMBER, text);
    printResult(r);
}

// ── 13. Read one SMS ──────────────────────────────────────────────────────────
void testSmsRead() {
    printSep();
    Serial.println("  [13] READ SMS BY INDEX");
    printSep();
    String idxStr = "1";
    promptCfg("SMS index (1-based)", idxStr);
    int idx = idxStr.toInt();
    Serial.println("  Reading index " + String(idx) + "...");
    SmsMessage msg = gsm.smsRead(idx);
    if (msg.sender.isEmpty()) {
        Serial.println("  No message at that index (or read failed).");
        return;
    }
    Serial.println("  Index     : " + msg.index);
    Serial.println("  Status    : " + msg.status);
    Serial.println("  From      : " + msg.sender);
    Serial.println("  Timestamp : " + msg.timestamp);
    Serial.println("  Text      : " + msg.text);
}

// ── 14. List all SMS ──────────────────────────────────────────────────────────
void testSmsList() {
    printSep();
    Serial.println("  [14] LIST ALL SMS");
    printSep();
    SmsMessage msgs[20];
    Serial.println("  Reading inbox...");
    int n = gsm.smsList(msgs, 20, "ALL");
    Serial.printf("  Found %d message(s)\r\n", n);
    printSep('-', 60);
    for (int i = 0; i < n; i++) {
        Serial.printf("  [%s] %s | %s\r\n",
            msgs[i].index.c_str(),
            msgs[i].sender.c_str(),
            msgs[i].text.substring(0, 40).c_str());
    }
    if (n == 0) Serial.println("  Inbox is empty.");
}

// ── 15. Delete SMS ────────────────────────────────────────────────────────────
void testSmsDelete() {
    printSep();
    Serial.println("  [15] DELETE SMS");
    printSep();
    Serial.println("  Flag meanings:");
    Serial.println("  0 = Delete by index  |  1 = All read  |  4 = All messages");
    String idxStr  = "1";
    String flagStr = "0";
    promptCfg("Index", idxStr);
    promptCfg("Flag (0/1/4)", flagStr);
    Serial.println("  Deleting...");
    A7670C_Result r = gsm.smsDelete(idxStr.toInt(), flagStr.toInt());
    printResult(r);
}

// ── 16. Dial voice call ───────────────────────────────────────────────────────
void testCallDial() {
    printSep();
    Serial.println("  [16] DIAL VOICE CALL");
    printSep();
    promptCfg("Number to dial", CFG_SMS_NUMBER);
    Serial.println("  Dialling " + CFG_SMS_NUMBER + "...");
    A7670C_Result r = gsm.callDial(CFG_SMS_NUMBER);
    printResult(r);
    if (r == A7670C_Result::OK) {
        Serial.println("  Call is ringing. Use [18] to hang up.");
        Serial.println("  Active: " + String(gsm.callIsActive() ? "YES ✔" : "NO"));
    }
}

// ── 17. Answer call ───────────────────────────────────────────────────────────
void testCallAnswer() {
    printSep();
    Serial.println("  [17] ANSWER CALL");
    printSep();
    A7670C_Result r = gsm.callAnswer();
    printResult(r);
}

// ── 18. Hang up ───────────────────────────────────────────────────────────────
void testCallHangUp() {
    printSep();
    Serial.println("  [18] HANG UP");
    printSep();
    A7670C_Result r = gsm.callHangUp();
    printResult(r);
}

// ── 19. Raw AT command ────────────────────────────────────────────────────────
void testRawAT() {
    printSep();
    Serial.println("  [19] RAW AT COMMAND");
    printSep();
    Serial.println("  Type any AT command (e.g. AT+CPSI?) and press Enter.");
    Serial.println("  Leave expected-response blank to wait for 'OK'.");
    String cmd      = serialReadLine("  Command");
    String expected = "OK";
    String expInput = serialReadLine("  Expected token (Enter=OK)");
    if (expInput.length()) expected = expInput;

    Serial.println("  Sending: " + cmd);
    printSep('-');
    String resp = gsm.sendAT(cmd, expected, 10000);
    Serial.println(resp);
    printSep('-');
    Serial.print("  Match '" + expected + "': "); printOK(gsm.lastOK());
}

// ── 20. Power off ─────────────────────────────────────────────────────────────
void testPowerOff() {
    printSep();
    Serial.println("  [20] POWER OFF MODULE");
    printSep();
    Serial.println("  Sending AT+CPOWD=1...");
    bool ok = gsm.powerOff();
    printOK(ok);
}

// ── 21. Reset ────────────────────────────────────────────────────────────────
void testReset() {
    printSep();
    Serial.println("  [21] RESET MODULE (PWRKEY)");
    printSep();
    Serial.println("  Resetting — this takes ~15 s...");
    bool ok = gsm.reset();
    printOK(ok);
}

// ── 22. Edit config ───────────────────────────────────────────────────────────
void editConfig() {
    printSep();
    Serial.println("  [22] EDIT CONFIG (press Enter to keep current)");
    printSep();
    promptCfg("APN", CFG_APN);
    promptCfg("SMS/Call number", CFG_SMS_NUMBER);
    promptCfg("HTTP GET URL", CFG_HTTP_URL);
    promptCfg("HTTP POST URL", CFG_POST_URL);
    promptCfg("HTTP POST body", CFG_POST_BODY);
    promptCfg("MQTT broker", CFG_MQTT_BROKER);
    String portStr = String(CFG_MQTT_PORT);
    promptCfg("MQTT port", portStr);
    CFG_MQTT_PORT = portStr.toInt();
    promptCfg("MQTT client ID", CFG_MQTT_ID);
    promptCfg("MQTT username", CFG_MQTT_USER);
    promptCfg("MQTT password", CFG_MQTT_PASS);
    promptCfg("MQTT pub topic", CFG_MQTT_PUB);
    promptCfg("MQTT sub topic", CFG_MQTT_SUB);
    promptCfg("MQTT message", CFG_MQTT_MSG);
    Serial.println("  ✔  Config updated.");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Register callbacks — they fire automatically from gsm.loop()
    gsm.onSms(onSmsReceived);
    gsm.onMqttMessage(onMqttMessage);
    gsm.onRing(onIncomingCall);

    // Enable AT traffic debug output
    gsm.setDebug(Serial);
    gsm.setDebugEnabled(true);   // ← set false to hide raw AT traffic

    printSep('═');
    Serial.println("  A7670C SERIAL TESTER");
    Serial.println("  ESP32 + A7670C @ 115200 baud");
    Serial.println("  Set line ending to  'Both NL & CR'  in Serial Monitor");
    printSep('═');
    Serial.println("  Initialising module...");

    if (!gsm.begin()) {
        Serial.println("  ✘  Module did not respond. Check power & wiring.");
        Serial.println("  Continuing anyway — use [21] Reset or [20→21] power cycle.");
    } else {
        Serial.println("  ✔  Module ready.");
    }

    printMenu();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void loop() {
    // Must be called every iteration to catch URCs (SMS, MQTT, RING)
    gsm.loop();

    // Read menu choice from Serial Monitor
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) { printMenu(); return; }

        int choice = input.toInt();
        Serial.println(input);  // echo

        switch (choice) {
            case 1:  testModuleInfo();      break;
            case 2:  testSignal();          break;
            case 3:  gsm.powerOn();         break;
            case 4:  testEnableInternet();  break;
            case 5:  testDisableInternet(); break;
            case 6:  testHttpGet();         break;
            case 7:  testHttpPost();        break;
            case 8:  testMqttConnect();     break;
            case 9:  testMqttPublish();     break;
            case 10: testMqttSubscribe();   break;
            case 11: testMqttDisconnect();  break;
            case 12: testSmsSend();         break;
            case 13: testSmsRead();         break;
            case 14: testSmsList();         break;
            case 15: testSmsDelete();       break;
            case 16: testCallDial();        break;
            case 17: testCallAnswer();      break;
            case 18: testCallHangUp();      break;
            case 19: testRawAT();           break;
            case 20: testPowerOff();        break;
            case 21: testReset();           break;
            case 22: editConfig();          break;
            default:
                Serial.println("  ⚠  Unknown option: " + input);
                break;
        }

        // Print menu again after each test
        Serial.println();
        printMenu();
    }
}
