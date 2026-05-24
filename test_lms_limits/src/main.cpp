#include "power_handler/power_handler.h"

powerMonitor monitor;

void setup() {
    Serial.begin(115200);
    if (!monitor.begin()) {
        Serial.println("INA3221 init failed!");
        while(1);
    }
}

void loop() {
    monitor.update();     // Call frequently (every 100-500ms is good)

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();

        Serial.printf("Solar: %.2f W | Battery: %.2f W | Load: %.2f W | Charging: %.2f W\n",
            monitor.getSolarPower()/1000.0,
            monitor.getBatteryPower()/1000.0,
            monitor.getSystemLoadPower()/1000.0,
            monitor.getChargingPower()/1000.0);

        Serial.printf("Solar Energy: %.3f Wh | Battery Out: %.3f Wh\n\n",
            monitor.getSolarEnergyWh(),
            monitor.getBatteryDischargedWh());

        Serial.println("Power Flow State: " + String(
            monitor.getPowerFlowState() == SOLAR_ONLY ? "SOLAR_ONLY" :
            monitor.getPowerFlowState() == BATTERY_DISCHARGING ? "BATTERY_DISCHARGING" :
            monitor.getPowerFlowState() == HYBRID ? "HYBRID" : "IDLE"
        ));

        Serial.println("Battery Voltage: " + String(monitor.getBatteryVoltage()) + " V");
        Serial.println("Battery Current: " + String(monitor.getBatteryCurrent()) + " mA");
        Serial.println("Solar Voltage: " + String(monitor.getSolarVoltage()) + " V");
        Serial.println("Solar Current: " + String(monitor.getSolarCurrent()) + " mA");
        Serial.println();
    }
}