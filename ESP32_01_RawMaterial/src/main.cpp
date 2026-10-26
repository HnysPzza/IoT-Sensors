// =============================================================================
// ESP32 SENSOR 1 — RAW MATERIAL COIL AVAILABILITY MONITOR
// =============================================================================
// Node ID   : S-01
// Purpose   : Monitor presence of metal raw-material coil at the uncoiler / feed
//             station to detect coil availability and alert on potential refill needs.
//
// Hardware  : ESP32 DevKit V1 (38-Pin)
//             GPIO 4 (D4)   <-- PC817 Pin 4 (Collector)
//             GND           <-- PC817 Pin 3 (Emitter)
//             GPIO 2        <-- Built-in Blue LED (Visual Refill Alert Indicator)
//
// Logic     : GPIO 4 is configured with INPUT_PULLUP.
//             - Metal coil present:  Sensor active -> PC817 on  -> GPIO 4 = LOW
//             - No coil detected:    Sensor idle   -> PC817 off -> GPIO 4 = HIGH
//
// LED Alert : OFF = Normal (Coil present at sensing station)
//             ON  = Alert (No coil detected / Possible refill required)
//
// Notes     : The inductive proximity sensor detects metal presence at the
//             designated sensing location, not the continuous remaining quantity
//             or weight of the coil. Missing detection represents an alert
//             condition indicating NO COIL DETECTED or POSSIBLE REFILL REQUIRED.
// =============================================================================

#include <Arduino.h>
#include "config.h"

// ── State Enumeration ────────────────────────────────────────────────────────
enum CoilState {
    COIL_STATE_UNKNOWN = -1,
    COIL_PRESENT = 0,         // Metal detected at sensing station
    NO_COIL_DETECTED = 1      // Metal absent (Possible refill required)
};

// ── Runtime State Variables ──────────────────────────────────────────────────
static int       currentRawReading     = HIGH;
static int       lastRawReading        = HIGH;
static uint32_t  lastDebounceTime      = 0;

static int       debouncedPinState     = HIGH;
static CoilState currentCoilState      = COIL_STATE_UNKNOWN;
static CoilState previousCoilState     = COIL_STATE_UNKNOWN;
static uint32_t  stateStartTime        = 0;

static uint32_t  lastHeartbeatTime     = 0;
static uint32_t  stateChangeCount      = 0;

// ── Helper: Convert CoilState to String ───────────────────────────────────────
const char* getCoilStateString(CoilState state) {
    switch (state) {
        case COIL_PRESENT:
            return "COIL PRESENT (Material Detected)";
        case NO_COIL_DETECTED:
            return "NO COIL DETECTED (Refill Alert)";
        default:
            return "INITIALIZING / UNKNOWN";
    }
}

// ── Helper: Convert GPIO Reading to CoilState ────────────────────────────────
CoilState pinReadingToCoilState(int pinValue) {
    return (pinValue == SENSOR_ACTIVE_STATE) ? COIL_PRESENT : NO_COIL_DETECTED;
}

// ── Print System Header / Boot Information ───────────────────────────────────
void printBootBanner() {
    Serial.println();
    Serial.println(F("================================================================================"));
    Serial.printf( "  %s — ESP32 SENSOR NODE [%s]\n", NODE_ROLE_NAME, NODE_ID);
    Serial.printf( "  Firmware Version : %s\n", FIRMWARE_VERSION);
    Serial.println(F("================================================================================"));
    Serial.printf( "  Sensor Input Pin : GPIO %d (INPUT_PULLUP via PC817 Optocoupler)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Alert Indicator  : GPIO %d (Onboard LED)\n", ONBOARD_LED_PIN);
    Serial.println(F("  Signal Logic     : LOW  = Metal Detected (Coil Present)"));
    Serial.println(F("                     HIGH = No Metal (No Coil Detected / Refill Alert)"));
    Serial.printf( "  Debounce Delay   : %u ms\n", SENSOR_DEBOUNCE_MS);
    Serial.printf( "  Heartbeat Period : %u ms\n", HEARTBEAT_INTERVAL_MS);
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println(F("  IMPORTANT OPERATIONAL NOTE:"));
    Serial.println(F("  The proximity sensor detects metal presence at the focal sensing point."));
    Serial.println(F("  A missing-detection state indicates 'NO COIL DETECTED / POSSIBLE REFILL'"));
    Serial.println(F("  and does not represent a continuous metric of remaining material volume."));
    Serial.println(F("================================================================================"));
    Serial.println();
}

// ── Report State Change Event ────────────────────────────────────────────────
// Avoids repeated logging. Only called when the debounced state changes.
void logStateTransition(CoilState newState, CoilState oldState, uint32_t transitionTime) {
    stateChangeCount++;
    uint32_t previousDurationMs = (transitionTime >= stateStartTime && stateStartTime > 0)
                                      ? (transitionTime - stateStartTime)
                                      : 0;

    Serial.println();
    Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
    if (newState == COIL_PRESENT) {
        Serial.printf( "[EVENT #%u] STATUS: RAW MATERIAL COIL DETECTED\n", stateChangeCount);
        Serial.println(F("  Current State : COIL PRESENT"));
        Serial.println(F("  Signal        : GPIO 4 is LOW (Metal detected at sensing station)"));
        Serial.println(F("  Visual LED    : OFF (Normal operating condition)"));
        if (oldState != COIL_STATE_UNKNOWN) {
            Serial.printf( "  Previous State: %s (Lasted: %u ms / %.1f s)\n",
                           getCoilStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Action Taken  : Material feed confirmed ready. Normal processing allowed."));
    } else {
        Serial.printf( "[ALERT #%u] WARNING: NO RAW MATERIAL COIL DETECTED!\n", stateChangeCount);
        Serial.println(F("  Current State : NO COIL DETECTED / POSSIBLE REFILL REQUIRED"));
        Serial.println(F("  Signal        : GPIO 4 is HIGH (No metal detected at sensing station)"));
        Serial.println(F("  Visual LED    : ON (Alert indicator illuminated)"));
        if (oldState != COIL_STATE_UNKNOWN) {
            Serial.printf( "  Previous State: %s (Lasted: %u ms / %.1f s)\n",
                           getCoilStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Action Taken  : Alert flagged. Inspect coil feed station and replenish raw stock."));
    }
    Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
    Serial.println();
}

// ── Update Hardware Visual Indicator ─────────────────────────────────────────
void updateVisualIndicator(CoilState state) {
    if (state == NO_COIL_DETECTED) {
        digitalWrite(ONBOARD_LED_PIN, LED_ALERT_STATE);   // LED ON = Alert / Missing Coil
    } else {
        digitalWrite(ONBOARD_LED_PIN, LED_NORMAL_STATE);  // LED OFF = Normal / Coil Present
    }
}

// ── Periodic Heartbeat Summary ───────────────────────────────────────────────
// Provides reassurance that system is alive without cluttering logs with redundant state lines.
void processHeartbeat(uint32_t now) {
    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        uint32_t durationInCurrentState = now - stateStartTime;

        Serial.printf( "[Heartbeat %7u ms] Node: %s | State: %-16s | In-State Duration: %5u ms | LED: %s | Changes: %u\n",
                       now,
                       NODE_ID,
                       (currentCoilState == COIL_PRESENT) ? "COIL PRESENT" : "NO COIL (ALERT)",
                       durationInCurrentState,
                       (digitalRead(ONBOARD_LED_PIN) == LED_ALERT_STATE) ? "ON (ALERT)" : "OFF",
                       stateChangeCount);
    }
}

// =============================================================================
// ARDUINO SETUP
// =============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);

    // Initialize GPIOs
    pinMode(SENSOR_INPUT_PIN, INPUT_PULLUP);
    pinMode(ONBOARD_LED_PIN, OUTPUT);

    printBootBanner();

    // Read initial hardware state
    int initialReading = digitalRead(SENSOR_INPUT_PIN);
    currentRawReading  = initialReading;
    lastRawReading     = initialReading;
    debouncedPinState  = initialReading;

    currentCoilState   = pinReadingToCoilState(debouncedPinState);
    previousCoilState  = COIL_STATE_UNKNOWN;
    stateStartTime     = millis();
    lastHeartbeatTime  = millis();

    // Set initial LED state
    updateVisualIndicator(currentCoilState);

    // Log the initial state once at boot
    logStateTransition(currentCoilState, previousCoilState, stateStartTime);
    previousCoilState = currentCoilState;
}

// =============================================================================
// ARDUINO LOOP
// =============================================================================
void loop() {
    uint32_t now = millis();
    int rawReading = digitalRead(SENSOR_INPUT_PIN);

    // ── 1. Debounce Processing ───────────────────────────────────────────────
    // If raw pin bounced or changed, restart debounce countdown timer
    if (rawReading != lastRawReading) {
        lastDebounceTime = now;
        lastRawReading   = rawReading;
    }

    // If reading has remained steady longer than debounce threshold, confirm state
    if ((now - lastDebounceTime) >= SENSOR_DEBOUNCE_MS) {
        if (rawReading != debouncedPinState) {
            debouncedPinState = rawReading;
            CoilState evaluatedState = pinReadingToCoilState(debouncedPinState);

            // ── 2. State Change Detection (Prevents Repetitive Logging) ─────
            if (evaluatedState != currentCoilState) {
                previousCoilState = currentCoilState;
                currentCoilState  = evaluatedState;

                // Update visual LED indicator
                updateVisualIndicator(currentCoilState);

                // Output event details to Serial Monitor
                logStateTransition(currentCoilState, previousCoilState, now);

                stateStartTime = now;
            }
        }
    }

    // ── 3. Periodic Non-Intrusive Heartbeat ──────────────────────────────────
    processHeartbeat(now);

    // ── 4. Yield briefly to FreeRTOS watchdog ────────────────────────────────
    delay(1);
}
