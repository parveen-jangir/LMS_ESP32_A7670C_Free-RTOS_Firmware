#pragma once
// ============================================================
// config.h — Hardware pin definitions & system configuration
// Derived from: ESP32-WROOM-32D schematic (IIT Mandi project)
// ============================================================

// ── Wi-Fi ────────────────────────────────────────────────────
#define WIFI_SSID       "Engineer"
#define WIFI_PASSWORD   "Mlp0Zaq1"
#define WIFI_RETRY_MS   5000        // Reconnect attempt interval

// ── WebSocket Server ─────────────────────────────────────────
#define WS_PORT         81          // WebSocket port

// ── I2C Bus (shared: BMP180, BH1750, MPU6050) ────────────────
// From schematic page 6: SCL=IO22, SDA=IO21
#define PIN_I2C_SCL     22
#define PIN_I2C_SDA     21

// ── DHT22 ────────────────────────────────────────────────────
// From schematic page 6/8: DHT net → IO33
#define PIN_DHT         33
#define DHT_TYPE        DHT22

// ── Soil Moisture Sensor (Analog) ────────────────────────────
// From schematic page 8: Soil analog → IO35 (input-only ADC pin)
#define PIN_SOIL        35
#define SOIL_ADC_MAX    4095        // 12-bit ADC
#define SOIL_DRY_VAL    3200        // Calibration: raw ADC when dry
#define SOIL_WET_VAL    1200        // Calibration: raw ADC when wet

// ── Short Circuit Protection (sensor power enable) ───────────
// From schematic page 7: SCP pins monitored by MCU GPIO
// These are read-back pins (digital) — active LOW = fuse blown
#define PIN_SCP_SOIL    36          // Soil_Vce SCP monitor (input)
#define PIN_SCP_MPU     39          // MPU_Vce SCP monitor  (input)
#define PIN_SCP_WEATHER 34          // Weather_Vce SCP monitor (input)
#define SCP_LOW_Voltage 1000
// Note: IO34,36,39 are input-only on ESP32 — correct usage

// ── Sensor Update Interval ───────────────────────────────────
#define SENSOR_UPDATE_MS    1000    // Stream interval in ms
#define HEALTH_UPDATE_MS    5000    // Status broadcast interval

// ── BMP180 I2C Address ───────────────────────────────────────
// Fixed by hardware: 0x77
#define BMP180_ADDR     0x77

// ── BH1750 I2C Address ───────────────────────────────────────
// ADDR pin → GND = 0x23 (default)
#define BH1750_ADDR     0x23

// ── MPU6050 I2C Address ──────────────────────────────────────
// AD0 → GND = 0x68
#define MPU6050_ADDR    0x68
#define MPU6050_INT_PIN 15