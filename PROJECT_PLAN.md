# MASTER PROJECT PLAN & SPECIFICATION
## IoT-Based Pipe Manufacturing Machine Monitoring System
**Client:** Petro Hydro Pipe Corp. (Dunggoan, Danao City, Cebu, Philippines)  
**Academic Institution:** CTU – Naga Extension Campus | BSIT 3B  
**Role Responsibility:** ESP32 Microcontroller Firmware & Inductive Proximity Sensor Subsystem  
**Document Status:** Master Reference & Planning Document (Pre-Implementation Baseline)

---

## 1. Project Overview & Team Structure

The **IoT-Based Pipe Manufacturing Machine Monitoring System** is an automated industrial monitoring platform replacing manual handwritten logbooks at Petro Hydro Pipe Corp. The system tracks machine downtime events, production events, production output counts, and production loss records in real time.

### Team Responsibilities Matrix

| Component | Technology Stack | Responsible Party | Scope Boundary |
|:---|:---|:---|:---|
| **Frontend** | React.js + Vanilla CSS | Partner | Web dashboard, charts, UI |
| **Backend** | Express.js + Node.js | Partner (Karl) | Business logic, downtime classification, APIs |
| **Database** | Supabase (PostgreSQL) | Partner | Persistent storage, history, schemas |
| **Auth / Email** | Resend | Partner | User authentication & alerts |
| **IoT Firmware** | C/C++ via PlatformIO + Arduino | **MY RESPONSIBILITY** | Event detection, debouncing, counts, diagnostics |
| **Microcontroller**| ESP32 DevKit V1 (38-pin) | **MY RESPONSIBILITY** | Hardware config, GPIO, WiFi, HTTP payload delivery |
| **Sensors** | Inductive Proximity Sensors (x5) | **MY RESPONSIBILITY** | Signal conditioning, isolation, calibration |
| **Plant Network** | MikroTik hAP lite, Cat5e LAN (25m) | Shared / Client | Factory network infrastructure |

---

## 2. Scope Definition

```
┌──────────────────────────────────────────────┬──────────────────────────────────────────────┐
│             ✔ MY RESPONSIBILITY              │           ✖ NOT MY RESPONSIBILITY            │
├──────────────────────────────────────────────┼──────────────────────────────────────────────┤
│ • Program ESP32 microcontrollers             │ • Redesign overall system architecture       │
│ • Configure & read inductive proximity sensor│ • Redesign web dashboard or UI               │
│ • Detect & filter sensor events reliably     │ • Redesign database schema                   │
│ • Timestamp & count detection events         │ • Redesign backend API                       │
│ • Detect sensor faults or abnormal states    │ • Change capstone project objectives         │
│ • Expose clean, documented interface to API  │ • Invent additional system features          │
│ • Test sensor nodes independently (Stages 1-5│ • Redesign or physically modify the machine  │
│ • Document sensor module integration specs   │ • Replace partner's existing work            │
│ • Deliver working sensor nodes for integratio│ • Choose final high-level machine-state logic│
└──────────────────────────────────────────────┴──────────────────────────────────────────────┘
```

---

## 3. Development Environment & Project Architecture

### Toolchain

* **Primary IDE:** Antigravity IDE
* **ESP32 Build System:** PlatformIO Core / CLI
* **Framework:** Arduino Framework for ESP32 (`framework = arduino`)
* **Language:** C / C++
* **Target Board:** ESP32 DevKit V1 (38-pin variant, `board = esp32dev`, `platform = espressif32`)

### Recommended PlatformIO Project Structure

```text
IoT/
├── platformio.ini         # Environment definition, board, monitor_speed, build flags
├── PROJECT_PLAN.MD        # Master project plan and technical specifications
├── include/               # Optional shared headers
├── lib/                   # Project-specific custom libraries (if needed)
└── src/
    ├── main.cpp           # Setup, main loop, system orchestrator
    ├── config.h           # Centralized hardware pins, WiFi, API, thresholds
    ├── sensor_driver.h    # Low-level hardware abstraction & GPIO reading
    ├── sensor_driver.cpp  # Pin mode setup, raw signal reading
    ├── event_detector.h   # State machine, edge detection, debounce, timers
    ├── event_detector.cpp # Filtering, cycle timing, falling/rising edge logic
    ├── diagnostics.h      # Health monitoring, fault states, logging
    ├── diagnostics.cpp    # Serial diagnostics output & heartbeat metrics
    └── integration.h      # Payload serialization & HTTP POST transmission
```

### Base `platformio.ini` Configuration

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_flags = 
    -DCORE_DEBUG_LEVEL=3
lib_deps =
    bblanchon/ArduinoJson @ ^6.21.3
```

---

## 4. Hardware Specifications & Signal Conditioning

### 4.1 Microcontroller & Pinout
* **Board:** ESP32 DevKit V1 — **38-Pin Variant** (19 pins per side).
* **Operating Logic:** 3.3V logic level (GPIO absolute maximum rating: 3.6V).
* **Target Sensor GPIO:** **GPIO 4 (Pin D4)**.
  * *Location on 38-Pin board:* Right-hand header, 7 pins up from bottom (between D16 and D0).
  * Safe general-purpose pin: Not a strapping pin, no boot-mode interference, supports interrupts and internal pull-ups.

### 4.2 Sensor Specifications
* **Type:** Industrial Inductive Proximity Sensor
* **Housing / Thread:** M30
* **Protection Rating:** IP67 (dust-tight, water-resistant)
* **Output Configuration:** PNP, Normally Open (N.O.)
* **Supply Voltage:** 10–30V DC (Powered via 12V DC 5A adapter)
* **Detection Distance:** 10–20 mm specified range
* **Output Signal Behavior:** When metal is detected, output (Black wire) switches to $V_{supply} - V_{sat}$ ($\approx 10\text{V} - 11\text{V}$). When no metal is detected, output floats.

### 4.3 Signal Conditioning: 4-Pin PC817 Optocoupler (CONFIRMED PRIMARY METHOD)

To protect the ESP32 from high industrial voltages, inductive motor spikes, and welding EMI in the pipe manufacturing facility, **galvanic isolation via a PC817 optocoupler** is employed.

```
       12V SENSOR CIRCUIT                            ESP32 3.3V CIRCUIT
  (Plant / Machine Side)                           (Microcontroller Side)
                                ┌───────────────┐
Sensor Black (Signal) ──[3.3kΩ]─┤1(Anode)  (Coll)4├───── ESP32 GPIO 4 (D4)
                                │     [LED]     │         (INPUT_PULLUP)
Sensor Blue (12V GND) ──────────┤2(Cath)   (Emit)3├───── ESP32 GND
                                └───────────────┘
                                  PC817 OPTO
```

#### PC817 Pin Connections
* **Pin 1 (Anode, marked with circular dot):** Connects to **Sensor Black Wire (Signal)** in series with a **3.3 kΩ resistor** (limits LED current to $\approx 3.3\text{ mA}$).
* **Pin 2 (Cathode):** Connects to **Sensor Blue Wire** & **12V Adapter Negative (-)**.
* **Pin 3 (Emitter):** Connects to **ESP32 GND**.
* **Pin 4 (Collector):** Connects to **ESP32 GPIO 4 (D4)**. Internal pullup configured (`pinMode(4, INPUT_PULLUP)`).

#### Logic Polarity Under Optocoupler
The optocoupler collector-emitter transistor inverts the raw signal:
* **No metal detected:** Sensor output OFF $\rightarrow$ Opto LED OFF $\rightarrow$ GPIO pulled HIGH by internal resistor (**3.3V / HIGH**).
* **Metal detected:** Sensor output ON (12V) $\rightarrow$ Opto LED ON $\rightarrow$ Phototransistor conducts to GND (**0.0V / LOW**).
* **Config Constant:** `#define SENSOR_ACTIVE_STATE LOW`

#### Isolation Rule
* **True Galvanic Isolation:** 12V supply ground and ESP32 USB ground are completely isolated. **Do not bridge 12V GND to ESP32 GND.**

### 4.4 Alternative / Backup Method: Resistor Voltage Divider
If optocoupler is bypassed for bench testing:
* **R1:** 10 kΩ (between Sensor Black and GPIO)
* **R2:** 3.3 kΩ (between GPIO and GND)
* **Output Voltage:** $12\text{V} \times \frac{3.3}{10 + 3.3} \approx 2.98\text{V}$ (Safe for 3.3V GPIO)
* **Logic Polarity:** Active HIGH (`#define SENSOR_ACTIVE_STATE HIGH`)
* **Grounding:** Requires shared common ground between 12V PSU (-) and ESP32 GND.

---

## 5. The Five Sensor Nodes: Roles & Detection Logic

Each of the 5 nodes is an independent physical ESP32 + sensor unit running the identical modular firmware, differentiated solely by configuration constants in `config.h`.

```
┌──────┬───────┬──────────────────────────┬─────────────────────────────┬───────────────────────┐
│ Node │ Label │ Location                 │ Detection Purpose           │ System Classification │
├──────┼───────┼──────────────────────────┼─────────────────────────────┼───────────────────────┤
│ 1    │ S-01  │ Raw Materials / Coil Area│ Coil joint & raw stock feed │ PROCESS EVENT         │
│ 2    │ S-02  │ Inside Filler Wire Area  │ Inside filler wire feed     │ PROCESS EVENT         │
│ 3    │ S-03  │ Main Machine Drive/Weld  │ Continuous cycling monitor  │ MASTER DOWNTIME       │
│ 4    │ S-04  │ Outside Filler Wire Area │ Outside filler wire feed    │ PROCESS EVENT         │
│ 5    │ S-05  │ Flying Cutter / Pipe Out │ Finished pipe count         │ PRODUCTION COUNT      │
└──────┴───────┴──────────────────────────┴─────────────────────────────┴───────────────────────┘
```

### 5.1 The Industrial Metal Detection Constraint
* **The Problem:** The machine body is entirely fabricated from metal. An inductive sensor registers any metal within its 10–20mm field.
* **Firmware Rule:** Continuous static metal detection $\neq$ Machine Running.
* **Resolution:** Firmware must detect **dynamic state transitions** (cyclic pulses from moving teeth, rotating cams, or passing seams) rather than static presence.

### 5.2 Node 3: Master Downtime Detection Logic (Inactivity Watchdog)
* Continuously expects state transitions within a specified window: `INACTIVITY_TIMEOUT_MS` (e.g., 30,000 ms).
* **Logic Flow:**
  1. Cyclic target detected $\rightarrow$ Record detection event + timestamp $\rightarrow$ Reset inactivity timer.
  2. If inactivity timer exceeds `INACTIVITY_TIMEOUT_MS` without a valid transition:
     * Firmware flags: `event_type = "TIMEOUT"` / condition `NO_DETECTION_TIMEOUT`.
     * Transmits status payload to Express.js backend.
     * **Partner Backend Responsibility:** Decides whether this timeout constitutes an official Unplanned Downtime event.

### 5.3 Node 5: Production Counter Logic (Falling Edge Detection)
* Counts finished manufactured pipes exiting the flying cutter area.
* **Logic Flow:**
  1. Pipe reaches output station $\rightarrow$ Sensor detects pipe (`state = DETECTED`).
  2. Crane / handler hoists pipe away $\rightarrow$ Sensor transitions from `DETECTED` $\rightarrow$ `NOT_DETECTED`.
  3. Firmware captures the **FALLING EDGE** transition.
  4. Increments `cumulative_count` by 1 and records timestamp.
* **Constraint:** Never count continuous presence to prevent duplicate counts of stationary pipes.

---

## 6. Sensor-Level vs. System-Level Boundary

To maintain clear responsibilities between firmware and web backend, the demarcation of duties is strictly enforced:

| Firmware Scope (Sensor-Level) | Partner Express.js Backend Scope (System-Level) |
|:---|:---|
| • Target detected boolean (`true`/`false`) | • Machine running / operating status |
| • Clean detection edge events (debounced) | • Machine idle status |
| • Event timestamps (milliseconds / NTP) | • Machine downtime declaration |
| • Cumulative pulse / pipe count | • Planned stop vs. unplanned breakdown |
| • Elapsed time since last valid detection | • Downtime root-cause categorization |
| • Sensor health (`OK`, `FAULT`, `UNKNOWN`) | • Production loss & OEE calculations |
| • Noise & bounce rejection metrics | • Dashboard data visualization & notifications |

---

## 7. Partner Integration Interface Specification

### 7.1 Proposed Event Data Payloads (JSON over HTTP POST)

#### Real-time Detection Event (`SENSOR_EVENT`)
```json
{
  "node_id": "S-03",
  "event_type": "DETECTED",
  "timestamp": 1725926400,
  "count": 1420,
  "interval_ms": 1250,
  "sensor_state": "OK"
}
```
*Valid `event_type` values:* `"DETECTED"`, `"NOT_DETECTED"`, `"TIMEOUT"`, `"FAULT"`

#### Periodic Heartbeat & Diagnostic Status (`SENSOR_STATUS`)
```json
{
  "node_id": "S-03",
  "status": "OK",
  "uptime_ms": 3600000,
  "last_seen_ms": 1250,
  "wifi_rssi": -62,
  "free_heap": 245120,
  "fault_code": 0
}
```
*Valid `status` values:* `"OK"`, `"FAULT"`, `"UNKNOWN"`

### 7.2 Open Integration Questions for Partner (Karl)

| # | Question Item | Technical Purpose | Status |
|:---:|:---|:---|:---:|
| **Q1** | Exact Express.js API endpoint URL? | Destination for `HTTP POST` (e.g. `http://SERVER_IP:PORT/api/iot/events`) | Pending Partner |
| **Q2** | Accepted JSON payload key names? | Alignment between firmware serializer and backend DTO | Pending Partner |
| **Q3** | Auth mechanism / Device token required? | HTTP Header (e.g. `x-device-token: XYZ`) | Pending Partner |
| **Q4** | Plant WiFi SSID & Password? | MikroTik 2.4GHz network credentials | Pending Client |
| **Q5** | Static IP or DHCP reservation? | ESP32 IP addressing on plant subnet | Pending Client |
| **Q6** | Protocol selection: HTTP or HTTPS? | Determines if SSL/TLS client overhead is needed | Pending Partner |
| **Q7** | Time synchronization method? | NTP server via WiFi vs. ESP32 internal `millis()` | Pending Partner |
| **Q8** | Offline buffering policy? | Ring buffer in RTC memory/Flash if WiFi drops | Pending Partner |

---

## 8. Configuration Architecture (`config.h`)

All hardware parameters, pin assignments, network credentials, and thresholds must be isolated in `src/config.h`. No hardcoded magic numbers permitted in driver or event logic.

```cpp
#pragma once

// ==========================================
// NODE IDENTITY
// ==========================================
#define NODE_ID                     "S-03"              // "S-01" through "S-05"
#define NODE_ROLE_DOWNTIME          1                   // Set 1 if Node 3
#define NODE_ROLE_COUNTER           0                   // Set 1 if Node 5
#define FIRMWARE_VERSION            "1.0.0"

// ==========================================
// HARDWARE PIN ASSIGNMENTS (38-PIN ESP32)
// ==========================================
#define SENSOR_INPUT_PIN            4                   // Pin D4 (GPIO 4)
#define STATUS_LED_PIN              2                   // Onboard diagnostic LED
#define SENSOR_ACTIVE_STATE         LOW                 // LOW for PC817 Optocoupler (Active LOW)

// ==========================================
// SIGNAL FILTERING & TIMING THRESHOLDS
// ==========================================
#define SENSOR_DEBOUNCE_MS          50                  // Minimum stable pulse width (ms)
#define INACTIVITY_TIMEOUT_MS       30000               // 30s timeout for Node 3 downtime trigger
#define HEARTBEAT_INTERVAL_MS       15000               // Status ping interval (ms)

// ==========================================
// NETWORK & BACKEND INTEGRATION
// ==========================================
#define WIFI_SSID                   "MIKROTIK_PLANT_2G" // To be confirmed
#define WIFI_PASSWORD               "PLANT_PASSWORD"    // To be confirmed
#define BACKEND_API_URL             "http://192.168.88.10:3000/api/iot/events"
#define BACKEND_HEARTBEAT_URL       "http://192.168.88.10:3000/api/iot/heartbeat"
#define DEVICE_AUTH_KEY             "DEVICE_SECRET_TOKEN"

// ==========================================
// DIAGNOSTICS & DEBUG
// ==========================================
#define SERIAL_BAUD_RATE            115200
#define DEBUG_LOG_VERBOSE           true
```

---

## 9. Progressive 5-Stage Testing Plan

Implementation strictly proceeds through these 5 validation stages:

### Stage 1: Basic Hardware Verification (No WiFi)
* **Goal:** Verify ESP32 power, GPIO reading, PC817 optical switching, and sensor detection without software complexity.
* **Method:** Serial monitor streaming raw pin state while manually positioning metal in front of M30 sensor face.
* **Pass Condition:** Serial logs clean transitions: `DETECTED` when metal is within 15mm; `NOT DETECTED` when clear. 0 crashes/resets.

### Stage 2: Debounce and Noise Immunity Testing
* **Goal:** Rejection of electrical noise from shop machinery, vibration, and bouncing transitions.
* **Method:** Tap sensor body, run nearby electric drill/motor, wave target rapidly.
* **Pass Condition:** 1 physical pass equals strictly 1 registered event. Zero spurious detections from vibration or motor noise.

### Stage 3: Timing, Counter, and Timeout Verification
* **Goal:** Validate Node 3 inactivity timer and Node 5 falling-edge production counter.
* **Method:**
  * Wave target 10 times consecutively $\rightarrow$ verify exact count of 10.
  * Leave Node 3 idle for $> 30\text{s}$ $\rightarrow$ verify `NO_DETECTION_TIMEOUT` triggers once.
  * Hold target in front of Node 5 and withdraw $\rightarrow$ verify increment happens strictly on withdrawal (falling edge).

### Stage 4: Fault Detection and Edge Case Testing
* **Goal:** Verify node robustness during power drops, sensor disconnects, and physical errors.
* **Method:** Unplug sensor signal wire; cut 12V sensor power; cycle ESP32 power rapidly.
* **Pass Condition:** Node reports `FAULT` or graceful recovery without firmware freeze.

### Stage 5: End-to-End System Integration Testing
* **Goal:** Multi-node synchronization with partner's Express.js API and Supabase database.
* **Method:** Run test nodes concurrently over MikroTik network; verify events accurately reflect in web dashboard log tables.

---

## 10. Technical Status & Question Tracking

| # | Item Description | Priority | Responsible | Current Confirmed Status |
|:---:|:---|:---:|:---:|:---|
| 1 | Exact Sensor Model & Datasheet | CRITICAL | Hardware / Supplier | **M30 PNP N.O. 10-30V DC Confirmed.** |
| 2 | Level Shifting Circuit Design | CRITICAL | Firmware Engineer | **4-Pin PC817 Optocoupler + 3.3kΩ Confirmed.** |
| 3 | ESP32 Board Variant & Pinout | HIGH | Firmware Engineer | **38-Pin DevKit V1 Confirmed; Pin D4 Selected.** |
| 4 | Ground Sharing Architecture | CRITICAL | Firmware Engineer | **True Galvanic Isolation (Unshared Grounds) Confirmed.** |
| 5 | Target Object per Machine Node | HIGH | Client (Rhende/Jayson)| Machine-specific mechanical mounting pending. |
| 6 | Exact Mounting Distance / Flush Mount| HIGH | Client / Plant Visit | Target range confirmed 10–20mm; final bracket pending. |
| 7 | Plant WiFi SSID & Credentials | HIGH | Client / IT | Pending network setup. |
| 8 | Express.js API Endpoint & Payload | CRITICAL | Partner (Karl) | Proposed structure documented; awaiting final route confirmation. |
| 9 | API Device Authentication Token | HIGH | Partner (Karl) | Pending backend route design. |
| 10| Host Server Local IP / Port | HIGH | Partner (Karl) | Local subnet target pending server deployment. |
| 11| NTP Server vs. Local `millis()` | MEDIUM | Partner (Karl) | Firmware ready for both NTP Unix epochs and delta `millis()`. |
| 12| Inactivity Timeout Constant (Node 3) | HIGH | Team / Client | Set to 30,000 ms baseline (configurable in `config.h`). |
| 13| Machine Cycle & Operating Speed | HIGH | Client Observation | Determines final `SENSOR_DEBOUNCE_MS`. |
| 14| Expected Detection Frequency | MEDIUM | Client Observation | Expected between 1–10 Hz depending on station. |
| 15| Offline Buffering Strategy | MEDIUM | Team Decision | In-memory ring buffer planned for transient WiFi drops. |

---

## 11. Handoff Rules & Governance for Antigravity IDE

As mandated in Section 11 of the project handoff specification:
1. **Read Entire Context:** Firmware scope is strictly limited to the sensor & ESP32 subsystem.
2. **Never Invent Hardware Specifications:** Hardware reality dictates code, not assumptions.
3. **No Code Before Circuit Resolution:** Circuit resolved with PC817 optocoupler and 38-pin DevKit V1 pinout.
4. **Modular Architecture:** Source files strictly partitioned (`main`, `sensor_driver`, `event_detector`, `diagnostics`, `integration`, `config`).
5. **No Magic Numbers:** Every threshold, pin, and timeout must live in `config.h`.
6. **No Premature Victory:** A successful compilation is not proof of working hardware. Stage 1 through Stage 5 testing must be physically validated.
7. **Preserve System Boundaries:** Firmware produces clean events, intervals, and health diagnostics. High-level machine-state logic belongs exclusively to the partner's Express.js backend.
