// =============================================================================
// ESP32 SENSOR 4 — OUTSIDE FILLER WIRE AVAILABILITY & REFILL MONITOR
// =============================================================================
// Node ID   : S-04
// Role      : Outside Filler Wire Monitor
// Material  : OUTSIDE FILLER WIRE
// Purpose   : Monitor presence of outside filler wire at the welding / feed
//             station to detect wire availability and alert on potential refill
//             or feed alignment issues, following the Sensor 2 availability model.
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
// LED Alert : OFF = Normal (Outside filler wire present at sensing station)
//             ON  = Alert  (No wire detected / Possible refill or check feed)
//
// Important Operational Notes:
//   1. Physical Sensing Limitation: The inductive proximity sensor detects metal
//      wire presence within 10–20 mm of its sensing face. A missing detection
//      signal indicates absence at this specific sensing station; it does NOT
//      conclusively prove that the supply spool is empty. The wire may have
//      jumped out of its guide channel or shifted out of alignment.
//   2. Independence: Sensor 4 operates completely independently of Sensor 2 (inside
//      filler wire). Either filler wire may be unavailable while the other remains
//      available.
//   3. No Invented Rules: No grace periods, timeout escalations, or downtime
//      calculations are applied locally.
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
    Serial.printf( "  Monitored Target : %s\n", MONITORED_MATERIAL);
    Serial.printf( "  Firmware Version : %s (Availability & Refill Monitor)\n", FIRMWARE_VERSION);
    Serial.println(F("================================================================================"));
    Serial.printf( "  Sensor Input Pin : GPIO %d (INPUT_PULLUP via PC817 Optocoupler)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Alert Indicator  : GPIO %d (Onboard Blue LED: ON when wire missing)\n", ONBOARD_LED_PIN);
    Serial.println(F("  Signal Polarity  : Active-LOW (PC817 Optocoupler Inversion)"));
    Serial.println(F("                     GPIO 4 = LOW  --> Outside Filler Wire Present (Normal)"));
    Serial.println(F("                     GPIO 4 = HIGH --> No Wire Detected (Alert / Refill Needed)"));
    Serial.printf( "  Debounce Delay   : %u ms\n", SENSOR_DEBOUNCE_MS);
    Serial.printf( "  Heartbeat Period : %u ms\n", HEARTBEAT_INTERVAL_MS);
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println(F("  OPERATIONAL CONSTRAINTS & SENSING NOTES:"));
    Serial.println(F("  - Inductive sensor detects metal presence within 10-20 mm of its face."));
    Serial.println(F("  - A missing detection signal alone indicates wire absence at this sensing point."));
    Serial.println(F("  - It does NOT prove the supply spool is empty until wire alignment is verified."));
    Serial.println(F("  - Sensor 4 operates independently from Sensor 2 (Inside Filler Wire)."));
    Serial.println(F("  - No unconfirmed grace periods or downtime rules are invented locally."));
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
        Serial.printf( "[EVENT #%u] STATUS: OUTSIDE FILLER WIRE DETECTED (FEED CONFIRMED)\n", stateChangeCount);
        Serial.printf( "  Node ID          : %s (%s)\n", NODE_ID, NODE_ROLE_NAME);
        Serial.printf( "  Target Material  : %s\n", MONITORED_MATERIAL);
        Serial.println(F("  Current State    : WIRE PRESENT"));
        Serial.printf( "  Signal Reading   : GPIO %d is LOW (0.0V - PC817 conducting to GND)\n", SENSOR_INPUT_PIN);
        Serial.println(F("  Visual LED       : OFF (Normal operating condition)"));
        if (oldState != WIRE_STATE_UNKNOWN) {
            Serial.printf( "  Previous State   : %s (Lasted: %u ms / %.2f s)\n",
                           getWireStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Operational Note : Outside filler wire confirmed present at sensing station. Feed ready."));
    } else {
        Serial.printf( "[ALERT #%u] WARNING: NO OUTSIDE FILLER WIRE DETECTED!\n", stateChangeCount);
        Serial.printf( "  Node ID          : %s (%s)\n", NODE_ID, NODE_ROLE_NAME);
        Serial.printf( "  Target Material  : %s\n", MONITORED_MATERIAL);
        Serial.println(F("  Current State    : NO WIRE DETECTED / POSSIBLE REFILL REQUIRED"));
        Serial.printf( "  Signal Reading   : GPIO %d is HIGH (3.3V - PC817 open, pulled HIGH)\n", SENSOR_INPUT_PIN);
        Serial.println(F("  Visual LED       : ON (Alert indicator illuminated)"));
        if (oldState != WIRE_STATE_UNKNOWN) {
            Serial.printf( "  Previous State   : %s (Lasted: %u ms / %.2f s)\n",
                           getWireStateString(oldState),
                           previousDurationMs,
                           previousDurationMs / 1000.0f);
        }
        Serial.println(F("  Operational Note : Refill alert flagged. Inspect outside filler wire station."));
        Serial.println(F("  Physical Caveat  : Missing signal indicates wire absent at focal sensing point."));
        Serial.println(F("                     Verify whether spool is depleted or wire slipped out of guide."));
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

        Serial.printf( "[Heartbeat %7u ms] Node: %s | State: %-16s | In-State Duration: %5u ms | Pin D4: %-4s | LED: %-3s | Changes: %u\n",
                       now,
                       NODE_ID,
                       (currentWireState == WIRE_PRESENT) ? "WIRE PRESENT" : "NO WIRE (ALERT)",
                       durationInCurrentState,
                       (debouncedPinState == SENSOR_ACTIVE_STATE) ? "LOW" : "HIGH",
                       (digitalRead(ONBOARD_LED_PIN) == LED_ALERT_STATE) ? "ON" : "OFF",
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

    // Set initial LED state (matches Sensor 1 & 2 alert convention)
    updateVisualIndicator(currentWireState);

    // Report initial detected state clearly at startup
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.printf( "  INITIAL HARDWARE STATE AT BOOT:\n");
    Serial.printf( "  Raw GPIO %d Reading : %s (%s)\n",
                   SENSOR_INPUT_PIN,
                   (initialReading == SENSOR_ACTIVE_STATE) ? "LOW (0.0V)" : "HIGH (3.3V)",
                   (initialReading == SENSOR_ACTIVE_STATE) ? "Metal detected" : "No metal detected");
    Serial.printf( "  Evaluated State     : %s\n", getWireStateString(currentWireState));
    Serial.printf( "  Initial Alert LED   : %s\n", (currentWireState == NO_WIRE_DETECTED) ? "ON (Alert)" : "OFF (Normal)");
    Serial.println(F("--------------------------------------------------------------------------------"));

    // Log the initial state transition once at boot
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
