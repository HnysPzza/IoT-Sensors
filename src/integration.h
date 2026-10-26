#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "event_detector.h"

// ==============================================================================
// INTEGRATION INTERFACE CONTRACT (Section 7 of Master Plan)
// Prepares serialized JSON payloads matching Express.js backend expectations
// ==============================================================================

class IntegrationInterface {
public:
    // Serializes a real-time detection event to JSON
    // Output matches Section 7.1 SENSOR_EVENT specification
    static size_t serializeSensorEvent(const DetectionEvent& event, char* outputBuffer, size_t bufferSize) {
        StaticJsonDocument<256> doc;
        doc["node_id"] = NODE_ID;
        doc["event_type"] = event.typeName;
        doc["timestamp"] = event.timestampMs;
        doc["count"] = event.cumulativeCount;
        doc["interval_ms"] = event.intervalMs;
        doc["sensor_state"] = (event.type == EVENT_FAULT) ? "FAULT" : "OK";
        
        return serializeJson(doc, outputBuffer, bufferSize);
    }

    // Serializes periodic node health & diagnostic status to JSON
    // Output matches Section 7.1 SENSOR_STATUS specification
    static size_t serializeSensorStatus(const EventDetector& detector, char* outputBuffer, size_t bufferSize) {
        StaticJsonDocument<256> doc;
        doc["node_id"] = NODE_ID;
        doc["status"] = "OK";
        doc["uptime_ms"] = millis();
        doc["last_seen_ms"] = detector.getMsSinceLastDetection();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["bounce_rejections"] = detector.getBounceRejections();
        
        return serializeJson(doc, outputBuffer, bufferSize);
    }
};
