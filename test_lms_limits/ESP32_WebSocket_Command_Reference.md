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

All communication between the browser UI and the ESP32 passes through an **MQTT broker** over a persistent WebSocket connection. Every message in both directions is a JSON object.

- **Browser → Broker (`lms/commands`):** Command objects with a `cmd` field
- **Broker → Browser (`lms/data`):** Response objects with a `type` field

The ESP32 subscribes to `lms/commands` and publishes to `lms/data`. The browser does the reverse.

---

## Connection Lifecycle

```
Browser connects to ws://76.13.243.127:9001
    ↓  authenticates with username + password
onconnect → subscribes to lms/data
          → auto-fires: sensor_read, power_read, gsm_info, system_info
    ↓
ondisconnect → auto-reconnects after 5 seconds
```

---

## Testing with MQTTX

| Action | MQTTX setting |
|--------|--------------|
| See commands the browser sends | Subscribe to `lms/commands` |
| Simulate ESP32 responses | Publish JSON to `lms/data` |

**Example: simulate a sensor reading in MQTTX**
```
Topic:   lms/data
Payload: {"type":"sensor_data","mpu6050":{"enabled":true,"valid":true,"ax":0.01,"ay":-0.02,"az":1.0,"gx":0.1,"gy":0.0,"gz":0.0,"movement_count":42}}
```

---

## Command Index

| # | Command | `cmd` value | Response `type` |
|---|---------|-------------|-----------------|
| 1 | Read All Sensors | `sensor_read` | `sensor_data` |
| 2 | Read Power | `power_read` | `power_data` |
| 3 | Get GSM Info | `gsm_info` | `gsm_info` |
| 4 | Toggle Sensor | `sensor_toggle` | `sensor_data` |
| 5 | Reset Rain Counter | `rain_reset` | `sensor_data` |
| 6 | **Reset Movement Count** | **`mpu_reset`** | **`sensor_data`** |
| 7 | **Calibrate MPU6050** | **`mpu_calibrate`** | **`sensor_data`** |
| 8 | Send AT Command | `gsm_at` | `gsm_response` |
| 9 | Run GSM Test | `gsm_test` | `gsm_test` |
| 10 | Reset GSM Module | `gsm_reset` | `gsm_info` |
| 11 | Trigger LoRa Hooter | `lora_trigger` | `lora_result` |
| 12 | Get System Info | `system_info` | `system_info` |

---

## 1. `sensor_read` — Read All Sensors

Requests a full snapshot of all sensor readings.

### Command
```json
{
  "cmd": "sensor_read"
}
```

### Response — `sensor_data`
```json
{
  "type": "sensor_data",
  "dht22": {
    "enabled": true,
    "valid": true,
    "temperature": 28.4,
    "humidity": 65.2
  },
  "bh1750": {
    "enabled": true,
    "valid": true,
    "illuminance": 1024.5
  },
  "bmp180": {
    "enabled": true,
    "valid": true,
    "temperature": 27.9,
    "pressure": 1013.2
  },
  "soil": {
    "enabled": true,
    "valid": true,
    "percent": 42.0,
    "raw": 1850
  },
  "mpu6050": {
    "enabled": true,
    "valid": true,
    "ax": 0.01,
    "ay": -0.02,
    "az": 1.00,
    "gx": 0.12,
    "gy": -0.05,
    "gz": 0.03,
    "movement_count": 17
  },
  "rain": {
    "enabled": true,
    "valid": true,
    "tips": 5,
    "mm": 1.27
  }
}
```

### Field Reference

| Sensor | Field | Unit | Description |
|--------|-------|------|-------------|
| `dht22` | `temperature` | °C | Ambient temperature |
| `dht22` | `humidity` | % | Relative humidity |
| `bh1750` | `illuminance` | lux | Light intensity |
| `bmp180` | `temperature` | °C | Temperature (barometric sensor) |
| `bmp180` | `pressure` | hPa | Atmospheric pressure |
| `soil` | `percent` | % | Soil moisture percentage |
| `soil` | `raw` | ADC count | Raw ADC value |
| `mpu6050` | `ax`, `ay`, `az` | g | Accelerometer (X, Y, Z) |
| `mpu6050` | `gx`, `gy`, `gz` | °/s | Gyroscope (X, Y, Z) |
| `mpu6050` | `movement_count` | count | Cumulative movement event counter |
| `rain` | `tips` | count | Tipping bucket tip count |
| `rain` | `mm` | mm | Total rainfall in millimetres |

### Badge Logic

| `enabled` | `valid` | Badge shown |
|-----------|---------|-------------|
| `false` | any | **OFF** (grey) |
| `true` | `true` | **OK** (green) |
| `true` | `false` | **ERR** (red) |

> Each sensor key is optional in the response. Missing keys are silently ignored by the UI.

---

## 2. `power_read` — Read Power Monitor

Requests solar panel, battery, and system power data.

### Command
```json
{
  "cmd": "power_read"
}
```

### Response — `power_data`
```json
{
  "type": "power_data",
  "solar": {
    "voltage": 18.42,
    "current": 320.5,
    "power": 5896,
    "energy_wh": 12.345
  },
  "battery": {
    "voltage": 3.85,
    "current": 210.0,
    "power": 808,
    "energy_wh": 5.678,
    "percent": 71,
    "charge_time_min": 45
  },
  "system": {
    "state": "HYBRID",
    "load_power": 1200,
    "energy_wh": 3.210
  }
}
```

### Field Reference

| Section | Field | Unit | Notes |
|---------|-------|------|-------|
| `solar` | `voltage` | V | Panel output voltage |
| `solar` | `current` | mA | Panel output current |
| `solar` | `power` | mW | Panel power (displayed as W, divided by 1000) |
| `solar` | `energy_wh` | Wh | Cumulative energy harvested |
| `battery` | `voltage` | V | Battery terminal voltage |
| `battery` | `current` | mA | Charge/discharge current |
| `battery` | `power` | mW | Battery power (displayed as W, divided by 1000) |
| `battery` | `energy_wh` | Wh | Cumulative energy discharged |
| `battery` | `percent` | % | Charge level — **optional** (see note below) |
| `battery` | `charge_time_min` | min | Estimated time to full — **optional** |
| `system` | `state` | enum | Power source state (see table below) |
| `system` | `load_power` | mW | Total system load (displayed as W, divided by 1000) |
| `system` | `energy_wh` | Wh | Cumulative energy consumed |

### Battery Percent Fallback

If `battery.percent` is not provided, it is estimated from voltage:

```
percent = ((voltage - 3.0) / 1.2) × 100
  3.0V → 0%
  4.2V → 100%
```

### Battery Bar Colour

| Level | Colour |
|-------|--------|
| `< 20%` | Red (low) |
| `20% – 49%` | Yellow (mid) |
| `≥ 50%` | Green |

### System State Values

| `state` value | Display label |
|---------------|---------------|
| `SOLAR_ONLY` | Solar Only |
| `BATTERY_DISCHARGING` | Battery |
| `HYBRID` | Solar + Battery |
| `IDLE` | Idle |

---

## 3. `gsm_info` — Get GSM / SIM Status

Requests current GSM module and SIM card status.

### Command
```json
{
  "cmd": "gsm_info"
}
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

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `registered` | bool | `true` → network registered (badge: **OK**); `false` → no network (badge: **NO NET**) |
| `sim_status` | string | SIM card status string (e.g. `READY`, `NOT INSERTED`) |
| `operator` | string | Network operator name |
| `sim_number` | string | SIM phone number |
| `ip` | string | Device IP address assigned by network |
| `voltage` | float (V) | GSM module supply voltage |
| `rssi` | int | Signal strength indicator (0–31) |

### RSSI Signal Bar Thresholds

| Bars lit | RSSI threshold |
|----------|---------------|
| 1 bar | ≥ 2 |
| 2 bars | ≥ 8 |
| 3 bars | ≥ 14 |
| 4 bars | ≥ 20 |
| 5 bars | ≥ 26 |

---

## 4. `sensor_toggle` — Enable / Disable a Sensor

Enables or disables a specific sensor on the ESP32.

### Command
```json
{
  "cmd": "sensor_toggle",
  "sensor": "DHT22",
  "state": "on"
}
```

### Parameters

| Field | Values | Description |
|-------|--------|-------------|
| `sensor` | `DHT22`, `BH1750`, `BMP180`, `SoilMoisture`, `MPU6050`, `RainGauge` | Sensor to toggle |
| `state` | `"on"` / `"off"` | Target state |

### Response — `sensor_data`

Same format as the `sensor_read` response. ESP32 returns a fresh sensor snapshot after applying the toggle.

```json
{
  "type": "sensor_data",
  "dht22": {
    "enabled": false,
    "valid": false
  }
}
```

---

## 5. `rain_reset` — Reset Rain Gauge Counter

Resets the tipping bucket counter and rainfall accumulation to zero.

### Command
```json
{
  "cmd": "rain_reset"
}
```

### Response — `sensor_data`
```json
{
  "type": "sensor_data",
  "rain": {
    "enabled": true,
    "valid": true,
    "tips": 0,
    "mm": 0.0
  }
}
```

---

## 6. `mpu_reset` — Reset MPU6050 Movement Count ★ NEW

Resets the cumulative movement event counter on the MPU6050 back to zero. The accelerometer/gyroscope readings themselves are unaffected.

### Command
```json
{
  "cmd": "mpu_reset"
}
```

### Response — `sensor_data`
```json
{
  "type": "sensor_data",
  "mpu6050": {
    "enabled": true,
    "valid": true,
    "ax": 0.01,
    "ay": -0.02,
    "az": 1.00,
    "gx": 0.12,
    "gy": -0.05,
    "gz": 0.03,
    "movement_count": 0
  }
}
```

> `movement_count` will be `0` on a successful reset. All other MPU6050 fields are returned as a fresh snapshot.

---

## 7. `mpu_calibrate` — Calibrate MPU6050 ★ NEW

Triggers an offset calibration routine on the MPU6050. The device must be kept **level and completely still** during calibration. The ESP32 samples the sensor, calculates zero-point offsets, and applies them to all subsequent readings.

### Command
```json
{
  "cmd": "mpu_calibrate"
}
```

### Response — `sensor_data`

The response is a standard `sensor_data` message. The MPU6050 object includes a `calibrated` field to indicate success or failure.

**Success:**
```json
{
  "type": "sensor_data",
  "mpu6050": {
    "enabled": true,
    "valid": true,
    "ax": 0.00,
    "ay": 0.00,
    "az": 1.00,
    "gx": 0.00,
    "gy": 0.00,
    "gz": 0.00,
    "movement_count": 17,
    "calibrated": true
  }
}
```

**Failure:**
```json
{
  "type": "sensor_data",
  "mpu6050": {
    "enabled": true,
    "valid": false,
    "calibrated": false
  }
}
```

### `calibrated` Field Behaviour in the UI

| `calibrated` value | UI result |
|--------------------|-----------|
| `true` | Calibration box shows **"Calibration done. Offsets applied."** (blue) |
| `false` | Calibration box shows **"Calibration failed — check sensor connection."** (red) |
| field absent | Calibration box is not updated (normal sensor poll) |

> `calibrated` is only present in responses to `mpu_calibrate`. It is not included in routine `sensor_read` responses, so regular polling never accidentally clears the calibration status box.

---

## 8. `gsm_at` — Send Raw AT Command

Forwards a raw AT command string to the A7670C GSM module and returns the modem's response. Used both by the AT terminal and for APN configuration.

### Command
```json
{
  "cmd": "gsm_at",
  "command": "AT+CSQ"
}
```

### APN Configuration Example
```json
{
  "cmd": "gsm_at",
  "command": "AT+CGDCONT=1,\"IP\",\"airtelgprs.com\""
}
```

### Response — `gsm_response`
```json
{
  "type": "gsm_response",
  "response": "+CSQ: 18,0\r\nOK"
}
```

> The raw modem response string is appended to the AT terminal display. Each command produces exactly one `gsm_response`.

### Quick Reference AT Commands

| Button | Command | Purpose |
|--------|---------|---------|
| AT | `AT` | Basic ping |
| Signal | `AT+CSQ` | Signal quality |
| SIM No | `AT+CNUM` | SIM phone number |
| Operator | `AT+COPS?` | Current operator |
| Voltage | `AT+CBC` | Battery/supply voltage |
| Reg | `AT+CREG?` | Network registration |
| APN | `AT+CGDCONT?` | Current APN |
| IMSI | `AT+CIMI` | SIM IMSI |
| Balance | `AT+CUSD=1,"*121#"` | SIM balance (USSD) |

---

## 9. `gsm_test` — Run GSM Diagnostic Test

Runs one of four diagnostic tests on the GSM module.

### Command
```json
{
  "cmd": "gsm_test",
  "test": "sms"
}
```

### `test` Values

| Value | Description |
|-------|-------------|
| `"sms"` | Send a test SMS message |
| `"call"` | Place a test voice call |
| `"api"` | Test HTTP API connectivity |
| `"ota"` | Trigger OTA firmware update |

### Response — `gsm_test`
```json
{
  "type": "gsm_test",
  "success": true,
  "result": "SMS sent to +919876543210"
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `success` | bool | `true` → result box shown in green; `false` → shown in red |
| `result` | string | Human-readable result message displayed to the user |

---

## 10. `gsm_reset` — Reset GSM Module

Triggers a hardware or software reset of the A7670C GSM module.

### Command
```json
{
  "cmd": "gsm_reset"
}
```

### Response — `gsm_info`

Returns a fresh (typically empty/reinitializing) GSM info snapshot:

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

> The module will take several seconds to reinitialize. Send `gsm_info` again after a delay to check registration status.

---

## 11. `lora_trigger` — Trigger LoRa Hooter

Sends a trigger packet over LoRa (433 MHz, RA-02 module) to the remote hooter Arduino receiver. The hooter activates within approximately 2 seconds of receipt.

### Command
```json
{
  "cmd": "lora_trigger",
  "value": 1
}
```

### Response — `lora_result`
```json
{
  "type": "lora_result",
  "sent": true
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `sent` | bool | `true` → packet transmitted, badge **OK**; `false` → transmission failed, badge **ERR** |

> If `sent` is `false`, check LoRa wiring and hooter power supply.

---

## 12. `system_info` — Get ESP32 System Info

Requests runtime system statistics from the ESP32.

### Command
```json
{
  "cmd": "system_info"
}
```

### Response — `system_info`
```json
{
  "type": "system_info",
  "uptime": 3725,
  "heap": 142336,
  "clients": 1
}
```

### Field Reference

| Field | Unit | Display | Description |
|-------|------|---------|-------------|
| `uptime` | seconds | `1h 2m 5s` | Time since last ESP32 boot |
| `heap` | bytes | `139.0 KB` | Free heap memory (divided by 1024) |
| `clients` | count | — | Number of active MQTT clients connected to broker |

---

## Auto-Polling Mode

The UI supports automatic periodic polling at a 5-second interval:

```
Auto ON  → setInterval(5000ms) → sensor_read + power_read  (published to lms/commands)
Auto OFF → clears the interval
```

Controlled by the **Auto: ON/OFF** button on the Sensors tab. GSM data is not auto-polled.

---

## MPU6050 — Movement Count Behaviour

The `movement_count` field is an integer that the ESP32 increments each time a movement event exceeds its detection threshold. It persists across sensor polls and only resets when an explicit `mpu_reset` command is received.

```
sensor_read response  →  movement_count: 17   (keeps accumulating)
mpu_reset response    →  movement_count: 0    (reset to zero)
mpu_calibrate         →  movement_count: 17   (count preserved, offsets recalculated)
```

The UI renders `movement_count` as a wide highlighted tile (blue border) below the six axis readings, making it visually distinct from the raw IMU values.

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

- If the MQTT connection closes unexpectedly, the UI displays a countdown and reconnects after **5 seconds**.
- JSON parse errors on incoming `lms/data` messages are silently ignored.
- If `ws_send()` is called while the MQTT client is not connected, the message is dropped silently.
- Sensor keys missing from a `sensor_data` response are silently skipped — no UI error is shown.
- The `calibrated` field is only expected in `mpu_calibrate` responses. Its absence in normal `sensor_read` responses is intentional and does not affect the calibration status display.