#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensor_driver.h"

enum EventType {
    EVENT_NONE,
    EVENT_DETECTED,       // Target entered (Active edge)
    EVENT_NOT_DETECTED,   // Target exited (Inactive edge)
    EVENT_TIMEOUT,        // Inactivity timeout threshold exceeded (Node 3)
    EVENT_FAULT           // Abnormal sensor behavior
};

struct DetectionEvent {
    EventType type;
    const char* typeName;
    uint32_t timestampMs;
    uint32_t cumulativeCount;
    uint32_t intervalMs;
};

class EventDetector {
public:
    EventDetector(SensorDriver& driver, 
                  uint32_t debounceMs = SENSOR_DEBOUNCE_MS,
                  uint32_t timeoutMs = INACTIVITY_TIMEOUT_MS);

    void init();
    
    // Call in loop() frequently to process state transitions and timers
    // Returns true if a significant event occurred
    bool update(DetectionEvent& outEvent);

    // Getters
    bool isStableDetected() const { return _stableState; }
    uint32_t getCount() const { return _cumulativeCount; }
    uint32_t getMsSinceLastDetection() const { return millis() - _lastDetectionTime; }
    uint32_t getBounceRejections() const { return _bounceRejectionCount; }

private:
    SensorDriver& _driver;
    uint32_t _debounceMs;
    uint32_t _timeoutMs;

    bool _stableState;          // Current debounced stable target presence (true = metal present)
    bool _lastRawReading;       // Raw reading from previous cycle
    uint32_t _lastRawChangeTime;// Timestamp when raw reading last flipped

    uint32_t _cumulativeCount;
    uint32_t _lastDetectionTime;
    uint32_t _lastClearedTime;
    uint32_t _bounceRejectionCount;

    bool _timeoutReported;      // Prevents spamming TIMEOUT events repeatedly
};
