// =============================================================================
// ESP32 SENSOR 5 — FINISHED PRODUCT (PIPE OUTPUT) PRODUCTION COUNTER
// =============================================================================
// Node ID     : S-05
// Purpose     : Count manufactured pipes passing the flying cutter / output station.
//
// Target Board: ESP32 DevKit V1 (38-Pin / 30-Pin) — board = esp32dev
//
// Hardware Wiring Assumptions:
//   - Sensor Signal Input : GPIO 4 (D4) <-- PC817 Optocoupler Pin 4 (Collector)
//   - Signal Ground       : GND         <-- PC817 Optocoupler Pin 3 (Emitter)
//   - Visual Indicator    : GPIO 2      <-- Built-in Blue LED on ESP32 DevKit V1
//   - Sensor Type         : 12V PNP Normally Open (N.O.) Inductive Proximity Sensor
//   - Signal Isolation    : 4-Pin PC817 Optocoupler (12V plant side isolated from 3.3V ESP32)
//
// Signal Logic (Active-LOW via PC817):
//   - GPIO 4 HIGH (3.3V) : No metal detected (gap between pipes / waiting for next pipe).
//   - GPIO 4 LOW  (0.0V) : Metal detected (pipe is currently present at sensing station).
//
// Initial Counting Behavior:
//   1. IDLE / WAITING: GPIO 4 is HIGH. Counter is armed, waiting for a pipe.
//   2. PIPE ARRIVES  : GPIO 4 transitions HIGH -> LOW.
//                      - Production count increases by 1.
//                      - Built-in LED turns ON.
//                      - Detection event logged to Serial Monitor.
//   3. PIPE REMAINS  : GPIO 4 stays LOW while the pipe passes or pauses in front of sensor.
//                      - Production count remains unchanged (prevents duplicate counting).
//   4. PIPE LEAVES   : GPIO 4 transitions LOW -> HIGH.
//                      - Pipe has cleared the sensing station.
//                      - Counter is re-armed for the next pipe.
//                      - Built-in LED turns OFF.
//
// Physical Limitation Note:
//   This counting logic requires the sensor signal to return to HIGH (inactive)
//   between successive pipes. If pipes pass end-to-end with zero physical gap,
//   the sensor will remain continuously LOW and will count the stream as one pipe.
// =============================================================================

#include <Arduino.h>

// ==============================================================================
// 1. HARDWARE PIN & THRESHOLD CONFIGURATION
// ==============================================================================

// Verify this GPIO pin against your physical wiring before connecting
const uint8_t SENSOR_INPUT_PIN       = 4;      // GPIO 4 (D4) - Optocoupler collector input
const uint8_t INDICATOR_LED_PIN      = 2;      // GPIO 2 (D2) - Built-in indicator LED

// Signal Polarity (Active-LOW with PC817 optocoupler)
const uint8_t SENSOR_DETECTED_STATE  = LOW;    // LOW  = Metal pipe present
const uint8_t SENSOR_IDLE_STATE      = HIGH;   // HIGH = No metal (gap between pipes)

// Visual Indicator Polarity
const uint8_t LED_ON_STATE           = HIGH;   // Turn LED ON when pipe is present
const uint8_t LED_OFF_STATE          = LOW;    // Turn LED OFF when sensor is idle

// Timing & Filtering Configuration
const uint32_t SENSOR_DEBOUNCE_MS    = 50;     // Stable duration (ms) required to confirm state change
const uint32_t HEARTBEAT_INTERVAL_MS = 5000;   // Status heartbeat broadcast interval (ms)
const uint32_t SERIAL_BAUD_RATE      = 115200; // Serial Monitor baud rate

// ==============================================================================
// 2. STATE MACHINE DEFINITIONS & VARIABLES
// ==============================================================================

enum PipeSensorState {
    STATE_UNKNOWN = -1,
    WAITING_FOR_PIPE = 0,   // Sensor is idle (HIGH), counter is armed
    PIPE_PRESENT = 1        // Pipe is currently detected (LOW), count already incremented
};

// Production Metrics
static uint32_t totalPipeCount       = 0;      // Cumulative count of finished pipes
static uint32_t pipeDetectionTime    = 0;      // millis() when current pipe was first detected
static uint32_t pipeClearedTime      = 0;      // millis() when previous pipe cleared

// Debounce & State Tracking
static int      currentRawReading    = HIGH;
static int      lastRawReading       = HIGH;
static uint32_t lastDebounceTime     = 0;

static int             debouncedPinState   = HIGH;
static PipeSensorState currentSensorState  = STATE_UNKNOWN;
static PipeSensorState previousSensorState = STATE_UNKNOWN;

// Heartbeat Tracking
static uint32_t lastHeartbeatTime    = 0;

// ==============================================================================
// 3. HELPER FUNCTIONS
// ==============================================================================

// Convert raw GPIO reading to state
PipeSensorState pinToSensorState(int pinValue) {
    return (pinValue == SENSOR_DETECTED_STATE) ? PIPE_PRESENT : WAITING_FOR_PIPE;
}

// State string representation for human-readable logging
const char* getStateLabel(PipeSensorState state) {
    switch (state) {
        case WAITING_FOR_PIPE: return "WAITING FOR PIPE (Armed)";
        case PIPE_PRESENT:     return "PIPE PRESENT (Detected)";
        default:               return "INITIALIZING";
    }
}

// Display boot banner with system information and operating rules
void printBootBanner() {
    Serial.println();
    Serial.println(F("================================================================================"));
    Serial.println(F("  ESP32 SENSOR 5 — FINISHED PRODUCT (PIPE OUTPUT) PRODUCTION COUNTER"));
    Serial.println(F("  Node Identifier  : S-05"));
    Serial.println(F("  Firmware Version : 1.0.0 (Standalone Counting Test)"));
    Serial.println(F("================================================================================"));
    Serial.printf( "  Sensor Input Pin : GPIO %d (INPUT_PULLUP via PC817 Optocoupler)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Indicator LED    : GPIO %d (Onboard Blue LED: ON when pipe detected)\n", INDICATOR_LED_PIN);
    Serial.println(F("  Signal Logic     : LOW  = Pipe Present (Metal Detected)"));
    Serial.println(F("                     HIGH = Idle / Gap Between Pipes (No Metal)"));
    Serial.printf( "  Debounce Filter  : %u ms stability check\n", SENSOR_DEBOUNCE_MS);
    Serial.printf( "  Heartbeat Period : %u ms\n", HEARTBEAT_INTERVAL_MS);
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println(F("  COUNTING RULES:"));
    Serial.println(F("  1. HIGH -> LOW transition : New pipe detected -> Count increments by 1."));
    Serial.println(F("  2. Sustained LOW          : Pipe is passing   -> Count remains locked."));
    Serial.println(F("  3. LOW -> HIGH transition : Pipe has cleared  -> Counter re-armed for next."));
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println(F("  OPERATIONAL LIMITATION:"));
    Serial.println(F("  Individual pipe counting requires a physical gap between consecutive pipes"));
    Serial.println(F("  so the sensor signal can return to HIGH (idle) between detections."));
    Serial.println(F("================================================================================"));
    Serial.println();
}

// Log a pipe detection event when HIGH -> LOW transition occurs
void logPipeDetected(uint32_t now) {
    totalPipeCount++;
    pipeDetectionTime = now;

    uint32_t gapDurationMs = (pipeClearedTime > 0 && now >= pipeClearedTime)
                                 ? (now - pipeClearedTime)
                                 : 0;

    Serial.println();
    Serial.println(F("********************************************************************************"));
    Serial.printf( "  [PIPE DETECTED] ===> TOTAL PRODUCTION COUNT: %u PIPES <===\n", totalPipeCount);
    Serial.println(F("********************************************************************************"));
    Serial.printf( "  Timestamp        : %u ms (Uptime: %.2f seconds)\n", now, now / 1000.0f);
    Serial.printf( "  Sensor Reading   : GPIO %d is LOW (Metal detected)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Indicator LED    : ON\n");
    if (pipeClearedTime > 0) {
        Serial.printf( "  Gap Since Last   : %u ms (%.2f seconds since previous pipe cleared)\n",
                       gapDurationMs, gapDurationMs / 1000.0f);
    }
    Serial.println(F("  Status           : Pipe is currently in front of sensor. Count is locked."));
    Serial.println(F("********************************************************************************"));
    Serial.println();
}

// Log a pipe cleared event when LOW -> HIGH transition occurs
void logPipeCleared(uint32_t now) {
    pipeClearedTime = now;
    uint32_t transitDurationMs = (pipeDetectionTime > 0 && now >= pipeDetectionTime)
                                     ? (now - pipeDetectionTime)
                                     : 0;

    Serial.println();
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.printf( "  [PIPE CLEARED] Pipe #%u has left the sensing station.\n", totalPipeCount);
    Serial.printf( "  Sensor Reading   : GPIO %d is HIGH (Sensor idle)\n", SENSOR_INPUT_PIN);
    Serial.printf( "  Indicator LED    : OFF\n");
    Serial.printf( "  Transit Duration : %u ms (Pipe was in sensing field for %.2f seconds)\n",
                   transitDurationMs, transitDurationMs / 1000.0f);
    Serial.println(F("  Counter Status   : RE-ARMED. Ready for the next pipe."));
    Serial.println(F("--------------------------------------------------------------------------------"));
    Serial.println();
}

// Periodic non-intrusive status heartbeat
void processHeartbeat(uint32_t now) {
    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;

        Serial.printf( "[Heartbeat %7u ms] Node: S-05 | State: %-26s | Total Pipes: %4u | Pin D4: %-4s | LED: %s\n",
                       now,
                       getStateLabel(currentSensorState),
                       totalPipeCount,
                       (debouncedPinState == LOW) ? "LOW" : "HIGH",
                       (digitalRead(INDICATOR_LED_PIN) == LED_ON_STATE) ? "ON" : "OFF");
    }
}

// ==============================================================================
// 4. ARDUINO SETUP
// ==============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);

    // Initialize GPIO pins
    pinMode(SENSOR_INPUT_PIN, INPUT_PULLUP);
    pinMode(INDICATOR_LED_PIN, OUTPUT);
    digitalWrite(INDICATOR_LED_PIN, LED_OFF_STATE);

    printBootBanner();

    // Read initial hardware state
    int initialReading = digitalRead(SENSOR_INPUT_PIN);
    currentRawReading  = initialReading;
    lastRawReading     = initialReading;
    debouncedPinState  = initialReading;

    currentSensorState  = pinToSensorState(debouncedPinState);
    previousSensorState = currentSensorState;
    lastDebounceTime    = millis();
    lastHeartbeatTime   = millis();

    // If a pipe is already sitting in front of the sensor at startup, count it
    if (currentSensorState == PIPE_PRESENT) {
        digitalWrite(INDICATOR_LED_PIN, LED_ON_STATE);
        logPipeDetected(millis());
    } else {
        digitalWrite(INDICATOR_LED_PIN, LED_OFF_STATE);
        Serial.printf( "  Initial State: %s. Ready to count incoming pipes.\n\n",
                       getStateLabel(currentSensorState));
    }
}

// ==============================================================================
// 5. ARDUINO LOOP
// ==============================================================================

void loop() {
    uint32_t now = millis();
    int rawReading = digitalRead(SENSOR_INPUT_PIN);

    // ── 1. Debounce Processing ───────────────────────────────────────────────
    // If raw pin bounced or changed, reset the debounce timer
    if (rawReading != lastRawReading) {
        lastDebounceTime = now;
        lastRawReading   = rawReading;
    }

    // Only commit the state change when signal is stable for SENSOR_DEBOUNCE_MS
    if ((now - lastDebounceTime) >= SENSOR_DEBOUNCE_MS) {
        if (rawReading != debouncedPinState) {
            debouncedPinState = rawReading;
            PipeSensorState evaluatedState = pinToSensorState(debouncedPinState);

            // ── 2. Edge Detection & Counting Logic ───────────────────────────
            if (evaluatedState != currentSensorState) {
                previousSensorState = currentSensorState;
                currentSensorState  = evaluatedState;

                // Event A: Pipe Arrives (HIGH -> LOW)
                if (currentSensorState == PIPE_PRESENT) {
                    digitalWrite(INDICATOR_LED_PIN, LED_ON_STATE);
                    logPipeDetected(now);
                }
                // Event B: Pipe Leaves (LOW -> HIGH)
                else if (currentSensorState == WAITING_FOR_PIPE) {
                    digitalWrite(INDICATOR_LED_PIN, LED_OFF_STATE);
                    logPipeCleared(now);
                }
            }
        }
    }

    // ── 3. Periodic Status Heartbeat ─────────────────────────────────────────
    processHeartbeat(now);

    // ── 4. Yield briefly to FreeRTOS watchdog ────────────────────────────────
    delay(1);
}
