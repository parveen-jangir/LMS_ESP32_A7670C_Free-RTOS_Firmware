#include <Arduino.h>
#include "SensorManager/SensorManager.h"

// Global sensor manager instance
SensorManager sensorManager;

// Test state variables
bool continuousMode = false;
uint32_t lastMenuTime = 0;
const uint32_t MENU_TIMEOUT = 100;

// Function prototypes
void printMainMenu();
void printSensorMenu();
void printCalibrationMenu();
void handleMainMenu(char input);
void handleSensorMenu(char input);
void handleCalibrationMenu(char input);
void testAllSensors();
void testIndividualSensor(const char* sensorName);
void printCalibrationValues();
void setCalibrationValues();
void resetCalibration();
void performAutoCalibration();
void printHelpInfo();

void setup() {
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    delay(2000);
    
    // Clear screen
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║       ESP32 Multi-Sensor Library - Comprehensive Testing       ║");
    Serial.println("║                                                                ║");
    Serial.println("║  Sensors: BMP180, BH1750, MPU6050, DHT22, SoilMoisture, Rain  ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
    
    // Initialize sensor manager
    Serial.println("[*] Initializing sensors...");
    delay(1000);
    
    if (!sensorManager.initialize()) {
        Serial.println("\n[ERROR] Failed to initialize sensors!");
        Serial.println("Please check:");
        Serial.println("  - I2C connections (GPIO21=SDA, GPIO22=SCL)");
        Serial.println("  - GPIO connections for digital sensors");
        Serial.println("  - Power supply to all sensors");
        Serial.println("\nSystem halted. Please fix hardware issues and restart.\n");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("[✓] All sensors initialized successfully!\n");
    
    // Default calibration setup
    Serial.println("[*] Setting default calibration offsets...");
    
    // BMP180 calibration
    sensorManager.setBMP180CalibrationOffset(0.0, 0.0);
    
    // BH1750 calibration
    sensorManager.setBH1750CalibrationOffset(0.0);
    
    // MPU6050 calibration
    sensorManager.setMPU6050CalibrationOffset(
        0.0, 0.0, 0.0,  // Accel offsets
        0.0, 0.0, 0.0,  // Gyro offsets
        0.0              // Temp offset
    );
    
    // DHT22 calibration
    sensorManager.setDHT22CalibrationOffset(0.0, 0.0);
    
    // Soil Moisture calibration (0=dry, 4095=wet)
    sensorManager.setSoilMoistureCalibrationOffset(0, 4095);
    
    // Rain Gauge calibration (0.2794 mm per tip)
    sensorManager.setRainGaugeTipVolume(0.2794);
    
    Serial.println("[✓] Calibration values set to defaults\n");
    
    // Set read interval
    sensorManager.setReadInterval(5000);
    
    // Start continuous reading task
    Serial.println("[*] Starting continuous sensor reading task...");
    if (!sensorManager.startReadingTask()) {
        Serial.println("[ERROR] Failed to start sensor task!");
        while (1) delay(1000);
    }
    
    Serial.println("[✓] Sensor task started successfully!\n");
    delay(1000);
    
    // Print sensor status
    sensorManager.printSensorStatus();
    
    // Print main menu
    printMainMenu();
}

void loop() {
    // Check for serial input
    if (Serial.available()) {
        char input = Serial.read();
        
        // Clear any remaining characters in buffer
        while (Serial.available()) {
            Serial.read();
        }
        
        handleMainMenu(input);
        lastMenuTime = millis();
    }
    
    // In continuous mode, print readings every 10 seconds
    if (continuousMode && (millis() - lastMenuTime) > 10000) {
        sensorManager.printLastReadings();
        lastMenuTime = millis();
    }
    
    delay(100);
}

// ================== MENU FUNCTIONS ==================

void printMainMenu() {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║                      MAIN MENU                                 ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  [1] Test All Sensors                                          ║");
    Serial.println("║  [2] Test Individual Sensor                                    ║");
    Serial.println("║  [3] View Sensor Status                                        ║");
    Serial.println("║  [4] Sensor Enable/Disable                                     ║");
    Serial.println("║  [5] Calibration Menu                                          ║");
    Serial.println("║  [6] Continuous Mode (Auto Print Every 10s)                    ║");
    Serial.println("║  [7] Stop Continuous Mode                                      ║");
    Serial.println("║  [8] Manual Single Read                                        ║");
    Serial.println("║  [9] View Calibration Values                                   ║");
    Serial.println("║  [0] Help & Info                                               ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");
    Serial.println("\nEnter your choice (0-9): ");
}

void printSensorMenu() {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║               SELECT SENSOR TO TEST                            ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  [1] BMP180 (Barometric Pressure & Temperature)                ║");
    Serial.println("║  [2] BH1750 (Light Intensity)                                  ║");
    Serial.println("║  [3] MPU6050 (Accelerometer & Gyroscope)                       ║");
    Serial.println("║  [4] DHT22 (Temperature & Humidity)                            ║");
    Serial.println("║  [5] Soil Moisture Sensor                                      ║");
    Serial.println("║  [6] Rain Gauge                                                ║");
    Serial.println("║  [B] Back to Main Menu                                         ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");
    Serial.println("\nEnter your choice (1-6, B): ");
}

void printCalibrationMenu() {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║               CALIBRATION MENU                                 ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  [1] View Current Calibration Values                           ║");
    Serial.println("║  [2] Set Custom Calibration Values                             ║");
    Serial.println("║  [3] Reset to Default Calibration                              ║");
    Serial.println("║  [4] Auto Calibration (Zero Point)                             ║");
    Serial.println("║  [B] Back to Main Menu                                         ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");
    Serial.println("\nEnter your choice (1-4, B): ");
}

void handleMainMenu(char input) {
    switch (input) {
        case '1':
            Serial.println("\n[→] Starting comprehensive sensor test...\n");
            delay(500);
            testAllSensors();
            delay(1000);
            printMainMenu();
            break;
            
        case '2':
            printSensorMenu();
            break;
            
        case '3':
            Serial.println();
            sensorManager.printSensorStatus();
            delay(2000);
            printMainMenu();
            break;
            
        case '4':
            handleSensorMenu('X');  // Show sensor enable/disable menu
            break;
            
        case '5':
            printCalibrationMenu();
            break;
            
        case '6':
            continuousMode = true;
            Serial.println("\n[✓] Continuous mode ENABLED");
            Serial.println("Sensor readings will print every 10 seconds");
            Serial.println("Press any key to return to menu\n");
            lastMenuTime = millis();
            break;
            
        case '7':
            continuousMode = false;
            Serial.println("\n[✓] Continuous mode DISABLED\n");
            delay(500);
            printMainMenu();
            break;
            
        case '8':
            Serial.println("\n[→] Performing manual single sensor read...\n");
            delay(200);
            sensorManager.readAllSensors();
            sensorManager.printLastReadings();
            delay(2000);
            printMainMenu();
            break;
            
        case '9':
            Serial.println();
            printCalibrationValues();
            delay(2000);
            printMainMenu();
            break;
            
        case '0':
            printHelpInfo();
            delay(3000);
            printMainMenu();
            break;
            
        default:
            Serial.println("\n[!] Invalid choice. Please enter 0-9\n");
            delay(500);
            printMainMenu();
            break;
    }
}

void handleSensorMenu(char input) {
    if (input == 'X') {
        // Show enable/disable menu
        Serial.write(27);
        Serial.print("[2J");
        Serial.write(27);
        Serial.print("[H");
        
        Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
        Serial.println("║           SENSOR ENABLE/DISABLE CONTROL                       ║");
        Serial.println("╠════════════════════════════════════════════════════════════════╣");
        Serial.printf("║  [1] Toggle BMP180          [Status: %s ]              ║", sensorManager.isSensorEnabled("BMP180") ? "ON" : "OFF");
        Serial.printf("║  [2] Toggle BH1750          [Status: %s ]              ║", sensorManager.isSensorEnabled("BH1750") ? "ON" : "OFF");
        Serial.printf("║  [3] Toggle MPU6050         [Status: %s ]              ║", sensorManager.isSensorEnabled("MPU6050") ? "ON" : "OFF");
        Serial.printf("║  [4] Toggle DHT22           [Status: %s ]              ║", sensorManager.isSensorEnabled("DHT22") ? "ON" : "OFF");
        Serial.printf("║  [5] Toggle SoilMoisture    [Status: %s ]              ║", sensorManager.isSensorEnabled("SoilMoisture") ? "ON" : "OFF");
        Serial.printf("║  [6] Toggle RainGauge       [Status: %s ]              ║", sensorManager.isSensorEnabled("RainGauge") ? "ON" : "OFF");
        Serial.print(sensorManager.isSensorEnabled("RainGauge") ? "ON" : "OFF");
        Serial.println(" ]              ║");
        Serial.println("║  [B] Back to Main Menu                                         ║");
        Serial.println("╚════════════════════════════════════════════════════════════════╝");
        Serial.println("\nEnter your choice (1-6, B): ");
        return;
    }
    
    switch (input) {
        case '1':
            if (sensorManager.isSensorEnabled("BMP180")) {
                sensorManager.disableSensor("BMP180");
                Serial.println("\n[✓] BMP180 DISABLED\n");
            } else {
                sensorManager.enableSensor("BMP180");
                Serial.println("\n[✓] BMP180 ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case '2':
            if (sensorManager.isSensorEnabled("BH1750")) {
                sensorManager.disableSensor("BH1750");
                Serial.println("\n[✓] BH1750 DISABLED\n");
            } else {
                sensorManager.enableSensor("BH1750");
                Serial.println("\n[✓] BH1750 ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case '3':
            if (sensorManager.isSensorEnabled("MPU6050")) {
                sensorManager.disableSensor("MPU6050");
                Serial.println("\n[✓] MPU6050 DISABLED\n");
            } else {
                sensorManager.enableSensor("MPU6050");
                Serial.println("\n[✓] MPU6050 ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case '4':
            if (sensorManager.isSensorEnabled("DHT22")) {
                sensorManager.disableSensor("DHT22");
                Serial.println("\n[✓] DHT22 DISABLED\n");
            } else {
                sensorManager.enableSensor("DHT22");
                Serial.println("\n[✓] DHT22 ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case '5':
            if (sensorManager.isSensorEnabled("SoilMoisture")) {
                sensorManager.disableSensor("SoilMoisture");
                Serial.println("\n[✓] SoilMoisture DISABLED\n");
            } else {
                sensorManager.enableSensor("SoilMoisture");
                Serial.println("\n[✓] SoilMoisture ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case '6':
            if (sensorManager.isSensorEnabled("RainGauge")) {
                sensorManager.disableSensor("RainGauge");
                Serial.println("\n[✓] RainGauge DISABLED\n");
            } else {
                sensorManager.enableSensor("RainGauge");
                Serial.println("\n[✓] RainGauge ENABLED\n");
            }
            delay(500);
            handleSensorMenu('X');
            break;
            
        case 'B':
        case 'b':
            printMainMenu();
            break;
            
        default:
            Serial.println("\n[!] Invalid choice\n");
            delay(500);
            handleSensorMenu('X');
            break;
    }
}

void handleCalibrationMenu(char input) {
    switch (input) {
        case '1':
            Serial.println();
            printCalibrationValues();
            delay(2000);
            printCalibrationMenu();
            break;
            
        case '2':
            setCalibrationValues();
            delay(1000);
            printCalibrationMenu();
            break;
            
        case '3':
            Serial.println("\n[*] Resetting calibration to defaults...\n");
            resetCalibration();
            Serial.println("[✓] Calibration reset to defaults\n");
            delay(1000);
            printCalibrationMenu();
            break;
            
        case '4':
            Serial.println("\n[*] Starting auto calibration...\n");
            performAutoCalibration();
            delay(1000);
            printCalibrationMenu();
            break;
            
        case 'B':
        case 'b':
            printMainMenu();
            break;
            
        default:
            Serial.println("\n[!] Invalid choice\n");
            delay(500);
            printCalibrationMenu();
            break;
    }
}

// ================== TEST FUNCTIONS ==================

void testAllSensors() {
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║              COMPREHENSIVE SENSOR TEST REPORT                  ║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
    
    // Read all sensors
    sensorManager.readAllSensors();
    
    Serial.println("[→] Reading all sensors...\n");
    delay(500);
    
    // Get all readings
    AllSensorReadings readings = sensorManager.getAllReadings();
    
    // Test counter
    int testsPassed = 0;
    int testsFailed = 0;
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ BMP180 - Barometric Pressure & Temperature Sensor               │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.bmp180.isValid) {
        Serial.printf("│  Temperature: %.2f °C\n", readings.bmp180.temperature);
        Serial.printf("│  Pressure:    %.2f hPa\n", readings.bmp180.pressure);
        Serial.printf("│  Altitude:    ~%.0f m (estimated)\n", 
                     (44330.0 * (1.0 - pow(readings.bmp180.pressure / 1013.25, 1.0/5.255))));
        Serial.println("│  Status:      ✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("│  Status:      ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ BH1750 - Light Intensity Sensor                                 │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.bh1750.isValid) {
        Serial.printf("│  Illuminance: %.2f Lux\n", readings.bh1750.illuminance);
        Serial.print("│  Light Level: ");
        if (readings.bh1750.illuminance < 50) Serial.println("Dark");
        else if (readings.bh1750.illuminance < 500) Serial.println("Dim");
        else if (readings.bh1750.illuminance < 5000) Serial.println("Normal");
        else if (readings.bh1750.illuminance < 10000) Serial.println("Bright");
        else Serial.println("Very Bright");
        Serial.println("│  Status:      ✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("│  Status:      ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ MPU6050 - 6-Axis Accelerometer & Gyroscope                      │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.mpu6050.isValid) {
        Serial.println("│  Acceleration (m/s²):");
        Serial.printf("│    X: %.3f  Y: %.3f  Z: %.3f\n", 
                     readings.mpu6050.accelX, readings.mpu6050.accelY, readings.mpu6050.accelZ);
        Serial.println("│  Angular Velocity (°/s):");
        Serial.printf("│    X: %.2f  Y: %.2f  Z: %.2f\n", 
                     readings.mpu6050.gyroX, readings.mpu6050.gyroY, readings.mpu6050.gyroZ);
        Serial.printf("│  Temperature: %.2f °C\n", readings.mpu6050.temperature);
        Serial.println("│  Status:      ✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("│  Status:      ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ DHT22 - Digital Temperature & Humidity Sensor                   │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.dht22.isValid) {
        Serial.printf("│  Temperature: %.2f °C\n", readings.dht22.temperature);
        Serial.printf("│  Humidity:    %.2f %%RH\n", readings.dht22.humidity);
        Serial.printf("│  Dew Point:   ~%.2f °C (estimated)\n", 
                     readings.dht22.temperature - ((100 - readings.dht22.humidity) / 5.0));
        Serial.println("│  Status:      ✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("│  Status:      ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ Soil Moisture Sensor - Analog Moisture Level                    │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.soilMoisture.isValid) {
        Serial.printf("│  Raw ADC Value:    %d (0-4095)\n", readings.soilMoisture.rawValue);
        Serial.printf("│  Moisture Level:   %.2f %%\n", readings.soilMoisture.percentage);
        Serial.print("│  Status Desc:      ");
        if (readings.soilMoisture.percentage < 25) Serial.println("Very Dry");
        else if (readings.soilMoisture.percentage < 50) Serial.println("Dry");
        else if (readings.soilMoisture.percentage < 75) Serial.println("Moist");
        else Serial.println("Wet");
        Serial.println("│  Status:           ✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("│  Status:           ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    Serial.println("╔─────────────────────────────────────────────────────────────────╗");
    Serial.println("│ Rain Gauge - Tipping Bucket Rain Sensor                         │");
    Serial.println("╠─────────────────────────────────────────────────────────────────╣");
    if (readings.rainGauge.isValid) {
        Serial.printf("│  Tip Count:        %lu\n", readings.rainGauge.tipCount);
        Serial.printf("│  Total Rainfall:   %.2f mm\n", readings.rainGauge.totalRainfall);
        Serial.printf("│  Last Tip Time:    %lu ms\n", readings.rainGauge.lastTipTime);
        Serial.println("│  Status:           ✓ PASSED (Waiting for rain tips)");
        testsPassed++;
    } else {
        Serial.println("│  Status:           ✗ FAILED (No valid reading)");
        testsFailed++;
    }
    Serial.println("╚─────────────────────────────────────────────────────────────────╝\n");
    
    // Summary
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║                     TEST SUMMARY                               ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.printf("║  Tests Passed: %d/6  ✓\n", testsPassed);
    Serial.printf("║  Tests Failed: %d/6  %s\n", testsFailed, testsFailed > 0 ? "✗" : "");
    
    if (testsFailed == 0) {
        Serial.println("║                                                                ║");
        Serial.println("║  Result: ALL TESTS PASSED! System is operational.           ║");
    } else {
        Serial.println("║                                                                ║");
        Serial.println("║  Result: Some tests failed. Check connections.               ║");
    }
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}

void testIndividualSensor(const char* sensorName) {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.printf("\n[→] Testing %s...\n\n", sensorName);
    
    // Read sensor
    sensorManager.readAllSensors();
    AllSensorReadings readings = sensorManager.getAllReadings();
    
    if (strcmp(sensorName, "BMP180") == 0) {
        if (readings.bmp180.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ BMP180 - Barometric Pressure & Temperature Sensor               │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.printf("│  Temperature:      %.2f °C\n", readings.bmp180.temperature);
            Serial.printf("│  Pressure:         %.2f hPa\n", readings.bmp180.pressure);
            Serial.printf("│  Pressure:         %.2f Pa\n", readings.bmp180.pressure * 100);
            Serial.printf("│  Altitude (est.):  %.0f m\n", 
                         44330.0 * (1.0 - pow(readings.bmp180.pressure / 1013.25, 1.0/5.255)));
            Serial.println("│  Status:           ✓ OPERATIONAL");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] BMP180 sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - I2C connections (GPIO21=SDA, GPIO22=SCL)");
            Serial.println("  - Sensor power supply");
            Serial.println("  - I2C address (should be 0x77)");
        }
    }
    else if (strcmp(sensorName, "BH1750") == 0) {
        if (readings.bh1750.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ BH1750 - Light Intensity Sensor                                 │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.printf("│  Illuminance:      %.2f Lux\n", readings.bh1750.illuminance);
            Serial.print("│  Light Level:      ");
            if (readings.bh1750.illuminance < 50) Serial.println("Dark");
            else if (readings.bh1750.illuminance < 500) Serial.println("Dim (Indoor, night)");
            else if (readings.bh1750.illuminance < 5000) Serial.println("Normal (Typical indoor)");
            else if (readings.bh1750.illuminance < 10000) Serial.println("Bright (Office)");
            else Serial.println("Very Bright (Direct sunlight)");
            Serial.println("│  Status:           ✓ OPERATIONAL");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] BH1750 sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - I2C connections (GPIO21=SDA, GPIO22=SCL)");
            Serial.println("  - Sensor power supply");
            Serial.println("  - I2C address (should be 0x23)");
        }
    }
    else if (strcmp(sensorName, "MPU6050") == 0) {
        if (readings.mpu6050.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ MPU6050 - 6-Axis IMU (Accelerometer & Gyroscope)                │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.println("│  ACCELEROMETER DATA (m/s²):");
            Serial.printf("│    X-Axis: %8.3f  Y-Axis: %8.3f  Z-Axis: %8.3f\n", 
                         readings.mpu6050.accelX, readings.mpu6050.accelY, readings.mpu6050.accelZ);
            Serial.println("│");
            Serial.println("│  GYROSCOPE DATA (°/s):");
            Serial.printf("│    X-Axis: %8.2f  Y-Axis: %8.2f  Z-Axis: %8.2f\n", 
                         readings.mpu6050.gyroX, readings.mpu6050.gyroY, readings.mpu6050.gyroZ);
            Serial.println("│");
            Serial.printf("│  Temperature:      %.2f °C\n", readings.mpu6050.temperature);
            Serial.println("│  Status:           ✓ OPERATIONAL");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] MPU6050 sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - I2C connections (GPIO21=SDA, GPIO22=SCL)");
            Serial.println("  - Sensor power supply");
            Serial.println("  - I2C address (should be 0x68)");
        }
    }
    else if (strcmp(sensorName, "DHT22") == 0) {
        if (readings.dht22.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ DHT22 - Temperature & Humidity Sensor                           │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.printf("│  Temperature:      %.2f °C\n", readings.dht22.temperature);
            Serial.printf("│  Relative Humidity: %.2f %%RH\n", readings.dht22.humidity);
            float dewPoint = readings.dht22.temperature - ((100 - readings.dht22.humidity) / 5.0);
            Serial.printf("│  Dew Point (est.):  %.2f °C\n", dewPoint);
            Serial.println("│  Status:           ✓ OPERATIONAL");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] DHT22 sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - GPIO32 connection");
            Serial.println("  - Sensor power supply (3.3-5.5V)");
            Serial.println("  - Pull-up resistor on data line");
        }
    }
    else if (strcmp(sensorName, "SoilMoisture") == 0) {
        if (readings.soilMoisture.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ Soil Moisture Sensor - Analog Sensor                            │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.printf("│  Raw ADC Value:    %d (0-4095)\n", readings.soilMoisture.rawValue);
            Serial.printf("│  Moisture %% :      %.2f %%\n", readings.soilMoisture.percentage);
            Serial.print("│  Moisture Level:   ");
            if (readings.soilMoisture.percentage < 25) Serial.println("Very Dry");
            else if (readings.soilMoisture.percentage < 50) Serial.println("Dry");
            else if (readings.soilMoisture.percentage < 75) Serial.println("Moist");
            else Serial.println("Wet");
            Serial.println("│  Status:           ✓ OPERATIONAL");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] Soil Moisture sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - GPIO27 ADC connection");
            Serial.println("  - Sensor power supply");
            Serial.println("  - ADC configuration");
        }
    }
    else if (strcmp(sensorName, "RainGauge") == 0) {
        if (readings.rainGauge.isValid) {
            Serial.println("╔─────────────────────────────────────────────────────────────────╗");
            Serial.println("│ Rain Gauge - Tipping Bucket Sensor                              │");
            Serial.println("╠─────────────────────────────────────────────────────────────────╣");
            Serial.printf("│  Total Tips:       %lu\n", readings.rainGauge.tipCount);
            Serial.printf("│  Total Rainfall:   %.4f mm\n", readings.rainGauge.totalRainfall);
            Serial.printf("│  Last Tip Time:    %lu ms ago\n", millis() - readings.rainGauge.lastTipTime);
            Serial.println("│  Status:           ✓ OPERATIONAL (Awaiting rain)");
            Serial.println("╚─────────────────────────────────────────────────────────────────╝");
        } else {
            Serial.println("[ERROR] Rain Gauge sensor not responding!");
            Serial.println("Check:");
            Serial.println("  - GPIO33 connection");
            Serial.println("  - Sensor power supply");
            Serial.println("  - Interrupt pin configuration");
        }
    }
    
    Serial.println("\n[*] Press any key to continue...");
}

void printCalibrationValues() {
    SensorCalibration cal = sensorManager.getCalibration();
    
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║               CURRENT CALIBRATION VALUES                       ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  BMP180:");
    Serial.printf("║    Temperature Offset:  %.4f °C\n", cal.bmp180TempOffset);
    Serial.printf("║    Pressure Offset:     %.4f hPa\n", cal.bmp180PressureOffset);
    Serial.println("║");
    Serial.println("║  BH1750:");
    Serial.printf("║    Illuminance Offset:  %.4f Lux\n", cal.bh1750Offset);
    Serial.println("║");
    Serial.println("║  MPU6050:");
    Serial.printf("║    Accel X Offset:      %.4f m/s²\n", cal.mpu6050AccelOffsetX);
    Serial.printf("║    Accel Y Offset:      %.4f m/s²\n", cal.mpu6050AccelOffsetY);
    Serial.printf("║    Accel Z Offset:      %.4f m/s²\n", cal.mpu6050AccelOffsetZ);
    Serial.printf("║    Gyro X Offset:       %.4f °/s\n", cal.mpu6050GyroOffsetX);
    Serial.printf("║    Gyro Y Offset:       %.4f °/s\n", cal.mpu6050GyroOffsetY);
    Serial.printf("║    Gyro Z Offset:       %.4f °/s\n", cal.mpu6050GyroOffsetZ);
    Serial.printf("║    Temperature Offset:  %.4f °C\n", cal.mpu6050TempOffset);
    Serial.println("║");
    Serial.println("║  DHT22:");
    Serial.printf("║    Temperature Offset:  %.4f °C\n", cal.dht22TempOffset);
    Serial.printf("║    Humidity Offset:     %.4f %%RH\n", cal.dht22HumidityOffset);
    Serial.println("║");
    Serial.println("║  Soil Moisture:");
    Serial.printf("║    Min (Dry) Value:     %.0f\n", cal.soilMoistureOffsetMin);
    Serial.printf("║    Max (Wet) Value:     %.0f\n", cal.soilMoistureOffsetMax);
    Serial.println("║");
    Serial.println("║  Rain Gauge:");
    Serial.printf("║    Tip Volume:          %.4f mm/tip\n", cal.rainGaugeTipVolume);
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}

void setCalibrationValues() {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║            MANUAL CALIBRATION VALUE SETTING                   ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  [1] BMP180 Temperature Offset");
    Serial.println("║  [2] BMP180 Pressure Offset");
    Serial.println("║  [3] BH1750 Illuminance Offset");
    Serial.println("║  [4] DHT22 Temperature Offset");
    Serial.println("║  [5] DHT22 Humidity Offset");
    Serial.println("║  [6] Soil Moisture Calibration");
    Serial.println("║  [7] Rain Gauge Tip Volume");
    Serial.println("║  [B] Back");
    Serial.println("╚════════════════════════════════════════════════════════════════╝");
    
    Serial.println("\nNote: Calibration values are stored in RAM only.");
    Serial.println("For persistent storage, implement EEPROM/SPIFFS save function.\n");
    Serial.println("Enter choice (1-7, B): ");
}

void resetCalibration() {
    // Reset all calibration values to 0 (or defaults)
    sensorManager.setBMP180CalibrationOffset(0.0, 0.0);
    sensorManager.setBH1750CalibrationOffset(0.0);
    sensorManager.setMPU6050CalibrationOffset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    sensorManager.setDHT22CalibrationOffset(0.0, 0.0);
    sensorManager.setSoilMoistureCalibrationOffset(0, 4095);
    sensorManager.setRainGaugeTipVolume(0.2794);
}

void performAutoCalibration() {
    Serial.println("╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║             AUTO CALIBRATION (ZERO POINT)                      ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  Place sensor in neutral/reference position:");
    Serial.println("║  - BMP180: Allow 30 seconds to stabilize");
    Serial.println("║  - MPU6050: Keep flat and stationary");
    Serial.println("║  - Soil Moisture: Test in air (for dry reference)");
    Serial.println("║");
    Serial.println("║  Starting in 10 seconds...");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
    
    // Countdown
    for (int i = 10; i > 0; i--) {
        Serial.printf("[*] %d seconds remaining...\n", i);
        delay(1000);
    }
    
    Serial.println("\n[*] Reading reference values...\n");
    
    // Take multiple readings and average
    float bmp180TempSum = 0, bmp180PressureSum = 0;
    float mpu6050AccelXSum = 0, mpu6050AccelYSum = 0, mpu6050AccelZSum = 0;
    float mpu6050GyroXSum = 0, mpu6050GyroYSum = 0, mpu6050GyroZSum = 0;
    
    const int calibSamples = 10;
    
    for (int i = 0; i < calibSamples; i++) {
        sensorManager.readAllSensors();
        AllSensorReadings readings = sensorManager.getAllReadings();
        
        bmp180TempSum += readings.bmp180.temperature;
        bmp180PressureSum += readings.bmp180.pressure;
        mpu6050AccelXSum += readings.mpu6050.accelX;
        mpu6050AccelYSum += readings.mpu6050.accelY;
        mpu6050AccelZSum += readings.mpu6050.accelZ - 9.81; // Subtract gravity
        mpu6050GyroXSum += readings.mpu6050.gyroX;
        mpu6050GyroYSum += readings.mpu6050.gyroY;
        mpu6050GyroZSum += readings.mpu6050.gyroZ;
        
        Serial.printf("[%d/%d] Sample acquired\n", i + 1, calibSamples);
        delay(500);
    }
    
    // Calculate averages and set as offsets (negative to zero them)
    float bmp180TempAvg = bmp180TempSum / calibSamples;
    float bmp180PressureAvg = bmp180PressureSum / calibSamples;
    float mpu6050AccelXAvg = mpu6050AccelXSum / calibSamples;
    float mpu6050AccelYAvg = mpu6050AccelYSum / calibSamples;
    float mpu6050AccelZAvg = mpu6050AccelZSum / calibSamples;
    float mpu6050GyroXAvg = mpu6050GyroXSum / calibSamples;
    float mpu6050GyroYAvg = mpu6050GyroYSum / calibSamples;
    float mpu6050GyroZAvg = mpu6050GyroZSum / calibSamples;
    
    // Set calibration values (offsets will zero out reference readings)
    sensorManager.setBMP180CalibrationOffset(-bmp180TempAvg, -bmp180PressureAvg);
    sensorManager.setMPU6050CalibrationOffset(
        -mpu6050AccelXAvg, -mpu6050AccelYAvg, -mpu6050AccelZAvg,
        -mpu6050GyroXAvg, -mpu6050GyroYAvg, -mpu6050GyroZAvg,
        0.0
    );
    
    Serial.println("\n[✓] Auto calibration complete!\n");
    Serial.println("Reference values (will read ~0 now):");
    Serial.printf("  BMP180 Temp Offset:  %.4f °C\n", -bmp180TempAvg);
    Serial.printf("  BMP180 Pressure Off: %.4f hPa\n", -bmp180PressureAvg);
    Serial.printf("  MPU6050 Accel X:     %.4f m/s²\n", -mpu6050AccelXAvg);
    Serial.printf("  MPU6050 Accel Y:     %.4f m/s²\n", -mpu6050AccelYAvg);
    Serial.printf("  MPU6050 Accel Z:     %.4f m/s²\n", -mpu6050AccelZAvg);
    Serial.printf("  MPU6050 Gyro X:      %.4f °/s\n", -mpu6050GyroXAvg);
    Serial.printf("  MPU6050 Gyro Y:      %.4f °/s\n", -mpu6050GyroYAvg);
    Serial.printf("  MPU6050 Gyro Z:      %.4f °/s\n", -mpu6050GyroZAvg);
}

void printHelpInfo() {
    Serial.write(27);
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H");
    
    Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
    Serial.println("║                   SYSTEM INFORMATION & HELP                    ║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  CONNECTED SENSORS:");
    Serial.println("║");
    Serial.println("║  1. BMP180 (0x77) - Barometric Pressure & Temperature");
    Serial.println("║     - I2C Bus: GPIO21(SDA), GPIO22(SCL)");
    Serial.println("║     - Output: Temperature (°C), Pressure (hPa), Altitude");
    Serial.println("║");
    Serial.println("║  2. BH1750 (0x23) - Light Intensity Sensor");
    Serial.println("║     - I2C Bus: GPIO21(SDA), GPIO22(SCL)");
    Serial.println("║     - Output: Illuminance (Lux)");
    Serial.println("║");
    Serial.println("║  3. MPU6050 (0x68) - 6-Axis IMU");
    Serial.println("║     - I2C Bus: GPIO21(SDA), GPIO22(SCL)");
    Serial.println("║     - Output: Accel X/Y/Z (m/s²), Gyro X/Y/Z (°/s), Temp (°C)");
    Serial.println("║");
    Serial.println("║  4. DHT22 - Temperature & Humidity");
    Serial.println("║     - GPIO: GPIO32");
    Serial.println("║     - Output: Temperature (°C), Humidity (%%RH)");
    Serial.println("║");
    Serial.println("║  5. Soil Moisture Sensor - Analog Sensor");
    Serial.println("║     - GPIO: GPIO27 (ADC)");
    Serial.println("║     - Output: Raw Value (0-4095), Percentage (0-100%%)");
    Serial.println("║");
    Serial.println("║  6. Rain Gauge - Tipping Bucket");
    Serial.println("║     - GPIO: GPIO33");
    Serial.println("║     - Output: Tip Count, Total Rainfall (mm)");
    Serial.println("║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  FEATURES:");
    Serial.println("║  • Continuous background sensor reading (FreeRTOS task)");
    Serial.println("║  • Per-sensor calibration with offset support");
    Serial.println("║  • Enable/Disable individual sensors");
    Serial.println("║  • Manual single reads");
    Serial.println("║  • Auto zero-point calibration");
    Serial.println("║  • Error counting and status reporting");
    Serial.println("║  • Thread-safe data access with mutex");
    Serial.println("║  • Configurable read interval");
    Serial.println("║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  MENU NAVIGATION:");
    Serial.println("║  • Main Menu: Select feature (0-9)");
    Serial.println("║  • Sensor Menu: Test individual sensors");
    Serial.println("║  • Calibration Menu: Manage calibration values");
    Serial.println("║  • Continuous Mode: Auto-prints readings every 10 seconds");
    Serial.println("║");
    Serial.println("╠════════════════════════════════════════════════════════════════╣");
    Serial.println("║  TROUBLESHOOTING:");
    Serial.println("║");
    Serial.println("║  I2C Sensors Not Working:");
    Serial.println("║    - Check GPIO21(SDA) and GPIO22(SCL) connections");
    Serial.println("║    - Verify 3.3V power supply to sensors");
    Serial.println("║    - Add 10kΩ pull-up resistors if needed");
    Serial.println("║");
    Serial.println("║  DHT22 Not Responding:");
    Serial.println("║    - Check GPIO32 connection");
    Serial.println("║    - Add 10kΩ pull-up resistor");
    Serial.println("║    - Wait minimum 2 seconds between reads");
    Serial.println("║");
    Serial.println("║  Soil Moisture Wrong Values:");
    Serial.println("║    - Calibrate with dry air (0%) and wet (100%)");
    Serial.println("║    - Use calibration menu option [2]");
    Serial.println("║");
    Serial.println("║  Serial Output Garbage:");
    Serial.println("║    - Check baud rate: 115200");
    Serial.println("║");
    Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}