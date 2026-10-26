# ESP32 Sensor 1: Raw-Material Coil Availability Monitor

## 1. Overview
This module (`ESP32_01_RawMaterial`) runs on an ESP32 DevKit V1 microcontroller to monitor the presence of raw-material coil stock at the uncoiler / coil feed point.

### Crucial Distinction from Sensor 3
- **Sensor 3 (Main Drive / Downtime):** Monitors continuous dynamic state transitions (cycling pulses). If transitions stop for a threshold (e.g., 10s or 30s), it reports machine downtime.
- **Sensor 1 (Raw Material Coil):** Monitors **material presence at a specific sensing station**. It is a steady-state presence monitor.
- **Physical Sensing Limitation:** The inductive proximity sensor detects metal within 10–20mm of its face. It **cannot** measure the total mass, thickness, or remaining meters of coil stock. Missing metal is categorized as `NO COIL DETECTED / POSSIBLE REFILL REQUIRED`.

---

## 2. Hardware Wiring (Identical to Sensor 3 Design)
- **Sensor Model:** 12V PNP Normally Open (N.O.) Inductive Proximity Sensor (M30).
- **Optocoupler:** 4-pin PC817 for galvanic isolation between 12V machine circuit and 3.3V ESP32 circuit.

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
| **Coil In Place** | 12V Active (HIGH) | ON (conducts to GND) | **LOW (0V)** | `COIL PRESENT` | **OFF** (Normal) |
| **No Coil / Empty** | Floats / 0V | OFF (open circuit) | **HIGH (3.3V)** | `NO COIL DETECTED` | **ON** (Alert) |

---

## 3. Features & Logic
1. **Debounce Protection:** Filters out industrial vibrations or transient signal noise (50 ms threshold).
2. **State-Change Logging Only:** Only emits full event / alert banners to the Serial Monitor when the sensor state changes (prevents spamming).
3. **Refill Alert Identification:** Identifies when material is absent and explicitly reports `NO COIL DETECTED / POSSIBLE REFILL REQUIRED`.
4. **Visual Indicator:** Drives the onboard blue LED (GPIO 2) so floor technicians see an immediate physical indicator without needing a monitor.
5. **Periodic Heartbeat:** Logs a compact single-line heartbeat every 5 seconds indicating uptime, current state, duration in state, and total state transitions.
