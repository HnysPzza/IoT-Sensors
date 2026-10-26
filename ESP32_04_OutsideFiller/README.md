# ESP32 Sensor 4: Outside Filler Wire Availability & Refill Monitor

## 1. Overview
This module (`ESP32_04_OutsideFiller`) runs on an ESP32 DevKit V1 microcontroller to monitor the availability of **outside filler wire** at the welding / feed guide station of the pipe manufacturing machine at Petro Hydro Pipe Corp.

- **Node Identifier**: `S-04`
- **Monitored Material**: Outside Filler Wire
- **Operational Role**: Availability and refill notification monitor

### Architecture & Independence
- **Sensor 1 (Raw Material Coil):** Monitors steady-state presence of raw-material coil stock at the uncoiler.
- **Sensor 2 (Inside Filler Wire):** Monitors steady-state presence of inside filler wire (`S-02`).
- **Sensor 3 (Main Drive / Downtime):** Monitors dynamic shaft pulses for machine downtime detection.
- **Sensor 4 (Outside Filler Wire):** Monitors steady-state presence of outside filler wire (`S-04`). Operates completely independently of Sensor 2. Either filler wire may be unavailable while the other remains available.
- **Sensor 5 (Finished Product Counter):** Monitors finished pipes exiting the line.

### Physical Sensing Limitation & Local Testing Note
The inductive proximity sensor detects metal presence within 10–20 mm of its sensing face. It **cannot** measure the total mass, remaining wire meters, or spool diameter. Missing detection indicates `NO WIRE DETECTED / POSSIBLE REFILL OR FEED INSPECTION REQUIRED`. A missing detection signal alone indicates wire absence at this specific sensing station; it does **not** conclusively prove that the supply spool is empty until the physical installation and wire path are verified on the machine.

---

## 2. Hardware Wiring (Consistent with Sensor 2 Setup)
- **Sensor Model:** 12V PNP Normally Open (N.O.) Inductive Proximity Sensor.
- **Optocoupler:** 4-pin PC817 for galvanic isolation between the 12V machine circuit and the 3.3V ESP32.

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
| **Outside Filler Wire In Place** | 12V Active (HIGH) | ON (conducts to GND) | **LOW (0V)** | `WIRE PRESENT` | **OFF** (Normal) |
| **No Wire Detected** | Floats / 0V | OFF (open circuit) | **HIGH (3.3V)** | `NO WIRE DETECTED` | **ON** (Alert) |

---

## 3. Operational Logic & Rules
1. **Debounce Protection:** 50 ms filter confirms state change before triggering an event.
2. **Anti-Spam Logging:** Only emits transition event / alert banners when the debounced state changes.
3. **Alert Indication:** Drives onboard LED (GPIO 2) **ON** when wire is missing, **OFF** when wire is present.
4. **Independent Status:** Sensor 4 does not share state or depend on Sensor 2.
5. **No Invented Rules:** No grace periods, timeout escalations, or downtime calculations are applied locally.
6. **Heartbeat Summary:** Emits a compact status line every 5 seconds indicating uptime, current state, duration in state, pin reading, LED status, and transition count.
7. **Startup Diagnostic:** Explicitly prints the initial detected hardware state at boot.
