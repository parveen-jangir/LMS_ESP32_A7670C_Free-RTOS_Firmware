# LMS Field Setup — MQTT JSON Command Reference

**Device:** LMS Field Setup (ESP32)  
**Interface:** MQTT over WebSocket  
**Broker IP:** `76.13.243.127`  
**MQTT Port:** `1883`  
**WebSocket Port:** `9001`  
**Username:** `lms_mqtt_broker`  
**Password:** `landslidemonitoringsystem`  
**Publish Topic (Browser → Broker):** `lms/commands`  
**Subscribe Topic (Broker → Browser):** `lms/data`  
**LoRa Frequency:** `433 MHz`  
**GSM Module:** `SIM A7670C`

---

## Overview

All communication between the browser UI and the ESP32 passes through an MQTT broker over a persistent WebSocket connection. Every message in both directions is a JSON object that uses a **`type`** field — both for commands sent by the browser and responses sent by the ESP32.

- **Browser → Broker (`lms/commands`):** `{ "type": "...", ... }`
- **Broker → Browser (`lms/data`):** `{ "type": "...", "status": "ok", ... }`

> **Key design rules:** Commands no longer use a `cmd` field — both sides use `type`. Timestamps are not included in any message. The ESP32 auto-sends a full `sensor_broadcast` whenever rain is reset, movement count is reset, or the MPU is calibrated. Any response that carries a `msg` field displays it as a 2-second floating pop-up in the UI and then disappears.

---

## Connection Lifecycle

```
Browser connects to ws://76.13.243.127:9001
    ↓  authenticates with username + password
onconnect → subscribes to lms/data
          → auto-fires: sensor_broadcast, battery_status,
                        solar_status, system_power,
                        gsm_info, system_info
    ↓
ondisconnect → auto-reconnects after 5 seconds
```

---

## `msg` Field — Floating Pop-up Behaviour

Any response from the ESP32 may include an optional `msg` string field. When present, the UI displays it as a **floating pop-up notification** that automatically fades out after **2 seconds**. It does not persist on screen.

```json
{ "type": "sensor_state", "sensor": "BMP180", "state": false, "status": "ok", "msg": "BMP180 disabled" }
```

The pop-up appears for: sensor toggle confirmations, save acknowledgements, error messages, and any other event where the ESP32 wants to surface a one-line status to the user.

---

## Testing with MQTTX

| Action | MQTTX setting |
|--------|--------------|
| See commands the browser sends | Subscribe to `lms/commands` |
| Simulate ESP32 responses | Publish JSON to `lms/data` |

---

## Command Index

| # | Command `type` | Triggered by | Response `type` |
|---|----------------|--------------|-----------------|
| 1 | `sensor_broadcast` | Read All button / Auto | `sensor_broadcast` |
| 2 | `sensor_state` | Toggle switch | `sensor_state` |
| 3 | `rain_reset` | Reset Counter button | `sensor_broadcast` (full) |
| 4 | `mpu_reset` | Reset Movement Count button | `sensor_broadcast` (full) |
| 5 | `mpu_calibrate` | Calibrate button | `sensor_broadcast` (full, with `calibrated`) |
| 6 | `battery_status` | Battery button / Auto | `battery_status` |
| 7 | `solar_status` | Solar button / Auto | `solar_status` |
| 8 | `system_power` | System button / Auto | `system_power` |
| 9 | `system_info` | On connect / LoRa tab | `system_info` |
| 10 | `save_tid` | TID input save button | `save_tid` |
| 11 | `gsm_info` | Refresh button | `gsm_info` |
| 12 | `gsm_at` | AT terminal / APN | `gsm_response` |
| 13 | `gsm_test` | Test buttons | `gsm_test` |
| 14 | `gsm_reset` | Reset button | `gsm_info` |
| 15 | `lora_trigger` | Trigger Hooter button | `lora_result` |

---

## 1. `sensor_broadcast` — Read All Sensors

Browser sends an empty request; ESP32 responds with a full snapshot of every sensor.

### Command
```json
{ "type": "sensor_broadcast" }
```

### Response — `sensor_broadcast`
```json
{
  "type": "sensor_broadcast",
  "status": "ok",
  "bmp180": {
    "temperature": 23.5,
    "pressure": 895.45,
    "valid": true,
    "state": true
  },
  "bh1750": {
    "illuminance": 15.83333,
    "valid": true,
    "state": true
  },
  "mpu6050": {
    "acceleration": {
      "x": -0.986748,
      "y": -3.070415,
      "z": -11.04823
    },
    "gyroscope": {
      "x": 0.480916,
      "y": -4.08397,
      "z": -3.091603
    },
    "temperature": -5788160,
    "movement_count": 0,
    "valid": true,
    "state": true
  },
  "dht22": {
    "temperature": 23.7,
    "humidity": 56.6,
    "valid": true,
    "state": true
  },
  "soil_moisture": {
    "raw": 3091,
    "percentage": 75.48229,
    "valid": true,
    "state": true
  },
  "rain_gauge": {
    "tip_count": 0,
    "rainfall_mm": 0,
    "last_tip_time": 0,
    "valid": true,
    "state": true
  }
}
```

### Field Reference — Common per Sensor

| Field | Type | Description |
|-------|------|-------------|
| `state` | bool | `true` = sensor enabled; `false` = disabled |
| `valid` | bool | `true` = reading is good; `false` = sensor error |

### Badge Logic

| `state` | `valid` | Badge |
|---------|---------|-------|
| `false` | any | **OFF** (grey) |
| `true` | `true` | **OK** (green) |
| `true` | `false` | **ERR** (red) |

### Sensor-Specific Fields

| Sensor key | Field | Unit | Description |
|------------|-------|------|-------------|
| `dht22` | `temperature` | °C | Ambient temperature |
| `dht22` | `humidity` | % | Relative humidity |
| `bh1750` | `illuminance` | lux | Light intensity |
| `bmp180` | `temperature` | °C | Temperature from barometric sensor |
| `bmp180` | `pressure` | hPa | Atmospheric pressure |
| `soil_moisture` | `raw` | ADC count | Raw ADC value |
| `soil_moisture` | `percentage` | % | Moisture percentage |
| `mpu6050` | `acceleration.x/y/z` | m/s² | Linear acceleration per axis |
| `mpu6050` | `gyroscope.x/y/z` | °/s | Angular velocity per axis |
| `mpu6050` | `temperature` | raw int | Internal MPU temperature register |
| `mpu6050` | `movement_count` | count | Cumulative movement event counter |
| `rain_gauge` | `tip_count` | count | Tipping bucket tips since last reset |
| `rain_gauge` | `rainfall_mm` | mm | Total rainfall since last reset |
| `rain_gauge` | `last_tip_time` | ms | millis() of most recent tip |

> Each sensor key is optional. Missing keys are silently skipped by the UI.

---

## 2. `sensor_state` — Toggle Sensor On / Off

Enables or disables a specific sensor. The ESP32 acknowledges immediately with a `sensor_state` response (which may carry a `msg` pop-up), then sends a full `sensor_broadcast` automatically.

### Command
```json
{
  "type": "sensor_state",
  "sensor": "BMP180",
  "state": true
}
```

| Field | Values | Description |
|-------|--------|-------------|
| `sensor` | `BMP180`, `BH1750`, `DHT22`, `MPU6050`, `SoilMoisture`, `RainGauge` | Sensor to toggle |
| `state` | `true` / `false` | `true` = enable, `false` = disable |

### Response — `sensor_state`

**When enabling:**
```json
{
  "type": "sensor_state",
  "sensor": "BMP180",
  "state": true
}
```

**When disabling:**
```json
{
  "type": "sensor_state",
  "sensor": "BMP180",
  "state": false,
  "status": "ok",
  "msg": "BMP180 disabled"
}
```

> `msg` triggers a 2-second floating pop-up in the UI. The toggle badge updates immediately to WAIT (enabled) or OFF (disabled). A full `sensor_broadcast` follows automatically.

---

## 3. `rain_reset` — Reset Rain Gauge Counter

Resets `tip_count`, `rainfall_mm`, and `last_tip_time` to zero. The ESP32 responds with a **complete `sensor_broadcast`** so all sensor cards update in one message.

### Command
```json
{ "type": "rain_reset" }
```

### Response — `sensor_broadcast` (full)

Same structure as command 1, with `rain_gauge` zeroed:

```json
{
  "type": "sensor_broadcast",
  "status": "ok",
  "rain_gauge": {
    "tip_count": 0,
    "rainfall_mm": 0,
    "last_tip_time": 0,
    "valid": true,
    "state": true
  },
  "bmp180": { ... },
  "bh1750": { ... },
  "mpu6050": { ... },
  "dht22": { ... },
  "soil_moisture": { ... }
}
```

---

## 4. `mpu_reset` — Reset MPU6050 Movement Count

Resets `movement_count` to `0` without affecting readings or calibration offsets. The ESP32 responds with a **complete `sensor_broadcast`**.

### Command
```json
{ "type": "mpu_reset" }
```

### Response — `sensor_broadcast` (full)

Same structure as command 1, with `mpu6050.movement_count` set to `0`:

```json
{
  "type": "sensor_broadcast",
  "status": "ok",
  "mpu6050": {
    "acceleration": { "x": -0.986748, "y": -3.070415, "z": -11.04823 },
    "gyroscope":    { "x": 0.480916,  "y": -4.08397,  "z": -3.091603 },
    "temperature": -5788160,
    "movement_count": 0,
    "valid": true,
    "state": true
  },
  "bmp180": { ... },
  "bh1750": { ... },
  "dht22": { ... },
  "soil_moisture": { ... },
  "rain_gauge": { ... }
}
```

---

## 5. `mpu_calibrate` — Calibrate MPU6050

Triggers the offset calibration routine. The device must be **level and completely still** during calibration. The ESP32 computes zero-point offsets, applies them, then responds with a **complete `sensor_broadcast`** that includes `mpu6050.calibrated`.

### Command
```json
{ "type": "mpu_calibrate" }
```

### Response — `sensor_broadcast` (full, with `calibrated`)

**On success:**
```json
{
  "type": "sensor_broadcast",
  "status": "ok",
  "mpu6050": {
    "acceleration": { "x": 0.00, "y": 0.00, "z": -9.81 },
    "gyroscope":    { "x": 0.00, "y": 0.00, "z": 0.00 },
    "temperature": -5788160,
    "movement_count": 17,
    "calibrated": true,
    "valid": true,
    "state": true
  },
  "bmp180": { ... },
  "bh1750": { ... },
  "dht22": { ... },
  "soil_moisture": { ... },
  "rain_gauge": { ... }
}
```

**On failure:**
```json
{
  "type": "sensor_broadcast",
  "status": "ok",
  "mpu6050": {
    "calibrated": false,
    "valid": false,
    "state": true
  }
}
```

### `calibrated` field behaviour in the UI

| `calibrated` | UI result |
|--------------|-----------|
| `true` | Blue box: "Calibration done. Offsets applied." |
| `false` | Red box: "Calibration failed — check sensor connection." |
| absent | Calibration box unchanged (normal sensor poll) |

> `movement_count` is preserved across calibration — only offsets are recalculated.

---

## 6. `battery_status` — Read Battery

### Command
```json
{ "type": "battery_status" }
```

### Response — `battery_status`
```json
{
  "type": "battery_status",
  "charging": false,
  "voltage": 3.7,
  "current": 0.5,
  "power_mw": 1.85,
  "energy_wh": 3.210,
  "percent": 71,
  "status": "ok"
}
```

### Field Reference

| Field | Unit | Description |
|-------|------|-------------|
| `charging` | bool | `true` if actively charging |
| `voltage` | V | Battery terminal voltage |
| `current` | A | Charge/discharge current |
| `power_mw` | mW | Instantaneous power |
| `energy_wh` | Wh | Cumulative energy discharged |
| `percent` | % | State of charge |

### Battery Bar Colour

| Level | Colour |
|-------|--------|
| `< 20%` | Red |
| `20–49%` | Yellow |
| `≥ 50%` | Green |

---

## 7. `solar_status` — Read Solar Panel

### Command
```json
{ "type": "solar_status" }
```

### Response — `solar_status`
```json
{
  "type": "solar_status",
  "voltage": 5.0,
  "current": 0.2,
  "power_mw": 1.0,
  "energy_wh": 3.210,
  "status": "ok"
}
```

### Field Reference

| Field | Unit | Description |
|-------|------|-------------|
| `voltage` | V | Panel output voltage |
| `current` | A | Panel output current |
| `power_mw` | mW | Instantaneous panel power |
| `energy_wh` | Wh | Cumulative energy harvested |

---

## 8. `system_power` — Read System Power Source

### Command
```json
{ "type": "system_power" }
```

### Response — `system_power`
```json
{
  "type": "system_power",
  "source": "battery",
  "voltage": 3.7,
  "current": 0.5,
  "power_mw": 1.85,
  "energy_wh": 3.210,
  "status": "ok"
}
```

### Field Reference

| Field | Unit | Description |
|-------|------|-------------|
| `source` | string | Active power source: `"battery"`, `"solar"`, `"hybrid"`, `"idle"` |
| `voltage` | V | System supply voltage |
| `current` | A | System supply current |
| `power_mw` | mW | Total system power draw |
| `energy_wh` | Wh | Cumulative energy consumed |

---

## 9. `system_info` — ESP32 Runtime Info

Returns uptime, memory, device identity (TID), and MAC address. The TID is a short user-assigned identifier string stored in the ESP32's non-volatile storage. On first boot it defaults to `""` until saved via `save_tid`.

### Command
```json
{ "type": "system_info" }
```

### Response — `system_info`
```json
{
  "type": "system_info",
  "uptime_s": 21,
  "free_heap": 316348,
  "tid": "t108",
  "mac": "00:11:22:33:44:55",
  "status": "ok"
}
```

### Field Reference

| Field | Unit | Display | Description |
|-------|------|---------|-------------|
| `uptime_s` | seconds | `0h 0m 21s` | Time since last ESP32 boot |
| `free_heap` | bytes | `308.9 KB` | Free heap memory (divided by 1024) |
| `tid` | string | — | User-assigned terminal/tracker ID (e.g. `"t108"`) |
| `mac` | string | — | ESP32 Wi-Fi MAC address (read-only hardware value) |

> `mac` is a fixed hardware value. `tid` is editable via the `save_tid` command.

---

## 10. `save_tid` — Save Terminal ID

Saves a user-assigned TID string to the ESP32's non-volatile storage (NVS/EEPROM). The TID can be any short string (e.g. `"t108"`, `"site-A"`) used to identify this specific unit in the field. The new TID takes effect immediately and persists across reboots.

### Command
```json
{
  "type": "save_tid",
  "tid": "t108"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tid` | string | New terminal ID to store on the device |

### Response — `save_tid`
```json
{
  "type": "save_tid",
  "tid": "t108",
  "status": "ok",
  "msg": "TID saved: t108"
}
```

> `msg` triggers a 2-second floating pop-up confirming the save. The System Info panel updates the displayed TID immediately.

---

## 11. `gsm_info` — Get GSM / SIM Status

### Command
```json
{ "type": "gsm_info" }
```

### Response — `gsm_info`
```json
{
  "type": "gsm_info",
  "registered": true,
  "sim_status": "READY",
  "operator": "Airtel",
  "sim_number": "+919876543210",
  "ip": "100.74.12.55",
  "voltage": 4.12,
  "rssi": 18
}
```

### RSSI Signal Bar Thresholds

| Bars lit | Min RSSI |
|----------|---------|
| 1 | 2 |
| 2 | 8 |
| 3 | 14 |
| 4 | 20 |
| 5 | 26 |

---

## 12. `gsm_at` — Send Raw AT Command

### Command
```json
{ "type": "gsm_at", "command": "AT+CSQ" }
```

### APN Configuration Example
```json
{ "type": "gsm_at", "command": "AT+CGDCONT=1,\"IP\",\"airtelgprs.com\"" }
```

### Response — `gsm_response`
```json
{ "type": "gsm_response", "response": "+CSQ: 18,0\r\nOK" }
```

### Quick Reference AT Commands

| Button | Command | Purpose |
|--------|---------|---------|
| AT | `AT` | Basic ping |
| Signal | `AT+CSQ` | Signal quality |
| SIM No | `AT+CNUM` | SIM phone number |
| Operator | `AT+COPS?` | Current operator |
| Voltage | `AT+CBC` | Supply voltage |
| Reg | `AT+CREG?` | Network registration |
| APN | `AT+CGDCONT?` | Current APN |
| IMSI | `AT+CIMI` | SIM IMSI |
| Balance | `AT+CUSD=1,"*121#"` | SIM balance (USSD) |

---

## 13. `gsm_test` — Run GSM Diagnostic Test

### Command
```json
{ "type": "gsm_test", "test": "sms" }
```

| `test` value | Description |
|-------------|-------------|
| `"sms"` | Send a test SMS |
| `"call"` | Place a test voice call |
| `"api"` | Test HTTP API connectivity |
| `"ota"` | Trigger OTA firmware update |

### Response — `gsm_test`
```json
{ "type": "gsm_test", "success": true, "result": "SMS sent to +919876543210" }
```

---

## 14. `gsm_reset` — Reset GSM Module

### Command
```json
{ "type": "gsm_reset" }
```

### Response — `gsm_info`
```json
{
  "type": "gsm_info",
  "registered": false,
  "sim_status": "INITIALIZING",
  "operator": "",
  "sim_number": "",
  "ip": "",
  "voltage": 0.0,
  "rssi": 0
}
```

---

## 15. `lora_trigger` — Trigger LoRa Hooter

### Command
```json
{ "type": "lora_trigger", "value": 1 }
```

### Response — `lora_result`
```json
{ "type": "lora_result", "sent": true }
```

| `sent` | UI result |
|--------|-----------|
| `true` | Green: "Packet sent. Hooter activates within 2s." Badge: **OK** |
| `false` | Red: "Send failed — check LoRa wiring." Badge: **ERR** |

---

## Auto-Polling Mode

```
Auto ON  → every 5 seconds publishes:
             sensor_broadcast + battery_status + solar_status + system_power
Auto OFF → clears the interval
```

GSM data is never auto-polled.

---

## sensor_broadcast Trigger Summary

The ESP32 automatically sends a complete `sensor_broadcast` in response to:

| Command | Reason |
|---------|--------|
| `sensor_broadcast` | Direct poll request |
| `rain_reset` | Rain counter was zeroed |
| `mpu_reset` | Movement count was zeroed |
| `mpu_calibrate` | Calibration completed (includes `mpu6050.calibrated`) |

---

## Static Connection Information

| Property | Value |
|----------|-------|
| Broker IP | `76.13.243.127` |
| MQTT Port | `1883` |
| WebSocket Port | `9001` |
| MQTT Username | `lms_mqtt_broker` |
| Publish Topic | `lms/commands` |
| Subscribe Topic | `lms/data` |
| LoRa Frequency | `433 MHz` |
| GSM Module | `SIM A7670C` |

---

## Error Handling Notes

- If the MQTT connection closes, the UI reconnects automatically after 5 seconds.
- JSON parse errors on `lms/data` messages are silently ignored.
- Publishing while disconnected silently drops the message.
- Missing sensor keys in `sensor_broadcast` are silently skipped.
- `mpu6050.calibrated` is only present in `mpu_calibrate` responses; its absence during normal polling never resets the calibration status box.
- `msg` fields are always optional. Their absence never causes an error.