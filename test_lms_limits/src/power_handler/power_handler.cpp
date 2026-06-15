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
    if (now - lastEnergyUpdate >= ENERGY_INTERVAL_MS) {
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

void powerMonitor::getBatteryStatusJson(JsonDocument& doc) {
    float voltage = getBatteryVoltage();
    float current = getBatteryCurrent();   // mA, +ve = discharging
    float power   = getBatteryPower();     // mW

    // Estimate SoC from voltage (adjust thresholds to your battery)
    // Assumes LiPo: 3.0V = 0%, 4.2V = 100%
    float percent = constrain((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f, 0.0f, 100.0f);

    bool charging = (current < 0);   // negative current = charging

    doc["type"]      = "battery_status";
    doc["charging"]  = charging;
    doc["voltage"]   = round(voltage * 1000) / 1000.0f;
    doc["current"]   = round(current * 1000) / 1000.0f;
    doc["power_mw"]  = round(power * 1000) / 1000.0f;
    doc["energy_wh"] = round(getBatteryDischargedWh() * 1000) / 1000.0f;
    doc["percent"]   = (int)round(percent);
    doc["status"]    = "ok";
}

void powerMonitor::getSolarStatusJson(JsonDocument& doc) {
    float voltage = getSolarVoltage();
    float current = getSolarCurrent();   // mA
    float power   = getSolarPower();     // mW

    doc["type"]      = "solar_status";
    doc["voltage"]   = round(voltage * 1000) / 1000.0f;
    doc["current"]   = round(current * 1000) / 1000.0f;
    doc["power_mw"]  = round(power * 1000) / 1000.0f;
    doc["energy_wh"] = round(getSolarEnergyWh() * 1000) / 1000.0f;
    doc["status"]    = "ok";
}

void powerMonitor::getSystemPowerJson(JsonDocument& doc) {
    PowerFlowState state = getPowerFlowState();

    const char* source;
    float voltage, current, power, energy;

    switch (state) {
        case SOLAR_ONLY:
            source  = "solar";
            voltage = getSolarVoltage();
            current = getSolarCurrent();
            power   = getSolarPower();
            energy  = getSolarEnergyWh();
            break;

        case BATTERY_DISCHARGING:
            source  = "battery";
            voltage = getBatteryVoltage();
            current = getBatteryCurrent();
            power   = getBatteryPower();
            energy  = getBatteryDischargedWh();
            break;

        case HYBRID:
            source  = "hybrid";
            voltage = getBatteryVoltage();
            current = getSolarCurrent() + getBatteryCurrent();
            power   = getSystemLoadPower();
            energy  = getSolarEnergyWh() + getBatteryDischargedWh();
            break;

        case IDLE:
        default:
            source  = "idle";
            voltage = getBatteryVoltage();
            current = 0.0f;
            power   = 0.0f;
            energy  = 0.0f;
            break;
    }

    doc["type"]      = "system_power";
    doc["source"]    = source;
    doc["voltage"]   = round(voltage * 1000) / 1000.0f;
    doc["current"]   = round(current * 1000) / 1000.0f;
    doc["power_mw"]  = round(power * 1000) / 1000.0f;
    doc["energy_wh"] = round(energy * 1000) / 1000.0f;
    doc["status"]    = "ok";
}