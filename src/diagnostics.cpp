#include "diagnostics.h"

Diagnostics::Diagnostics(EventDetector& detector, SensorDriver& driver)
    : _detector(detector), _driver(driver), _lastHeartbeatTime(0) {}

void Diagnostics::init() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(200); // Allow serial line to stabilize
    printBootBanner();
}

void Diagnostics::printBootBanner() {
    Serial.println();
    Serial.println(F("=============================================================="));
    Serial.println(F("   PETRO HYDRO PIPE CORP. - INDUSTRIAL MACHINE MONITORING    "));
    Serial.println(F("   ESP32 SENSOR NODE FIRMWARE                                "));
    Serial.println(F("=============================================================="));
    Serial.printf(" Node Identifier      : %s\n", NODE_ID);
    Serial.printf(" Firmware Version     : %s\n", FIRMWARE_VERSION);
    Serial.printf(" Target Sensor Pin    : GPIO %d (DevKit D4)\n", SENSOR_INPUT_PIN);
    Serial.printf(" Active State Logic   : %s (Optocoupler Inverted)\n", 
                  (SENSOR_ACTIVE_STATE == LOW) ? "ACTIVE LOW (0V on detect)" : "ACTIVE HIGH (3.3V on detect)");
    Serial.printf(" Role Configuration   : %s\n", 
                  IS_DOWNTIME_MASTER ? "Node 3 (DOWNTIME MASTER - Watchdog)" : 
                  (IS_PRODUCTION_COUNTER ? "Node 5 (PRODUCTION COUNTER - Falling Edge)" : "PROCESS EVENT MONITOR"));
    Serial.printf(" Debounce Duration    : %d ms\n", SENSOR_DEBOUNCE_MS);
    Serial.printf(" Inactivity Timeout   : %d ms\n", INACTIVITY_TIMEOUT_MS);
    Serial.println(F("=============================================================="));
    Serial.println(F(" Status: Sensor subsystem initialized. Listening for metal..."));
    Serial.println();
}

void Diagnostics::logEvent(const DetectionEvent& event) {
    if (!ENABLE_VERBOSE_LOG) return;

    if (event.type == EVENT_TIMEOUT) {
        Serial.println();
        Serial.println(F(">>> [ALARM / TIMEOUT] >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"));
        Serial.printf(" [!] NODE %s INACTIVITY WATCHDOG TRIGGERED!\n", NODE_ID);
        Serial.printf("     Duration without detection : %u ms (%u sec)\n", event.intervalMs, event.intervalMs / 1000);
        Serial.printf("     Cumulative cycles before   : %u\n", event.cumulativeCount);
        Serial.printf("     Condition reported to API  : NO_DETECTION_TIMEOUT\n");
        Serial.println(F("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"));
        Serial.println();
        return;
    }

    // Standard detection event log
    Serial.printf("[%9u ms] [EVENT: %-12s] | Raw GPIO: %d | Count: %4u | Delta: %5u ms\n",
                  event.timestampMs,
                  event.typeName,
                  _driver.readRawPin(),
                  event.cumulativeCount,
                  event.intervalMs);
}

void Diagnostics::checkAndLogHeartbeat() {
    uint32_t now = millis();
    if (now - _lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        _lastHeartbeatTime = now;

        Serial.printf("[HEARTBEAT] Uptime: %6u s | State: %-7s | Cycles: %4u | Inactive: %5u ms | Heap: %6u B\n",
                      now / 1000,
                      _detector.isStableDetected() ? "METAL" : "IDLE",
                      _detector.getCount(),
                      _detector.getMsSinceLastDetection(),
                      ESP.getFreeHeap());
    }
}
