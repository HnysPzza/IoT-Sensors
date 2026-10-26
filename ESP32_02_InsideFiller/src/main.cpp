// =============================================================================
// ESP32 SENSOR 2 — INSIDE FILLER WIRE AVAILABILITY & REFILL MONITOR
// =============================================================================
// Node ID   : S-02
// Role      : Inside Filler Wire Monitor
// Purpose   : Monitor presence of inside filler wire at the welding / feed
//             station to detect wire availability and alert on potential refill
//             or feed alignment issues, following the Sensor 1 refill model.
//
// Hardware  : ESP32 DevKit V1 (38-Pin / 30-Pin) — board = esp32dev
//             GPIO 4 (D4)   <-- PC817 Optocoupler Pin 4 (Collector)
//             GND           <-- PC817 Optocoupler Pin 3 (Emitter)
//             GPIO 2        <-- Built-in Blue LED (Visual Refill Alert Indicator)
//
// Logic     : GPIO 4 is configured with INPUT_PULLUP.
//             - Wire present:     Sensor active -> PC817 on  -> GPIO 4 = LOW
//             - No wire detected: Sensor idle   -> PC817 off -> GPIO 4 = HIGH
//
// LED Alert : OFF = Normal (Inside filler wire present at sensing station)
//             ON  = Alert  (No wire detected / Possible refill or check feed)
//
// Crucial Physical Sensing Limitation:
//   The inductive proximity sensor detects metal wire presence at its focal
//   sensing point (10–20 mm range). A missing detection signal indicates
//   wire absence at this specific sensing station; it does NOT by itself
//   prove that the supply spool is empty until the physical wire feed and
//   sensor mounting arrangement are verified on the machine.
// =============================================================================

#include <Arduino.h>
#include "config.h"

// ── State Enumeration ────────────────────────────────────────────────────────
enum WireState {
    WIRE_STATE_UNKNOWN = -1,
    WIRE_PRESENT = 0,         // Metal wire detected at sensing station (Normal)
    NO_WIRE_DETECTED = 1      // Metal wire absent (Possible refill / feed check)
};

// ── Runtime State Variables ──────────────────────────────────────────────────
static int       currentRawReading     = HIGH;
static int       lastRawReading        = HIGH;
static uint32_t  lastDebounceTime      = 0;

static int       debouncedPinState     = HIGH;
static WireState currentWireState      = WIRE_STATE_UNKNOWN;
static WireState previousWireState     = WIRE_STATE_UNKNOWN;
static uint32_t  stateStartTime        = 0;

static uint32_t  lastHeartbeatTime     = 0;
static uint32_t  stateChangeCount      = 0;

// ── Helper: Convert WireState to Human-Readable Label ────────────────────────
const char* getWireStateString(WireState state) {
    switch (state) {
        case WIRE_PRESENT:
            return "WIRE PRESENT (Normal Operation)";
        case NO_WIRE_DETECTED:
            return "NO WIRE DETECTED (Refill Alert)";
        default:
            return "INITIALIZING / UNKNOWN";
    }
}

// ── Helper: Convert GPIO Reading to WireState ────────────────────────────────
WireState pinReadingToWireState(int pinValue) {
    return (pinValue == SENSOR_ACTIVE_STATE) ? WIRE_PRESENT : NO_WIRE_DETECTED;
}

// ── Print System Header / Boot Information ───────────────────────────────────
void printBootBanner() {
    Serial.println();
    Serial.println(F("================================================================================"));
    Serial.printf( "  %s — ESP32 SENSOR NODE [%s]\n", NODE_ROLE_NAME, NODE_ID);
    Serial.printf( "  Firmware Version : %s (Refill Notification & Availability Monitor)\n", FIRMWARE_VERSION);
    Serial.println(F("================================================================================"));
    Serial.printf( "  Sensor Input Pin : GPIO %d (INPUT_PULLUP via PC817 Optocoupler)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Alert Indicator  : GPIO %d (Onboard Blue LED: ON when wire missing)\n", ONBOARD_LED_PIN);
    Serial.println(F("  Signal Polarity  : Active-LOW (PC817 Optocoupler Inversion)"));
    Serial.println(F("                     GPIO 4 = LOW  --> Inside Filler Wire Present (Normal)"));
    Serial.println(F("                     GPIO 4 = HIGH --> No Wire Detected (Alert / Refill Needed)"));
    Serial.printf( "  Debounce Delay   : %u ms\n", SENSOR_DEBOUNCE_MS);
    Serial.printf( "  Heartbeat Period : %u ms\n", HEARTBEAT_INTERVAL_MS);
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println(F("  PHYSICAL SENSING DISTINCTION:"));
    Serial.println(F("  - The inductive proximity sensor detects metal presence within 10-20mm."));
    Serial.println(F("  - A missing detection signal alone indicates wire absence at this sensing point."));
    Serial.println(F("  - It does NOT prove the supply spool is empty until wire alignment is verified."));
    Serial.println(F("  - No unconfirmed grace period or downtime calculation is invented locally."));
    Serial.println(F("================================================================================"));
    Serial.println();
}

// ── Update Hardware Visual Indicator ─────────────────────────────────────────
void updateVisualIndicator(WireState state) {
    if (state == NO_WIRE_DETECTED) {
        digitalWrite(ONBOARD_LED_PIN, LED_ALERT_STATE);   // LED ON = Alert / Missing Wire
    } else {
        digitalWrite(ONBOARD_LED_PIN, LED_NORMAL_STATE);  // LED OFF = Normal / Wire Present
    }
}

// ── Report State Change Event ────────────────────────────────────────────────
// Prevents log spamming: only called when the debounced state changes.
void logStateTransition(WireState newState, WireState oldState, uint32_t transitionTime) {
    stateChangeCount++;
    uint32_t previousDurationMs = (transitionTime >= stateStartTime && stateStartTime > 0)
                                      ? (transitionTime - stateStartTime)
                                      : 0;

    Serial.println();
    Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
    if (newState == WIRE_PRESENT) {
        Serial.printf( "[EVENT #%u] STATUS: INSIDE FILLER WIRE DETECTED (FEED CONFIRMED)\n", stateChangeCount);
        Serial.printf( "  Node ID          : %s (%s)\n", NODE_ID, NODE_ROLE_NAME);
        Serial.println(F("  Current State    : WIRE PRESENT"));
        Serial.printf( "  Signal Reading   : GPIO %d is LOW (0.0V - PC817 conducting to GND)\n", SENSOR_INPUT_PIN);
        Serial.println(F("  Visual LED       : OFF (Normal operating condition)"));
        if (oldState != WIRE_STATE_UNKNOWN) {
            Serial.printf( "  Previous State   : %s (Lasted: %u ms / %.2f s)\n",
                           getWireStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Operational Note : Filler wire confirmed present at sensing station. Feed ready."));
    } else {
        Serial.printf( "[ALERT #%u] WARNING: NO INSIDE FILLER WIRE DETECTED!\n", stateChangeCount);
        Serial.printf( "  Node ID          : %s (%s)\n", NODE_ID, NODE_ROLE_NAME);
        Serial.println(F("  Current State    : NO WIRE DETECTED / POSSIBLE REFILL REQUIRED"));
        Serial.printf( "  Signal Reading   : GPIO %d is HIGH (3.3V - PC817 open, pulled HIGH)\n", SENSOR_INPUT_PIN);
        Serial.println(F("  Visual LED       : ON (Alert indicator illuminated)"));
        if (oldState != WIRE_STATE_UNKNOWN) {
            Serial.printf( "  Previous State   : %s (Lasted: %u ms / %.2f s)\n",
                           getWireStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Operational Note : Refill alert flagged. Inspect inside filler wire station."));
        Serial.println(F("  Physical Caveat  : Missing signal indicates wire absent at focal point. Verify"));
        Serial.println(F("                     whether spool is depleted or wire slipped out of guide channel."));
    }
    Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
    Serial.println();
}

// ── Periodic Heartbeat Summary ───────────────────────────────────────────────
// Emits regular liveness summaries without cluttering logs with redundant state transitions.
void processHeartbeat(uint32_t now) {
    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        uint32_t durationInCurrentState = now - stateStartTime;

        Serial.printf( "[Heartbeat %7u ms] Node: %s | State: %-16s | In-State Duration: %5u ms | LED: %s | Changes: %u\n",
                       now,
                       NODE_ID,
                       (currentWireState == WIRE_PRESENT) ? "WIRE PRESENT" : "NO WIRE (ALERT)",
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

    currentWireState   = pinReadingToWireState(debouncedPinState);
    previousWireState  = WIRE_STATE_UNKNOWN;
    stateStartTime     = millis();
    lastHeartbeatTime  = millis();

    // Set initial LED state (matches Sensor 1 alert convention)
    updateVisualIndicator(currentWireState);

    // Log the initial state once at boot
    logStateTransition(currentWireState, previousWireState, stateStartTime);
    previousWireState = currentWireState;
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
            WireState evaluatedState = pinReadingToWireState(debouncedPinState);

            // ── 2. State Change Detection (Prevents Repetitive Logging) ─────
            if (evaluatedState != currentWireState) {
                previousWireState = currentWireState;
                currentWireState  = evaluatedState;

                // Update visual LED indicator
                updateVisualIndicator(currentWireState);

                // Output event details to Serial Monitor
                logStateTransition(currentWireState, previousWireState, now);

                stateStartTime = now;
            }
        }
    }

    // ── 3. Periodic Status Heartbeat ─────────────────────────────────────────
    processHeartbeat(now);

    // ── 4. Yield briefly to FreeRTOS watchdog ────────────────────────────────
    delay(1);
}
