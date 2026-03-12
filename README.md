# String Art Machine — Automated ESP32 Controller

An ESP32-based automated string art machine that uses stepper motors to wind thread around pegs on a turntable, creating intricate mathematical string art patterns. The system pairs a Python-based pattern generator with embedded firmware, connected via a WiFi web interface.

---

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Hardware](#hardware)
- [Software Architecture](#software-architecture)
  - [Firmware (C++ / PlatformIO)](#firmware-c--platformio)
  - [Pattern Generator (Python)](#pattern-generator-python)
- [Web Interface](#web-interface)
- [Command Format](#command-format)
- [Pattern Algorithms](#pattern-algorithms)
- [API Reference](#api-reference)
- [File Structure](#file-structure)
- [Getting Started](#getting-started)
- [Configuration Reference](#configuration-reference)

---

## Overview

This project automates the creation of string art — a craft where thread is wound between pegs arranged in a circle to form geometric and mathematical patterns. Instead of winding by hand, this machine uses:

- A **turntable** driven by a stepper motor through a 16:1 cycloidal gear reducer, positioning 200 pegs with high precision.
- A **threader** mechanism (linear actuator) that inserts and withdraws a needle to wrap thread around each peg.
- An **ESP32 microcontroller** running firmware that coordinates both motors, accepts commands over WiFi, and exposes a full web-based control panel.
- A **Python script** that generates mathematical patterns (cardioids, nested polygons, etc.) and exports them as command sequences the machine can execute.

---

## How It Works

```
  ┌──────────────────┐       ┌────────────────────┐
  │  Python Script    │       │   ESP32 Firmware    │
  │  (DemoStringArts) │       │                     │
  │                   │       │  ┌───────────────┐  │
  │  Generate pattern │──────▶│  │ Web Server    │  │
  │  Export .txt file  │ WiFi │  │ (Upload API)  │  │
  └──────────────────┘ Upload│  └───────┬───────┘  │
                              │          │          │
                              │  ┌───────▼───────┐  │
                              │  │ Command Parser │  │
                              │  │ & Sequencer    │  │
                              │  └───────┬───────┘  │
                              │          │          │
                              │  ┌───────▼───────┐  │
                              │  │ Stepper Motor  │  │
                              │  │ Controller     │  │
                              │  └───────┬───────┘  │
                              │     ┌────┴────┐     │
                              │     │         │     │
                              │  Turntable  Threader │
                              └────────────────────┘
```

1. **Pattern Generation (PC):** The Python script `DemoStringArts.py` computes a mathematical pattern as a sequence of peg indices, then converts it to machine commands (slot positions + threader up/down signals). The output is saved as a `.txt` file.
2. **Upload (WiFi):** The user opens the ESP32's web interface in a browser and uploads the command file.
3. **Execution (ESP32):** The firmware parses the CSV commands, then steps through them one-by-one — rotating the turntable to each peg and cycling the threader to wrap the thread.
4. **Result:** A physical string art piece created automatically.

---

## Hardware

### Components

| Component                     | Description                                                       |
| ----------------------------- | ----------------------------------------------------------------- |
| **ESP32 DOIT DevKit V1**      | Main controller (WiFi + dual-core)                                |
| **NEMA 17 Stepper × 2**       | 200 steps/rev (1.8°/step) for turntable and threader              |
| **TB6600 Stepper Driver × 2** | Microstepping drivers (up to 32 microsteps)                       |
| **16:1 Cycloidal Reducer**    | Gear reduction on the turntable for precision positioning         |
| **Limit Switch**              | Normally-open, used for threader homing (active-low with pull-up) |
| **Turntable**                 | Circular platform with 200 evenly-spaced pegs                     |
| **Threader Mechanism**        | Linear needle actuator for wrapping thread around pegs            |

### Pin Mapping (ESP32)

| Function       | GPIO Pin          |
| -------------- | ----------------- |
| Turntable STEP | 26                |
| Turntable DIR  | 27                |
| Threader STEP  | 25                |
| Threader DIR   | 33                |
| Limit Switch   | 32 (INPUT_PULLUP) |

---

## Software Architecture

### Firmware (C++ / PlatformIO)

The firmware is built with PlatformIO targeting the ESP32 DOIT DevKit V1. It uses the Arduino framework and is organized into four core modules:

#### `main.cpp` — Entry Point & Event Loop

Minimal dispatcher that initializes all subsystems in `setup()` and runs a non-blocking polling loop in `loop()`:

- Polls threader homing state machine
- Advances command execution via `commandsPoll()`
- Monitors limit switch for unexpected triggers (emergency pause)

#### `commands.cpp` — Command Sequencer

Parses, stores, and executes command sequences:

- **Parser:** Accepts CSV text with slot numbers (0–199), `D` (threader down), `U` (threader up), and `C` (threader center). Validates all values against peg count; stores up to 20,000 commands.
- **State Machine:** Cycles through `IDLE → RUNNING → PAUSED → IDLE`. On each `loop()` tick, waits for motors to finish, applies an optional inter-command delay, then dispatches the next command.
- **Runtime Controls:** Command delay (0–60 sec) and speed percentage (1–100%) adjustable via the web UI during execution.

#### `steppers.cpp` — Motor Control & Homing

Drives both stepper motors using the **FastAccelStepper** library for non-blocking motion with acceleration profiles.

**Turntable:**

- Tracks absolute position in steps and a normalized true angle (0°–360°) in double precision to prevent floating-point drift.
- `turntableGoToSlot(slot)` calculates the shortest rotation path. If the shortest path is CCW, the turntable overshoots past the target then approaches from CW, ensuring the turntable always arrives via CW movement to eliminate backlash.
- Supports free-form jogging, zeroing, and slot queries.

**Threader:**

- Moves to three calibratable positions: Up (5°), Center (18°), Down (22°) from home.
- Non-blocking 3-phase homing sequence using the limit switch:
  1. **Seek** — Move toward switch at full speed.
  2. **Backoff** — Back away 10 steps after switch triggers.
  3. **Slow Approach** — Re-approach at 20% speed for precision.
- Limit switch is debounced (20 ms) and monitored for unexpected hits during operation (triggers emergency pause).

#### `webserver.cpp` — WiFi & REST API

Runs an asynchronous HTTP server on port 80 using **ESPAsyncWebServer** and **ArduinoJson**:

- Connects to WiFi (SSID: `VitaminWater`). Falls back to AP mode (`StringArt` / `stringart123`) if connection fails within 15 seconds.
- Serves an embedded single-page web UI from `webpage.h` (stored in PROGMEM).
- Provides REST API endpoints for status polling, configuration, manual control, file upload, and execution control.
- Upload handler accumulates chunked request bodies (up to 128 KB) before parsing.

#### `logbuf.cpp` — Ring Buffer Logger

Circular buffer holding 200 log entries (96 chars each) with monotonic IDs:

- `logMsg(fmt, ...)` — Printf-style logging, echoed to Serial.
- `logGetSince(id)` — Returns entries newer than a given ID, enabling the web UI to poll for incremental updates.

### Pattern Generator (Python)

#### `DemoStringArts.py`

Generates mathematical patterns and converts them to machine command sequences:

- **Configuration:** `NUM_PEGS = 200`, selectable pattern list, color theme for preview.
- **`build_commands(path)`:** Converts a list of peg indices into the 4-token-per-wrap format the ESP32 expects: `[slot, D, slot+1, U]` — move to peg, lower threader, nudge one peg forward to wrap, raise threader.
- **`preview(path)`:** Matplotlib visualization showing the circle of pegs and thread lines.
- **Output:** Saves command tokens as comma-separated `.txt` files ready for upload.

#### `test_patterns.py`

Regression test suite that validates all patterns:

- Checks that token count is divisible by 4.
- Verifies the `+1 wrap` invariant: each wrap always advances exactly one peg from the slot position.

---

## Web Interface

The embedded web UI (defined in `webpage.h`) is a single-page responsive application with a dark theme. It provides:

| Section            | Function                                                                                                                                          |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Status**         | Live display of machine state (IDLE/RUNNING/PAUSED/HOMING), current slot, turntable angle, threader angle, and progress bar. Polled every 500 ms. |
| **Configuration**  | Edit all tunable parameters: peg count, gear ratio, overshoot angle, microsteps, motor speeds, accelerations, and threader positions.             |
| **Manual Control** | Jog turntable (±1.8° with 10× multiplier), go to specific slot, zero position, home threader, and manually move threader up/center/down.          |
| **Program Upload** | Upload a `.txt`/`.csv` command file to the ESP32.                                                                                                 |
| **Execution**      | Start, pause, resume, and stop pattern execution. Adjust command delay and speed in real time.                                                    |
| **Log**            | Live scrolling log display, polling every 1 second.                                                                                               |

---

## Command Format

Command files are comma-separated text with the following token types:

| Token     | Meaning                             |
| --------- | ----------------------------------- |
| `0`–`199` | Move turntable to this peg slot     |
| `D`       | Move threader down (engage thread)  |
| `U`       | Move threader up (disengage thread) |
| `C`       | Move threader to center position    |

Each thread wrap is a group of 4 tokens:

```
slot, D, slot+1, U
```

Example sequence for wrapping pegs 1, 4, and 3:

```
1,D,2,U,4,D,5,U,3,D,4,U
```

This means: go to peg 1 → threader down → nudge to peg 2 (wraps thread) → threader up → go to peg 4 → threader down → nudge to peg 5 → threader up → ...

---

## Pattern Algorithms

### Cardioid / Nephroid

```python
pattern_cardioid(num_pegs, multiplier=2)
```

Connects each peg `i` to peg `(i × multiplier) mod num_pegs`:

- **multiplier = 2** → Cardioid curve
- **multiplier = 3** → Nephroid curve
- **multiplier = 4** → 3-cusp epicycloid

Duplicate symmetric pairs and self-connections are removed. The default produces ~200 unique thread wraps for 200 pegs.

### Nested Polygons

```python
pattern_nested_polygons(num_pegs, sides=6, layers=12, rotation_step=7.5)
```

Draws concentric regular polygons, each rotated slightly from the previous:

- `sides` — Number of vertices per polygon (3 = triangles, 6 = hexagons, etc.)
- `layers` — Number of concentric rings
- `rotation_step` — Rotation offset between layers in degrees

---

## API Reference

All endpoints are served on port 80. POST bodies are JSON unless noted.

| Endpoint           | Method | Description                                                                                             |
| ------------------ | ------ | ------------------------------------------------------------------------------------------------------- |
| `/`                | GET    | Serve the web UI                                                                                        |
| `/api/status`      | GET    | Machine state, slot, angles, progress, homing status                                                    |
| `/api/log?since=N` | GET    | Log entries with ID > N                                                                                 |
| `/api/config`      | GET    | Current configuration values                                                                            |
| `/api/config`      | POST   | Update configuration (pegs, gear ratio, speeds, etc.)                                                   |
| `/api/zero`        | POST   | Set current turntable position as slot 0                                                                |
| `/api/home`        | POST   | Start threader homing sequence                                                                          |
| `/api/start`       | POST   | Begin command execution                                                                                 |
| `/api/pause`       | POST   | Pause execution                                                                                         |
| `/api/resume`      | POST   | Resume execution                                                                                        |
| `/api/stop`        | POST   | Stop execution                                                                                          |
| `/api/jog`         | POST   | Jog turntable: `{"degrees": float}`                                                                     |
| `/api/goto`        | POST   | Go to slot: `{"slot": int}`                                                                             |
| `/api/threader`    | POST   | Threader control: `{"action": "up"\|"down"\|"center"\|"setUp"\|"setDown"\|"setCenter", "value": float}` |
| `/api/upload`      | POST   | Upload command sequence (raw CSV text body)                                                             |
| `/api/debug`       | POST   | Set delay/speed: `{"delay": float, "speed": int}`                                                       |

**Status Response:**

```json
{
  "state": 0,
  "slot": 42,
  "pegs": 200,
  "homed": true,
  "cmdIndex": 10,
  "cmdCount": 800,
  "ttDeg": 76.5,
  "thDeg": 18.25,
  "cmdDelay": 0.0,
  "speedPct": 100
}
```

---

## File Structure

```
├── README.md                  # This file
├── platformio.ini             # PlatformIO build configuration (ESP32 DOIT DevKit V1)
├── DemoStringArts.py          # Python pattern generator & previewer
├── test_patterns.py           # Pattern validation test suite
├── pattern_cardioid.txt       # Pre-generated cardioid command file (200 pegs)
├── include/
│   ├── config.h               # Hardware pins, motor defaults, system limits
│   ├── commands.h             # Command sequencer interface
│   ├── steppers.h             # Stepper motor control interface
│   ├── webserver.h            # Web server initialization
│   ├── webpage.h              # Embedded HTML/CSS/JS web UI (PROGMEM)
│   └── logbuf.h               # Ring buffer logger interface
├── src/
│   ├── main.cpp               # Arduino setup() and loop() entry point
│   ├── commands.cpp           # CSV parser and command execution state machine
│   ├── steppers.cpp           # FastAccelStepper motor control & homing logic
│   ├── webserver.cpp          # WiFi connection, AsyncWebServer, REST API
│   └── logbuf.cpp             # Circular log buffer with serial echo
├── lib/                       # PlatformIO library directory (unused)
└── test/                      # PlatformIO test directory
```

---

## Getting Started

### Prerequisites

- **PlatformIO** (VS Code extension or CLI)
- **Python 3** with `matplotlib` (for pattern generation and previews)
- **ESP32 DOIT DevKit V1** connected via USB

### Build & Upload Firmware

1. Open the project folder in VS Code with PlatformIO installed.
2. Build and upload:
   ```
   pio run --target upload
   ```
3. Open the serial monitor at 115200 baud to see boot messages and the assigned IP address.

### Connect to the Web Interface

1. The ESP32 connects to WiFi SSID `VitaminWater`. If it fails, it creates an access point named `StringArt` (password: `stringart123`).
2. Open the IP address shown in the serial monitor in a web browser.

### Generate & Upload a Pattern

1. Edit `DemoStringArts.py` to select desired patterns via `SELECTED_PATTERNS`.
2. Run the script:
   ```
   python DemoStringArts.py
   ```
3. A matplotlib preview window will show the pattern. Command files are saved to the output directory.
4. In the web interface, use the **Program Upload** section to upload the generated `.txt` file.
5. Home the threader using the **Home** button.
6. Press **Start** to begin automated string art creation.

### Validate Patterns

```
python test_patterns.py
```

This runs all patterns through validation checks to ensure they produce correctly structured command sequences.

---

## Configuration Reference

All parameters below are adjustable at runtime through the web interface's Configuration panel.

### Turntable

| Parameter      | Default         | Range       | Description                                       |
| -------------- | --------------- | ----------- | ------------------------------------------------- |
| Number of Pegs | 200             | 2–1024      | Peg count on the turntable                        |
| Gear Ratio     | 16.0            | 0.1–100.0   | Turntable gear reduction ratio                    |
| Overshoot      | 2.0°            | 0–10°       | CCW overshoot angle; turntable always finishes CW |
| Microsteps     | 16              | 1–32        | Microstepping divisor                             |
| Speed          | 1000 steps/sec  | 1000–50000  | Maximum step rate                                 |
| Acceleration   | 1500 steps/sec² | 1500–100000 | Acceleration ramp                                 |

### Threader

| Parameter       | Default         | Range | Description                     |
| --------------- | --------------- | ----- | ------------------------------- |
| Microsteps      | 16              | 1–32  | Microstepping divisor           |
| Speed           | 1000 steps/sec  | 1000+ | Maximum step rate               |
| Acceleration    | 1000 steps/sec² | 1000+ | Acceleration ramp               |
| Home Speed      | 30 steps/sec    | —     | Slow homing approach speed      |
| Up Position     | 5.0°            | —     | Threader raised (clear of pegs) |
| Center Position | 18.0°           | —     | Loading / neutral position      |
| Down Position   | 22.0°           | —     | Threader engaged (wrapping)     |

### Execution

| Parameter     | Default | Range | Description                |
| ------------- | ------- | ----- | -------------------------- |
| Command Delay | 0.0 sec | 0–60  | Pause between each command |
| Speed %       | 100     | 1–100 | Global speed multiplier    |
