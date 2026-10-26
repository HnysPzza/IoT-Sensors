#pragma once
#include <Arduino.h>

// ==============================================================================
// 1. NODE IDENTITY & PURPOSE
// ==============================================================================
#define NODE_ID                     "S-01"
#define NODE_ROLE_NAME              "Raw Material Coil Monitor"
#define FIRMWARE_VERSION            "1.0.0"

// ==============================================================================
// 2. HARDWARE GPIO PIN CONFIGURATION (ESP32 DevKit V1 38-Pin)
// ==============================================================================
// Sensor Signal Input (Collector of PC817 optocoupler)
#define SENSOR_INPUT_PIN            4      // GPIO 4 (Pin D4)

// Onboard visual indicator (Blue LED on ESP32 DevKit V1)
#define ONBOARD_LED_PIN             2      // GPIO 2

// Optocoupler Inverted Logic Polarity:
// - Metal present at proximity sensor -> Sensor 12V output ON -> PC817 LED ON
// - PC817 phototransistor conducts -> GPIO 4 pulled to GND (LOW)
// - No metal at proximity sensor      -> Sensor output OFF     -> PC817 LED OFF
// - Internal pullup holds GPIO 4 at 3.3V (HIGH)
#define SENSOR_ACTIVE_STATE         LOW    // LOW = Metal detected (Coil present)

// Visual Indicator Logic:
// - Refill Alert / No Coil : LED ON (alert operators)
// - Coil Present (Normal)  : LED OFF
#define LED_ALERT_STATE             HIGH   // Turn LED ON when alert triggered
#define LED_NORMAL_STATE            LOW    // Turn LED OFF when material present

// ==============================================================================
// 3. SIGNAL FILTERING & TIMING THRESHOLDS
// ==============================================================================
#define SENSOR_DEBOUNCE_MS          50     // Stable duration (ms) to confirm state change
#define HEARTBEAT_INTERVAL_MS       5000   // Periodic status summary interval (ms)

// ==============================================================================
// 4. SERIAL MONITOR CONFIGURATION
// ==============================================================================
#define SERIAL_BAUD_RATE            115200 // Serial Monitor baud rate
