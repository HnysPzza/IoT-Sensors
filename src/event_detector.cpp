#include "event_detector.h"

EventDetector::EventDetector(SensorDriver& driver, uint32_t debounceMs, uint32_t timeoutMs)
    : _driver(driver),
      _debounceMs(debounceMs),
      _timeoutMs(timeoutMs),
      _stableState(false),
      _lastRawReading(false),
      _lastRawChangeTime(0),
      _cumulativeCount(0),
      _lastDetectionTime(0),
      _lastClearedTime(0),
      _bounceRejectionCount(0),
      _timeoutReported(false) {}

void EventDetector::init() {
    _driver.init();
    _stableState = _driver.isTargetPresentRaw();
    _lastRawReading = _stableState;
    _lastRawChangeTime = millis();
    _lastDetectionTime = millis();
    _lastClearedTime = millis();
    _timeoutReported = false;
    _driver.setDebugLed(_stableState);
}

bool EventDetector::update(DetectionEvent& outEvent) {
    uint32_t now = millis();
    bool rawState = _driver.isTargetPresentRaw();

    // 1. Debounce state machine
    if (rawState != _lastRawReading) {
        // Raw pin flipped state, restart debounce timer
        _lastRawChangeTime = now;
        _lastRawReading = rawState;
    }

    // Check if raw state has remained stable for longer than SENSOR_DEBOUNCE_MS
    if ((now - _lastRawChangeTime) >= _debounceMs) {
        if (rawState != _stableState) {
            // Confirmed state transition!
            _stableState = rawState;
            _driver.setDebugLed(_stableState);

            if (_stableState) {
                // Transition: Inactive -> Active (Target Entered / Detected)
                uint32_t interval = now - _lastDetectionTime;
                _lastDetectionTime = now;
                _timeoutReported = false; // Reset inactivity watchdog

                // For Node 1, 2, 3, 4: increment on entry
                if (!IS_PRODUCTION_COUNTER) {
                    _cumulativeCount++;
                }

                outEvent.type = EVENT_DETECTED;
                outEvent.typeName = "DETECTED";
                outEvent.timestampMs = now;
                outEvent.cumulativeCount = _cumulativeCount;
                outEvent.intervalMs = interval;
                return true;
            } else {
                // Transition: Active -> Inactive (Target Cleared / Removed)
                uint32_t presentDuration = now - _lastDetectionTime;
                _lastClearedTime = now;

                // For Node 5 (Flying Cutter Production Counter):
                // Section 6.4: Increment count on FALLING EDGE only (pipe lifted away by crane)
                if (IS_PRODUCTION_COUNTER) {
                    _cumulativeCount++;
                }

                outEvent.type = EVENT_NOT_DETECTED;
                outEvent.typeName = "NOT_DETECTED";
                outEvent.timestampMs = now;
                outEvent.cumulativeCount = _cumulativeCount;
                outEvent.intervalMs = presentDuration;
                return true;
            }
        }
    } else if (rawState != _stableState) {
        // Transition attempted but hasn't settled yet (counting noise/bounce)
        _bounceRejectionCount++;
    }

    // 2. Inactivity Watchdog (Node 3 Downtime Master)
    // Section 6.3: If no valid detection transition occurs within INACTIVITY_TIMEOUT_MS
    if (IS_DOWNTIME_MASTER && !_timeoutReported) {
        if ((now - _lastDetectionTime) >= _timeoutMs) {
            _timeoutReported = true; // Report once per timeout period

            outEvent.type = EVENT_TIMEOUT;
            outEvent.typeName = "TIMEOUT";
            outEvent.timestampMs = now;
            outEvent.cumulativeCount = _cumulativeCount;
            outEvent.intervalMs = (now - _lastDetectionTime);
            return true;
        }
    }

    return false;
}
