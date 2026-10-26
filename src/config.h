#pragma once
#include <Arduino.h>

// ==============================================================================
// 1. NODE IDENTITY & ROLE
// ==============================================================================
// Node identifiers: S-01, S-02, S-03, S-04, S-05
#define NODE_ID                     "S-03"
#define FIRMWARE_VERSION            "1.0.0"

// Role selection (Set exactly one to true or combine per node requirements)
#define IS_DOWNTIME_MASTER          true   // True for Node 3 (Main welding drive inactivity timeout)
#define IS_PRODUCTION_COUNTER       false  // True for Node 5 (Falling-edge pipe output counter)

// ==============================================================================
// 2. HARDWARE GPIO PIN CONFIGURATION (ESP32 DevKit V1 38-Pin)
// ==============================================================================
#define SENSOR_INPUT_PIN            4      // GPIO 4 (Pin D4) - Safe general input
#define ONBOARD_LED_PIN             2      // GPIO 2 (Onboard blue LED for visual debug)

// Active state of sensor input:
// Under PC817 optocoupler, transistor pulls GPIO to GND when metal is detected -> Active LOW
#define SENSOR_ACTIVE_STATE         LOW    

// ==============================================================================
// 3. SIGNAL FILTERING & TIMING THRESHOLDS
// ==============================================================================
#define SENSOR_DEBOUNCE_MS          50     // Stable duration required to confirm state change (ms)
#define INACTIVITY_TIMEOUT_MS       30000  // Inactivity timeout threshold for Node 3 (30 seconds)
#define HEARTBEAT_INTERVAL_MS       5000   // Periodic status report interval (ms)

// ==============================================================================
// 4. DIAGNOSTICS & SERIAL CONFIGURATION
// ==============================================================================
#define SERIAL_BAUD_RATE            115200 // Serial Monitor baud rate
#define ENABLE_VERBOSE_LOG          true   // Set to true for development logging
