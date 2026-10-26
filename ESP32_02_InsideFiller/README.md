# ESP32 Sensor 2: Inside Filler Wire Availability & Refill Monitor

## 1. Overview
This module (`ESP32_02_InsideFiller`) runs on an ESP32 DevKit V1 microcontroller to monitor the availability of **inside filler wire** at the welding / feed guide station of the pipe manufacturing machine.

### Crucial Distinctions in the Multi-Sensor Architecture
- **Sensor 1 (Raw Material Coil):** Monitors steady-state presence of raw-material coil stock at the uncoiler.
- **Sensor 2 (Inside Filler Wire):** Monitors steady-state presence of inside filler wire at the welding wire guide. Reuses Sensor 1's steady-state presence and refill alert model.
- **Sensor 3 (Main Drive / Downtime):** Monitors dynamic state transitions (pulses) from the main motor shaft. If transitions cease for 10s+, it triggers machine downtime.
- **Sensor 5 (Finished Product Counter):** Monitors finished pipes exiting the line and tallies total production count.

### Physical Sensing Limitation & Local Testing Note
The inductive proximity sensor detects metal presence within 10–20 mm of its sensing face. It **cannot** measure the total mass, remaining wire meters, or spool diameter. Missing detection indicates `NO WIRE DETECTED / POSSIBLE REFILL OR FEED INSPECTION REQUIRED`. A missing signal does not confirm whether the spool is empty or the wire slipped out of alignment until physical installation is inspected.

---

## 2. Hardware Wiring (Consistent with All Sensor Nodes)
- **Sensor Model:** 12V PNP Normally Open (N.O.) Inductive Proximity Sensor.
- **Optocoupler:** 4-pin PC817 for galvanic isolation between the 12V sensor and the 3.3V ESP32.

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

### Signal Logic Table
| Physical Condition | Proximity Sensor Output | PC817 Phototransistor | ESP32 GPIO 4 | System State | Onboard LED (GPIO 2) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Filler Wire In Place** | 12V Active (HIGH) | ON (conducts to GND) | **LOW (0V)** | `WIRE PRESENT` | **OFF** (Normal) |
| **No Wire Detected** | Floats / 0V | OFF (open circuit) | **HIGH (3.3V)** | `NO WIRE DETECTED` | **ON** (Alert) |

---

## 3. Operational Logic & Rules
1. **Debounce Protection:** 50 ms filter confirms state change before triggering an event.
2. **Anti-Spam Logging:** Only emits transition event / alert banners when the debounced state changes.
3. **Alert Indication:** Turns onboard LED (GPIO 2) ON when wire is missing, OFF when wire is present.
4. **No Invented Rules:** Adheres to verified Sensor 1 behavior. No operator grace period or downtime threshold is invented locally.
5. **Heartbeat Summary:** Emits a compact status line every 5 seconds indicating uptime, current state, duration in state, and transition count.
