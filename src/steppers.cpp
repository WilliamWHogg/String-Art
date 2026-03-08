#include "config.h"
#include "steppers.h"
#include "commands.h"
#include "logbuf.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <math.h>

// ─── Runtime parameters (defaults from config.h) ───────────────────────────-
uint16_t numPegs = DEFAULT_PEGS;
double ttGearRatio = TT_GEAR_RATIO_DEFAULT;
float ttBacklashDeg = TT_BACKLASH_DEG_DEFAULT;
uint16_t ttMicrosteps = TT_MICROSTEPS_DEFAULT;
uint16_t thMicrosteps = TH_MICROSTEPS_DEFAULT;
uint32_t ttSpeed = TT_SPEED_DEFAULT;
uint32_t ttAccel = TT_ACCEL_DEFAULT;
uint32_t thSpeed = TH_SPEED_DEFAULT;
uint32_t thAccel = TH_ACCEL_DEFAULT;
uint32_t thHomeSpeed = TH_HOME_SPEED;

// ─── Derived values ─────────────────────────────────────────────────────────
static double ttStepsPerRev; // total microsteps for one full output revolution (double for fractional gear ratios)

// ─── FastAccelStepper objects ───────────────────────────────────────────────
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper *ttStepper = nullptr;
static FastAccelStepper *thStepper = nullptr;

// ─── Turntable position tracking ───────────────────────────────────────────
static int32_t ttPositionSteps = 0; // absolute position in microsteps
static double ttTrueDeg = 0.0;      // true turntable angle in degrees [0, 360)
static int8_t ttLastDir = 0;        // last movement direction: +1 CW, -1 CCW, 0 unknown

// ─── Limit switch edge detection & debounce ────────────────────────────────
static bool lastSwitchState = false;
static bool debouncedSwitch = false;
static unsigned long lastSwitchChange = 0;
static const unsigned long DEBOUNCE_MS = 20;
static bool lastPollState = false;

static bool readSwitchDebounced()
{
    bool raw = (digitalRead(LIMIT_SW_PIN) == LOW);
    if (raw != lastSwitchState)
    {
        lastSwitchChange = millis();
        lastSwitchState = raw;
    }
    if ((millis() - lastSwitchChange) >= DEBOUNCE_MS)
        debouncedSwitch = lastSwitchState;
    return debouncedSwitch;
}

// ─── Threader position tracking ────────────────────────────────────────────
float thUpDeg = TH_UP_DEG_DEFAULT;         // "up" position in degrees from home
float thCenterDeg = TH_CENTER_DEG_DEFAULT; // "center" position in degrees from home
float thDownDeg = TH_DOWN_DEG_DEFAULT;     // "down" position in degrees from home
static double thTrueDeg = 0.0;             // true threader angle in degrees from home

// ─── Threader homing state machine ─────────────────────────────────────────
static bool _threaderHomed = false;
enum HomeState
{
    HOME_IDLE,
    HOME_SEEK,
    HOME_BACKOFF,
    HOME_SLOW
};
static HomeState homeState = HOME_IDLE;

// ──────────────────────────────────────────────────────────────────────────────

static uint16_t prevThMicrosteps = TH_MICROSTEPS_DEFAULT;

void steppersRecalc()
{
    ttStepsPerRev = (double)MOTOR_STEPS_PER_REV * ttGearRatio * ttMicrosteps;

    uint32_t spdScale = speedPct > 0 ? speedPct : 1;

    if (ttStepper)
    {
        ttStepper->setSpeedInHz((uint32_t)((uint64_t)ttSpeed * ttMicrosteps * spdScale / 100));
        ttStepper->setAcceleration((uint32_t)((uint64_t)ttAccel * ttMicrosteps * spdScale / 100));
    }

    // Rescale threader position when microsteps change so the
    // absolute degree position stays the same.
    if (thStepper)
    {
        if (thMicrosteps != prevThMicrosteps && prevThMicrosteps != 0)
        {
            // Recompute position from the true angle (microstep-independent)
            int32_t newPos = (int32_t)lround(thTrueDeg / 360.0 * MOTOR_STEPS_PER_REV * thMicrosteps);
            thStepper->forceStopAndNewPosition(newPos);
            logMsg("TH rescaled: %u -> %u us, pos=%ld (%.3f deg)",
                   prevThMicrosteps, thMicrosteps, newPos, thTrueDeg);
        }
        prevThMicrosteps = thMicrosteps;

        thStepper->setSpeedInHz((uint32_t)((uint64_t)thSpeed * thMicrosteps * spdScale / 100));
        thStepper->setAcceleration((uint32_t)((uint64_t)thAccel * thMicrosteps * spdScale / 100));
    }

    logMsg("Recalc: %d pegs, %.1f steps/rev (ratio=%.4f)",
           numPegs, ttStepsPerRev, ttGearRatio);
}

void steppersInit()
{
    engine.init();

    ttStepper = engine.stepperConnectToPin(TT_STEP_PIN);
    if (ttStepper)
    {
        ttStepper->setDirectionPin(TT_DIR_PIN, false);
        ttStepper->setCurrentPosition(0);
    }

    thStepper = engine.stepperConnectToPin(TH_STEP_PIN);
    if (thStepper)
    {
        thStepper->setDirectionPin(TH_DIR_PIN);
        thStepper->setCurrentPosition(0);
    }

    pinMode(LIMIT_SW_PIN, INPUT_PULLUP);

    steppersRecalc();

    lastSwitchState = (digitalRead(LIMIT_SW_PIN) == LOW);
    debouncedSwitch = lastSwitchState;
    lastPollState = lastSwitchState;
    logMsg("Steppers initialised (TT=%d/%d, TH=%d/%d) LimitSW=%s",
           TT_STEP_PIN, TT_DIR_PIN, TH_STEP_PIN, TH_DIR_PIN,
           lastSwitchState ? "PRESSED" : "open");
}

// ─── Turntable ──────────────────────────────────────────────────────────────

uint16_t turntableCurrentSlot()
{
    // Derive slot from the true angle (no step-based rounding)
    double slotF = ttTrueDeg / 360.0 * numPegs;
    uint16_t slot = (uint16_t)lround(slotF);
    if (slot >= numPegs)
        slot = 0;
    return slot;
}

// Convert a true degree value to the nearest integer step within one revolution
static int32_t degreesToTTSteps(double deg)
{
    int32_t stepsPerRev = (int32_t)lround(ttStepsPerRev);
    int32_t s = (int32_t)lround(deg / 360.0 * ttStepsPerRev);
    if (s >= stepsPerRev)
        s -= stepsPerRev;
    if (s < 0)
        s += stepsPerRev;
    return s;
}

void turntableGoToSlot(uint16_t slot)
{
    if (slot >= numPegs)
        slot = numPegs - 1;

    int32_t stepsPerRev = (int32_t)lround(ttStepsPerRev);

    // Compute the exact target angle (double — no precision loss for ~300 moves)
    double newTrueDeg = (double)slot * 360.0 / (double)numPegs;
    int32_t targetStepNorm = degreesToTTSteps(newTrueDeg);

    // Current normalised position
    int32_t curNorm = ttPositionSteps % stepsPerRev;
    if (curNorm < 0)
        curNorm += stepsPerRev;

    // Shortest-path delta
    int32_t delta = targetStepNorm - curNorm;
    int32_t half = stepsPerRev / 2;
    if (delta > half)
        delta -= stepsPerRev;
    else if (delta < -half)
        delta += stepsPerRev;

    uint16_t fromSlot = turntableCurrentSlot();
    ttTrueDeg = newTrueDeg; // update true angle even if delta==0

    if (delta == 0)
        return;

    // Backlash compensation: add extra steps when direction reverses
    int8_t newDir = (delta > 0) ? 1 : -1;
    int32_t backlashSteps = 0;
    if (ttLastDir != 0 && newDir != ttLastDir && ttBacklashDeg > 0.0f)
    {
        backlashSteps = (int32_t)lround((double)ttBacklashDeg / 360.0 * ttStepsPerRev);
        if (newDir < 0)
            backlashSteps = -backlashSteps;
    }
    ttLastDir = newDir;

    ttPositionSteps += delta + backlashSteps;
    ttStepper->moveTo(ttPositionSteps);

    logMsg("TT: slot %u -> %u (%.3f deg, %ld steps %s)",
           fromSlot, slot, ttTrueDeg, labs(delta), delta > 0 ? "CW" : "CCW");
}

void turntableJog(float degrees)
{
    // Update true angle
    ttTrueDeg += (double)degrees;
    ttTrueDeg = fmod(ttTrueDeg, 360.0);
    if (ttTrueDeg < 0.0)
        ttTrueDeg += 360.0;

    // Jog always moves in the requested direction — compute steps directly
    int32_t steps = (int32_t)lround((double)degrees / 360.0 * ttStepsPerRev);
    if (steps == 0)
        return;

    // Backlash compensation on direction reversal
    int8_t newDir = (steps > 0) ? 1 : -1;
    int32_t backlashSteps = 0;
    if (ttLastDir != 0 && newDir != ttLastDir && ttBacklashDeg > 0.0f)
    {
        backlashSteps = (int32_t)lround((double)ttBacklashDeg / 360.0 * ttStepsPerRev);
        if (newDir < 0)
            backlashSteps = -backlashSteps;
    }
    ttLastDir = newDir;

    ttPositionSteps += steps + backlashSteps;
    ttStepper->moveTo(ttPositionSteps);
    logMsg("TT jog %.2f deg -> %.3f deg (%ld steps)", degrees, ttTrueDeg, steps);
}

void turntableZero()
{
    ttPositionSteps = 0;
    ttTrueDeg = 0.0;
    ttLastDir = 0;
    ttStepper->setCurrentPosition(0);
    logMsg("TT zeroed at current position");
}

// ─── Threader ───────────────────────────────────────────────────────────────

static int32_t thDegreesToSteps(double deg)
{
    return (int32_t)lround(deg / 360.0 * MOTOR_STEPS_PER_REV * thMicrosteps);
}

void threaderDown()
{
    if (!thStepper)
        return;
    thTrueDeg = (double)thDownDeg;
    int32_t target = thDegreesToSteps(thTrueDeg);
    thStepper->moveTo(target);
    logMsg("Threader DOWN (%.3f deg, %ld steps)", thTrueDeg, target);
}

void threaderUp()
{
    if (!thStepper)
        return;
    thTrueDeg = (double)thUpDeg;
    int32_t target = thDegreesToSteps(thTrueDeg);
    thStepper->moveTo(target);
    logMsg("Threader UP (%.3f deg, %ld steps)", thTrueDeg, target);
}

void threaderCenter()
{
    if (!thStepper)
        return;
    thTrueDeg = (double)thCenterDeg;
    int32_t target = thDegreesToSteps(thTrueDeg);
    thStepper->moveTo(target);
    logMsg("Threader CENTER (%.3f deg, %ld steps)", thTrueDeg, target);
}

void threaderGoToDown()
{
    threaderDown();
}

void threaderGoToUp()
{
    threaderUp();
}

void threaderGoToCenter()
{
    threaderCenter();
}

bool threaderIsHomed() { return _threaderHomed; }

void threaderHomeStart()
{
    if (!thStepper)
        return;

    // If switch is already pressed, back off first
    if (readSwitchDebounced())
    {
        _threaderHomed = false;
        homeState = HOME_BACKOFF;
        thStepper->setSpeedInHz(thHomeSpeed * thMicrosteps);
        thStepper->setAcceleration(thAccel * thMicrosteps);
        thStepper->move((int32_t)TH_HOME_BACKOFF * thMicrosteps);
        logMsg("Homing: switch already pressed, backing off first...");
        return;
    }

    _threaderHomed = false;
    homeState = HOME_SEEK;

    thStepper->setSpeedInHz(thHomeSpeed * thMicrosteps);
    thStepper->setAcceleration(thAccel * thMicrosteps);
    thStepper->move(-(int32_t)(MOTOR_STEPS_PER_REV * thMicrosteps));
    logMsg("Homing: seeking switch (speed=%lu)...", thHomeSpeed);
}

bool threaderHomingBusy()
{
    return homeState != HOME_IDLE;
}

void threaderHomePoll()
{
    if (homeState == HOME_IDLE)
        return;

    bool switchHit = readSwitchDebounced();

    switch (homeState)
    {
    case HOME_SEEK:
        if (switchHit)
        {
            thStepper->forceStopAndNewPosition(0);
            // Back off until switch releases
            thStepper->setSpeedInHz(thHomeSpeed * thMicrosteps);
            thStepper->move((int32_t)TH_HOME_BACKOFF * thMicrosteps);
            homeState = HOME_BACKOFF;
            logMsg("Homing: switch hit, backing off %d steps...", TH_HOME_BACKOFF);
        }
        else if (!thStepper->isRunning())
        {
            homeState = HOME_IDLE;
            logMsg("Homing FAILED: switch not found");
        }
        break;

    case HOME_BACKOFF:
        if (!thStepper->isRunning())
        {
            if (switchHit)
            {
                // Still on switch — back off more
                thStepper->move((int32_t)TH_HOME_BACKOFF * thMicrosteps);
                logMsg("Homing: still on switch, backing off more...");
            }
            else
            {
                // Switch released — slow approach at 20% speed
                uint32_t slowSpeed = (thHomeSpeed / 5);
                if (slowSpeed < 1)
                    slowSpeed = 1;
                thStepper->setSpeedInHz(slowSpeed * thMicrosteps);
                thStepper->move(-(int32_t)(TH_HOME_BACKOFF * 3) * thMicrosteps);
                homeState = HOME_SLOW;
                logMsg("Homing: slow approach at %lu steps/s...", slowSpeed);
            }
        }
        break;

    case HOME_SLOW:
        if (switchHit)
        {
            thStepper->forceStopAndNewPosition(0);
            _threaderHomed = true;
            thTrueDeg = 0.0;
            homeState = HOME_IDLE;
            // Restore normal speed
            thStepper->setSpeedInHz(thSpeed * thMicrosteps);
            logMsg("Homing complete");
        }
        else if (!thStepper->isRunning())
        {
            homeState = HOME_IDLE;
            logMsg("Homing FAILED: switch not found on slow approach");
        }
        break;

    default:
        break;
    }
}

// ─── General ────────────────────────────────────────────────────────────────

bool steppersMoving()
{
    return (ttStepper && ttStepper->isRunning()) ||
           (thStepper && thStepper->isRunning());
}

void steppersPollLimitSwitch()
{
    bool pressed = readSwitchDebounced();
    if (pressed != lastPollState)
    {
        lastPollState = pressed;
        logMsg("Limit switch %s", pressed ? "PRESSED" : "released");

        // Unexpected press while running (not homing) → emergency pause
        if (pressed && homeState == HOME_IDLE && commandsState() == STATE_RUNNING)
        {
            if (thStepper && thStepper->isRunning())
                thStepper->forceStop();
            commandsPause();
            logMsg("PAUSED: threader home switch hit unexpectedly");
        }
    }
}

double turntableTrueDeg() { return ttTrueDeg; }
double threaderTrueDeg() { return thTrueDeg; }
