#pragma once
#include <Arduino.h>
#include <Adafruit_INA3221.h>
#include "config.h"

enum PowerFlowState {
    SOLAR_ONLY,
    BATTERY_DISCHARGING,
    HYBRID,
    IDLE
};

class powerMonitor {
private:
    Adafruit_INA3221 ina;
    uint8_t i2cAddr;

    // Energy counters
    float solarEnergyWh = 0.0;
    float batteryDischargedWh = 0.0;

    unsigned long lastEnergyUpdate = 0;

    // Previous power values for integration
    float prevSolarPower = 0.0;
    float prevBatteryPower = 0.0;

public:
    powerMonitor(uint8_t addr = 0x40);

    bool begin();

    // Voltage & Current
    float getSolarVoltage();      // V
    float getSolarCurrent();      // mA
    float getSolarPower();        // mW

    float getBatteryVoltage();    // V
    float getBatteryCurrent();    // mA  (+ve = discharging)
    float getBatteryPower();      // mW

    // Derived
    float getSystemLoadPower();   // mW
    float getChargingPower();     // mW

    PowerFlowState getPowerFlowState();

    // Energy (updated every second)
    float getSolarEnergyWh();
    float getBatteryDischargedWh();

    void resetEnergyCounters();

    // Call this in loop() as fast as you want (recommended every 100-500ms)
    void update();
};