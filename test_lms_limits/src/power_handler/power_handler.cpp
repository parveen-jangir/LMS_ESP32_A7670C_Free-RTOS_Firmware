#include "power_handler.h"

powerMonitor::powerMonitor(uint8_t addr) : i2cAddr(addr) {}

bool powerMonitor::begin() {
    if (!ina.begin(i2cAddr, &Wire)) {
        return false;
    }

    ina.setAveragingMode(INA3221_AVG_16_SAMPLES);

    for (uint8_t i = 0; i < 3; i++) {
        ina.setShuntResistance(i, SHUNT_RESISTANCE);
    }

    lastEnergyUpdate = millis();
    return true;
}

float powerMonitor::getSolarVoltage() {
    return ina.getBusVoltage(SOLAR_VOLTAGE_CHANNEL);
}

float powerMonitor::getSolarCurrent() {
    return ina.getCurrentAmps(SOLAR_VOLTAGE_CHANNEL) * 1000.0f;
}

float powerMonitor::getSolarPower() {
    return getSolarVoltage() * getSolarCurrent();   // mW
}

float powerMonitor::getBatteryVoltage() {
    return ina.getBusVoltage(BATTERY_VOLTAGE_CHANNEL);
}

float powerMonitor::getBatteryCurrent() {
    return ina.getCurrentAmps(BATTERY_VOLTAGE_CHANNEL) * 1000.0f;
}

float powerMonitor::getBatteryPower() {
    return getBatteryVoltage() * getBatteryCurrent();  // mW
}

float powerMonitor::getSystemLoadPower() {
    return getSolarPower() + getBatteryPower();
}

float powerMonitor::getChargingPower() {
    float solarP = getSolarPower();
    float loadP  = getSystemLoadPower();
    return max(0.0f, solarP - loadP);
}

PowerFlowState powerMonitor::getPowerFlowState() {
    float solarP = getSolarPower();
    float batP   = getBatteryPower();

    if (solarP > 100 && batP < 50) return SOLAR_ONLY;
    if (solarP < 50 && batP > 100) return BATTERY_DISCHARGING;
    if (solarP > 100 && batP > 100) return HYBRID;
    return IDLE;
}

float powerMonitor::getSolarEnergyWh() {
    return solarEnergyWh;
}

float powerMonitor::getBatteryDischargedWh() {
    return batteryDischargedWh;
}

void powerMonitor::resetEnergyCounters() {
    solarEnergyWh = 0.0;
    batteryDischargedWh = 0.0;
}

void powerMonitor::update() {
    unsigned long now = millis();

    // Update energy every 1 second
    if (now - lastEnergyUpdate >= ENERGY_INTERVAL) {
        float solarP = getSolarPower();           // mW
        float batteryP = getBatteryPower();       // mW  (positive = discharged)

        // Integrate using average of previous and current power
        float solarAvg = (prevSolarPower + solarP) * 0.5f;
        float batteryAvg = (prevBatteryPower + batteryP) * 0.5f;

        // Add energy: (mW * 1s) / 3,600,000 = Wh
        solarEnergyWh += solarAvg / 3600000.0f;
        batteryDischargedWh += batteryAvg / 3600000.0f;

        prevSolarPower = solarP;
        prevBatteryPower = batteryP;
        lastEnergyUpdate = now;
    }
}