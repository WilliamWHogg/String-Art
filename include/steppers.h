#pragma once
#include <stdint.h>

// Initialise FastAccelStepper engines and both steppers.
void steppersInit();

// Recalculate stepsPerSlot after pegs or microstep changes.
void steppersRecalc();

// ─── Turntable ──────────────────────────────────────────────────────────────
// Move to a slot by shortest path (CW or CCW).
void turntableGoToSlot(uint16_t slot);

// Jog by a signed number of degrees (+ = CW, − = CCW).
void turntableJog(float degrees);

// Declare current position as slot 0.
void turntableZero();

// Current logical slot number.
uint16_t turntableCurrentSlot();

// True angle in degrees (drift-free, double precision).
double turntableTrueDeg();
double threaderTrueDeg();

// ─── Threader ───────────────────────────────────────────────────────────────
void threaderDown();
void threaderUp();
void threaderCenter();
void threaderGoToDown();   // move to absolute down position
void threaderGoToUp();     // move to absolute up position
void threaderGoToCenter(); // move to absolute center position

// Begin non-blocking homing sequence.  Poll threaderHomingBusy() in loop().
void threaderHomeStart();
bool threaderHomingBusy(); // true while homing is in progress
void threaderHomePoll();   // call every loop() iteration during homing
bool threaderIsHomed();

// ─── General ────────────────────────────────────────────────────────────────
bool steppersMoving();          // true if either motor is still running
void steppersPollLimitSwitch(); // call from loop() to log switch changes

// Runtime-adjustable parameters (call steppersRecalc() after changing microsteps/pegs)
extern uint16_t numPegs;
extern uint16_t ttMicrosteps;
extern uint16_t thMicrosteps;
extern uint32_t ttSpeed; // steps/sec
extern uint32_t ttAccel; // steps/sec²
extern uint32_t thSpeed;
extern uint32_t thAccel;
extern double ttGearRatio;   // turntable gear ratio (adjustable for calibration)
extern float ttBacklashDeg;  // turntable backlash compensation (degrees)
extern uint32_t thHomeSpeed; // homing speed (steps/sec)
extern float thUpDeg;        // threader "up" position (degrees from home)
extern float thCenterDeg;    // threader "center" position (degrees from home)
extern float thDownDeg;      // threader "down" position (degrees from home)
