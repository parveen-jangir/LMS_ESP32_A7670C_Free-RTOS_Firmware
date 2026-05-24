#ifndef CONFIG_H
#define CONFIG_H

// ==================== I2C Configuration ====================
#define I2C_SDA_PIN       21
#define I2C_SCL_PIN       22
#define I2C_FREQUENCY     400000  // 400kHz I2C clock
#define I2C_TIMEOUT_MS    1000    // I2C timeout in ms

// ==================== GPIO Pin Configuration ====================
#define DHT22_PIN         32
#define SOIL_MOISTURE_PIN 27
#define RAIN_GAUGE_PIN    33

// ==================== I2C Sensor Addresses ====================
#define BMP180_ADDRESS    0x77
#define BH1750_ADDRESS    0x23
#define MPU6050_ADDRESS   0x68

// ==================== Sensor State ====================
#define SENSOR_ON         1
#define SENSOR_OFF        0

// ==================== Sampling Configuration ====================
#define SENSOR_READ_INTERVAL_MS  5000  // Read all sensors every 5 seconds
#define DHT22_READ_INTERVAL_MS   2000  // DHT22 needs ~2s between reads
#define RAIN_GAUGE_DEBOUNCE_MS   100   // Debounce for rain gauge

// ==================== ADC Configuration ====================
#define ADC_RESOLUTION    12       // 12-bit ADC (0-4095)
#define ADC_ATTENUATION   3        // 11dB attenuation (~3.3V)

// ==================== FreeRTOS Configuration ====================
#define SENSOR_TASK_STACK_SIZE    4096
#define SENSOR_TASK_PRIORITY      2
#define SENSOR_TASK_CORE          1    // Core 1 for sensor tasks

// ==================== Calibration Defaults ====================
#define BH1750_DEFAULT_OFFSET      0.0
#define BMP180_DEFAULT_TEMP_OFFSET      0.0

// ==================== Error Handling ====================
#define MAX_SENSOR_RETRIES        3
#define SENSOR_ERROR_THRESHOLD    5    // Mark sensor as failed after 5 consecutive errors

// ==================== Debug Configuration ====================
#define DEBUG_ENABLED             1
#define DEBUG_SERIAL_BAUDRATE     115200

// ==================== Adafruit BMP085 Mode ====================
#define BMP085_MODE               Adafruit_BMP085::BMP085_ULTRAHIGHRES  // Ultra high resolution

// ==================== Adafruit DHT Configuration ====================
#define DHT_TYPE                  DHT22   // DHT 22 (AM2302)
#define DHT_READING_DELAY_MS      2000    // Minimum delay between DHT reads

#define SOLAR_VOLTAGE_CHANNEL   2
#define BATTERY_VOLTAGE_CHANNEL 0

#endif // CONFIG_H