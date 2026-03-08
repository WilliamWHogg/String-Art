#pragma once
#include <stdint.h>

enum MachineState : uint8_t
{
    STATE_IDLE = 0,
    STATE_RUNNING = 1,
    STATE_PAUSED = 2,
    STATE_HOMING = 3
};

// Initialise command storage (zeroes array).
void commandsInit();

// Parse CSV text ("42,D,87,U,...") into command array.
// Returns true on success, false on parse error.  errorMsg filled on failure.
bool commandsParse(const char *csv, uint16_t csvLen, char *errorMsg, uint16_t errorMsgLen);

// Execution control
void commandsStart();
void commandsPause();
void commandsResume();
void commandsStop();

// Call every loop() iteration — advances to next command when motors idle.
void commandsPoll();

// Accessors
MachineState commandsState();
uint16_t commandsIndex();
uint16_t commandsCount();

// Debug controls (adjustable at runtime via web)
extern float cmdDelaySec; // delay between commands (seconds)
extern uint8_t speedPct;  // speed multiplier 1-100 %
