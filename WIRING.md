# Wiring Diagram — ESP32 → TB6600 Stepper Drivers & Homing Switch

## Pin Summary

| Signal              | ESP32 GPIO | Destination                                          |
| ------------------- | ---------- | ---------------------------------------------------- |
| Turntable STEP      | GPIO 26    | TB6600 #1 — PUL+ (STEP)                              |
| Turntable DIR       | GPIO 27    | TB6600 #1 — DIR+                                     |
| Threader STEP       | GPIO 25    | TB6600 #2 — PUL+ (STEP)                              |
| Threader DIR        | GPIO 33    | TB6600 #2 — DIR+                                     |
| Homing Limit Switch | GPIO 32    | Switch terminal (other leg → GND)                    |
| Common GND          | GND        | TB6600 #1 PUL−/DIR−, TB6600 #2 PUL−/DIR−, Switch GND |

---

## ASCII Wiring Diagram

```
                        ┌─────────────────────────────────┐
                        │            ESP32                │
                        │                                 │
                        │  GPIO 26 (TT_STEP) ─────────────┼──────────► TB6600 #1  PUL+
                        │  GPIO 27 (TT_DIR)  ─────────────┼──────────► TB6600 #1  DIR+
                        │                                 │            TB6600 #1  PUL− ──► GND
                        │                                 │            TB6600 #1  DIR− ──► GND
                        │                                 │
                        │  GPIO 25 (TH_STEP) ─────────────┼──────────► TB6600 #2  PUL+
                        │  GPIO 33 (TH_DIR)  ─────────────┼──────────► TB6600 #2  DIR+
                        │                                 │            TB6600 #2  PUL− ──► GND
                        │                                 │            TB6600 #2  DIR− ──► GND
                        │                                 │
                        │  GPIO 32 (LIMIT_SW)─────────────┼──────────► Homing Switch (NO terminal)
                        │  GND ───────────────────────────┼──────────► Homing Switch (COM terminal)
                        │                                 │
                        └─────────────────────────────────┘
```

---

## TB6600 Driver Connections (each driver)

```
TB6600 Driver
┌──────────────────────────────┐
│  Signal Inputs               │
│  PUL+  ◄── ESP32 STEP pin   │   3.3 V logic is sufficient; no resistor needed
│  PUL−  ◄── GND              │
│  DIR+  ◄── ESP32 DIR  pin   │
│  DIR−  ◄── GND              │
│  ENA+  ◄── (leave open or   │   Tie ENA+/ENA− together or leave open to keep
│  ENA−  ◄──  tie to GND)     │   driver always enabled
│                              │
│  Power Input                 │
│  VCC  ◄── 12–24 V DC supply │
│  GND  ◄── Power supply GND  │
│                              │
│  Motor Output                │
│  A+  ──► Stepper coil A+    │
│  A−  ──► Stepper coil A−    │
│  B+  ──► Stepper coil B+    │
│  B−  ──► Stepper coil B−    │
└──────────────────────────────┘
```

---

## Homing / Limit Switch

The switch is **normally-open (NO)**, wired directly between GPIO 32 and GND.

```
  GPIO 32 ──────┬──── Switch NO terminal
                │
               [SW]  (normally open)
                │
  GND    ──────┴──── Switch COM terminal
```

The firmware enables the internal **pull-up** on GPIO 32, so the pin reads HIGH
when the switch is open and LOW when the switch closes (homing triggered).

---

## Power Notes

- ESP32 is powered via USB or its 5 V / 3.3 V rails (do **not** exceed 3.3 V on GPIO pins).
- TB6600 drivers require a separate **12–24 V DC** supply sized for your stepper motors.
- Share a **common GND** between the ESP32 and the TB6600 driver signal grounds (PUL−/DIR−).
- Do **not** connect the high-voltage motor supply directly to the ESP32.

```

```
