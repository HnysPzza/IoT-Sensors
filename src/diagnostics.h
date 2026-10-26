#pragma once
#include <Arduino.h>
#include "config.h"
#include "event_detector.h"

class Diagnostics {
public:
    Diagnostics(EventDetector& detector, SensorDriver& driver);

    void init();
    void printBootBanner();
    void logEvent(const DetectionEvent& event);
    void checkAndLogHeartbeat();

private:
    EventDetector& _detector;
    SensorDriver& _driver;
    uint32_t _lastHeartbeatTime;
};
