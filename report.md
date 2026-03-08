# String Art Controller Project Report

## 1. Project Purpose and Main Functionality

This project is a **Geometric String Art Controller and Pattern Generator**. It consists of two main components:

- **ESP32-based String Art Controller**: Firmware for an ESP32 microcontroller that controls stepper motors and a threader mechanism to physically create string art patterns on a turntable. The controller receives command files (peg sequences) and executes them, wrapping thread around pegs according to geometric patterns.
- **Pattern Generator (Python)**: A Python script (`DemoStringArts.py`) that generates geometric string art patterns, previews them, and exports command files (.txt) for upload to the ESP32 controller.

The workflow is:

1. Generate pattern command files using the Python script.
2. Upload these files via a web interface hosted by the ESP32.
3. The ESP32 executes the commands, moving motors and threader to create the physical string art.

## 2. Key Files and Their Roles

- **DemoStringArts.py**: Main Python script for generating, previewing, and exporting string art patterns. Handles configuration, pattern registry, and file output.
- **test_patterns.py**: Python test script for validating pattern generators and command file structure.
- **pattern_cardioid.txt**: Example output file containing peg sequence commands for a cardioid pattern.
- **platformio.ini**: PlatformIO configuration file for building and uploading ESP32 firmware. Specifies board, framework, and library dependencies.
- **src/main.cpp**: Entry point for ESP32 firmware. Initializes subsystems and runs the main control loop.
- **src/commands.cpp**: Handles parsing and execution of command files.
- **src/steppers.cpp**: Controls stepper motors for turntable and threader.
- **src/webserver.cpp**: Implements the ESP32 web interface for file upload and control.
- **include/config.h**: Hardware configuration (WiFi credentials, pin assignments, motor parameters).
- **include/webpage.h**: Embedded HTML/JS for the ESP32 web interface.
- **include/README**: Explains the purpose and usage of header files.
- **lib/README**: Instructions for adding custom libraries.
- **test/README**: Information about unit testing with PlatformIO.

## 3. Directory Structure and Organization

- **src/**: C++ source files for ESP32 firmware (main logic, motor control, command handling, webserver).
- **include/**: Header files for shared declarations and configuration.
- **lib/**: Directory for custom libraries (currently only README).
- **test/**: Directory for PlatformIO unit tests (currently only README).
- ****pycache**/**: Python cache files.
- **Root**: Python scripts, example command files, PlatformIO config.

## 4. Notable Code Features or Patterns

- **Pattern Generation**: Python functions generate peg sequences for various geometric patterns (e.g., cardioid, nested polygons). Each pattern is registered and selectable via config.
- **Command File Format**: CSV-like text files with sequences such as `slot,D,slot+1,U`, representing peg movements and threader actions.
- **Web Interface**: ESP32 hosts a web page (HTML/JS in `webpage.h`) for uploading command files and controlling execution. Features include configuration, file upload, and status polling.
- **Modular Firmware**: ESP32 code is organized into modules for logging, motor control, command parsing, and webserver.
- **Configurable Hardware**: Pin assignments, motor parameters, and WiFi credentials are set in `config.h`.
- **PlatformIO Build System**: Uses PlatformIO for cross-platform firmware development, dependency management, and unit testing.

## 5. Build and Test Instructions

### Build (ESP32 Firmware)

- Edit `platformio.ini` to configure board and dependencies.
- Place custom libraries in `lib/` as needed.
- Build and upload firmware using PlatformIO CLI:
  ```
  pio run --target upload
  ```
- Serial monitor can be accessed via PlatformIO.

### Pattern Generation (Python)

- Edit configuration and pattern parameters in `DemoStringArts.py`.
- Run the script:
  ```
  python DemoStringArts.py
  ```
- Preview windows will open, and `.txt` command files will be saved in the project directory.
- Upload `.txt` files to the ESP32 via the web interface.

### Testing

- Unit tests for pattern generators can be run with:
  ```
  python test_patterns.py
  ```
- PlatformIO unit testing is supported (see `test/README`).

---

**References:**

- [DemoStringArts.py](DemoStringArts.py)
- [platformio.ini](platformio.ini)
- [src/main.cpp](src/main.cpp)
- [include/config.h](include/config.h)
- [include/webpage.h](include/webpage.h)
- [test_patterns.py](test_patterns.py)
- [lib/README](lib/README)
- [test/README](test/README)
- [include/README](include/README)

This report provides a comprehensive overview suitable for onboarding, documentation, or review.
