#pragma once
#include <Arduino.h>
#include "config.h"

class SensorDriver {
public:
    SensorDriver(uint8_t inputPin = SENSOR_INPUT_PIN, uint8_t activeState = SENSOR_ACTIVE_STATE);
    
    // Initialize GPIO modes
    void init();
    
    // Direct hardware read (HIGH or LOW)
    int readRawPin();
    
    // Evaluates if the intended metal target is actively detected based on active state
    bool isTargetPresentRaw();
    
    // Control onboard debug LED
    void setDebugLed(bool state);

private:
    uint8_t _pin;
    uint8_t _activeState;
};
