#ifndef CONFIG_H
#define CONFIG_H

// ==================== I2C Configuration ====================
#define I2C_SDA_PIN       21
#define I2C_SCL_PIN       22
#define I2C_FREQUENCY     400000  // 400kHz I2C clock
#define I2C_TIMEOUT_MS    1000    // I2C timeout in ms

// ==================== GPIO Pin Configuration ====================
#define DHT22_PIN           32
#define SOIL_MOISTURE_PIN   27
#define RAIN_GAUGE_PIN      33
#define MPU_INTERRUPT_PIN   15

// ==================== I2C Sensor Addresses ====================
#define BMP180_ADDRESS    0x77
#define BH1750_ADDRESS    0x23
#define MPU6050_ADDRESS   0x68

// ==================== Sensor State ====================
#define SENSOR_ON         1
#define SENSOR_OFF        0

// ==================== Sampling Configuration ====================
#define SENSOR_READ_INTERVAL_MS  1000  // Read all sensors every 1 second
#define RAIN_GAUGE_DEBOUNCE_MS   100   // Debounce for rain gauge

// ==================== ADC Configuration ====================
#define ADC_RESOLUTION    12       // 12-bit ADC (0-4095)
#define ADC_ATTENUATION   3        // 11dB attenuation (~3.3V)

// ==================== Calibration Defaults ====================
#define BH1750_DEFAULT_OFFSET           0.0
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

#define SOLAR_VOLTAGE_CHANNEL     2
#define BATTERY_VOLTAGE_CHANNEL   0
#define ENERGY_INTERVAL_MS        1000
#define SHUNT_RESISTANCE          0.05f   // ohms

// ==================== LoRa Configuration ====================
#define LORA_FREQUENCY            433E6  // LoRa frequency (433 MHz)
#define LORA_TX_POWER             14     // LoRa transmission power (dBm)
#define LORA_SPREADING_FACTOR     9      // LoRa spreading factor (7-12)

#define LORA_NSS_PIN              5      // LoRa NSS pin
#define LORA_RESET_PIN            25     // LoRa reset pin
#define LORA_DIO0_PIN             26     // LoRa DIO0 pin
#define MAX_PAYLOAD_LEN         1024

// ==================== A7670C Modem Configuration ====================
#define GSM_SERIAL Serial2
#define MODEM_RX_PIN 16
#define MODEM_TX_PIN 17
#define MODEM_PWR_PIN 2
#define MODEM_BAUD_RATE 115200

// ==================== Command Handler Configuration ====================
#define COMMAND_QUEUE_DEPTH 10

// ==================== FreeRTOS Configuration ====================
#define COMMAND_TASK_STACK_SIZE     8192
#define COMMAND_TASK_PRIORITY       7
#define COMMAND_TASK_DELAY_MS       5
#define COMMAND_TASK_CORE_ID        0

#define SENSOR_TASK_STACK_SIZE    4096
#define SENSOR_TASK_PRIORITY      7
#define SENSOR_TASK_CORE          1    // Core 1 for sensor tasks

#define GSM_TASK_STACK_SIZE       4096
#define GSM_TASK_PRIORITY         6
#define GSM_TASK_CORE             1    // Core 1 for GSM tasks

#define API_TASK_STACK_SIZE       4096
#define API_TASK_PRIORITY         5
#define API_TASK_CORE             1    // Core 1 for API tasks

// ==================== A7670C Configuration ====================
#define APN         "airtelgprs.com"      // Your carrier APN
#define CFG_SMS_NUMBER  "+919511511257"    // Number for SMS/call tests
// #define MQTT_BROKER "35.182.218.175"
#define MQTT_BROKER "76.13.243.127"
#define MQTT_PORT    1883
#define MQTT_USER   "lms_mqtt_broker"
#define MQTT_PASS   "landslidemonitoringsystem"

#define DEFAULT_TID "t1"
#define API_HIT_INTERVAL_MS 10*60*1000  // 10 minutes

#define OTA_URL "https://raw.githubusercontent.com/parveen-jangir/mqtt_bin_file/main/test_esp_ota.bin"
#endif // CONFIG_H