/*
 * ESP32 WebSocket Server — AP Mode
 * ================================
 * Libraries required:
 *   - WiFi.h          (built-in with ESP32 Arduino core)
 *   - AsyncTCP        https://github.com/me-no-dev/AsyncTCP
 *   - ESPAsyncWebServer https://github.com/me-no-dev/ESPAsyncWebServer
 *
 * PlatformIO lib_deps:
 *   me-no-dev/AsyncTCP @ ^1.1.1
 *   me-no-dev/ESP Async WebServer @ ^1.2.3
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ─────────────────────────────────────────────
//  Access Point credentials
// ─────────────────────────────────────────────
const char* AP_SSID     = "ESP32-WebSocket-Test";
const char* AP_PASSWORD = "12345678";

// ─────────────────────────────────────────────
//  Server instances
// ─────────────────────────────────────────────
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");

// ─────────────────────────────────────────────
//  Broadcast timer
// ─────────────────────────────────────────────
unsigned long lastBroadcast = 0;
const unsigned long BROADCAST_INTERVAL_MS = 5000;
uint32_t broadcastCount = 0;

// ─────────────────────────────────────────────
//  WebSocket event handler
// ─────────────────────────────────────────────
void onWsEvent(AsyncWebSocket*       server,
               AsyncWebSocketClient* client,
               AwsEventType          type,
               void*                 arg,
               uint8_t*              data,
               size_t                len)
{
  switch (type) {

    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connected  —  IP: %s\n",
                    client->id(),
                    client->remoteIP().toString().c_str());
      client->text("{\"event\":\"connected\",\"msg\":\"Hello from ESP32!\"}");
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u disconnected\n", client->id());
      break;

    case WS_EVT_ERROR:
      Serial.printf("[WS] Client #%u error(%u): %s\n",
                    client->id(),
                    *((uint16_t*)arg),
                    (char*)data);
      break;

    case WS_EVT_PONG:
      Serial.printf("[WS] Client #%u pong\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;

      // Only handle complete, single-frame text messages for simplicity.
      // For large / fragmented messages extend this with a buffer.
      if (info->final && info->index == 0 && info->len == len
          && info->opcode == WS_TEXT)
      {
        // NULL-terminate the payload
        String msg((char*)data, len);

        Serial.printf("[WS] Client #%u sent: %s\n", client->id(), msg.c_str());

        // Echo back as JSON
        String echo = "{\"event\":\"echo\",\"msg\":" + msg + "}";

        // If the payload already looks like JSON pass it through,
        // otherwise wrap in a string literal.
        if (msg[0] != '{' && msg[0] != '[') {
          echo = "{\"event\":\"echo\",\"msg\":\"" + msg + "\"}";
        }

        client->text(echo);
        Serial.printf("[WS] Echoed to client #%u: %s\n",
                      client->id(), echo.c_str());
      }
      break;
    }

    default:
      break;
  }
}

// ─────────────────────────────────────────────
//  HTTP root handler (simple health-check page)
// ─────────────────────────────────────────────
void onHttpRoot(AsyncWebServerRequest* request)
{
  request->send(200, "text/html",
    "<html><body style='font-family:monospace;padding:2rem'>"
    "<h2>ESP32 WebSocket Server</h2>"
    "<p>WebSocket endpoint: <b>ws://192.168.4.1/ws</b></p>"
    "<p>Open your local <code>index.html</code> after connecting to "
    "<b>ESP32-WebSocket-Test</b>.</p>"
    "</body></html>");
}

// ─────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║   ESP32  WebSocket  AP  Demo          ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // ── Start Access Point ──────────────────────
  Serial.printf("[WIFI] Starting AP  SSID: %s\n", AP_SSID);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (!apOk) {
    Serial.println("[WIFI] ERROR: softAP() failed!");
    while (true) { delay(1000); }
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WIFI] AP started  —  IP: %s\n\n", apIP.toString().c_str());

  // ── Register WebSocket handler ───────────────
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ── HTTP routes ──────────────────────────────
  server.on("/", HTTP_GET, onHttpRoot);

  // 404 catch-all
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });

  // ── Start HTTP server ────────────────────────
  server.begin();
  Serial.println("[HTTP] Server started on port 80");
  Serial.println("[WS]  WebSocket endpoint: ws://192.168.4.1/ws");
  Serial.println("\n[READY] Waiting for connections...\n");
}

// ─────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────
void loop()
{
  // Clean up disconnected clients periodically
  ws.cleanupClients();

  // Broadcast a status message every BROADCAST_INTERVAL_MS
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL_MS) {
    lastBroadcast = now;
    broadcastCount++;

    uint32_t connectedClients = ws.count();

    // Build JSON broadcast payload
    String payload = "{\"event\":\"status\","
                     "\"uptime\":" + String(now / 1000) + ","
                     "\"clients\":" + String(connectedClients) + ","
                     "\"broadcast\":" + String(broadcastCount) + "}";

    if (connectedClients > 0) {
      ws.textAll(payload);
      Serial.printf("[BROADCAST #%u] %s\n",
                    broadcastCount, payload.c_str());
    } else {
      Serial.printf("[BROADCAST #%u] (no clients connected)\n",
                    broadcastCount);
    }
  }
}