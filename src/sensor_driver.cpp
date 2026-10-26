#include "sensor_driver.h"

SensorDriver::SensorDriver(uint8_t inputPin, uint8_t activeState)
    : _pin(inputPin), _activeState(activeState) {}

void SensorDriver::init() {
    // Under PC817 optocoupler, Pin 4 (Collector) pulls to GND on detection.
    // INPUT_PULLUP ensures the line is cleanly pulled to 3.3V when no metal is detected.
    pinMode(_pin, INPUT_PULLUP);
    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, LOW);
}

int SensorDriver::readRawPin() {
    return digitalRead(_pin);
}

bool SensorDriver::isTargetPresentRaw() {
    return (readRawPin() == _activeState);
}

void SensorDriver::setDebugLed(bool state) {
    digitalWrite(ONBOARD_LED_PIN, state ? HIGH : LOW);
}
