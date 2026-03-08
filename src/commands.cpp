#include "config.h"
#include "commands.h"
#include "steppers.h"
#include "logbuf.h"
#include <Arduino.h>
#include <string.h>
#include <ctype.h>

static uint16_t cmds[MAX_COMMANDS];
static uint16_t cmdCount = 0;
static uint16_t cmdIndex = 0;
static MachineState state = STATE_IDLE;
static bool waitingForMotor = false;
static bool waitingForDelay = false;
static unsigned long delayStartMs = 0;

float cmdDelaySec = CMD_DELAY_DEFAULT; // inter-command delay (seconds)
uint8_t speedPct = SPEED_PCT_DEFAULT;  // speed multiplier percentage

void commandsInit()
{
    memset(cmds, 0, sizeof(cmds));
    cmdCount = 0;
    cmdIndex = 0;
    state = STATE_IDLE;
    waitingForMotor = false;
    waitingForDelay = false;
}

bool commandsParse(const char *csv, uint16_t csvLen, char *errorMsg, uint16_t errorMsgLen)
{
    uint16_t count = 0;
    const char *p = csv;
    const char *end = csv + csvLen;

    while (p < end && count < MAX_COMMANDS)
    {
        // Skip whitespace and commas
        while (p < end && (*p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            p++;
        if (p >= end)
            break;

        if (*p == 'D' || *p == 'd')
        {
            cmds[count++] = CMD_THREADER_DOWN;
            p++;
        }
        else if (*p == 'U' || *p == 'u')
        {
            cmds[count++] = CMD_THREADER_UP;
            p++;
        }
        else if (*p == 'C' || *p == 'c')
        {
            cmds[count++] = CMD_THREADER_CENTER;
            p++;
        }
        else if (isdigit((unsigned char)*p))
        {
            uint16_t val = 0;
            while (p < end && isdigit((unsigned char)*p))
            {
                val = val * 10 + (*p - '0');
                p++;
            }
            if (val >= numPegs)
            {
                snprintf(errorMsg, errorMsgLen,
                         "Slot %u at command %u exceeds peg count %u",
                         val, count, numPegs);
                return false;
            }
            cmds[count++] = val;
        }
        else
        {
            snprintf(errorMsg, errorMsgLen,
                     "Unexpected char '%c' at position %u", *p, (uint16_t)(p - csv));
            return false;
        }
    }

    if (count >= MAX_COMMANDS && p < end)
    {
        // Check there's more data remaining
        while (p < end && (*p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            p++;
        if (p < end)
        {
            snprintf(errorMsg, errorMsgLen,
                     "Too many commands (max %u)", MAX_COMMANDS);
            return false;
        }
    }

    if (count == 0)
    {
        snprintf(errorMsg, errorMsgLen, "No commands found");
        return false;
    }

    cmdCount = count;
    cmdIndex = 0;
    state = STATE_IDLE;
    waitingForMotor = false;
    logMsg("Uploaded %u commands", cmdCount);
    return true;
}

void commandsStart()
{
    if (cmdCount == 0)
    {
        logMsg("No commands loaded");
        return;
    }
    cmdIndex = 0;
    waitingForMotor = false;
    waitingForDelay = false;
    state = STATE_RUNNING;
    logMsg("Execution started (%u commands)", cmdCount);
}

void commandsPause()
{
    if (state == STATE_RUNNING)
    {
        state = STATE_PAUSED;
        logMsg("Paused at command %u/%u", cmdIndex, cmdCount);
    }
}

void commandsResume()
{
    if (state == STATE_PAUSED)
    {
        state = STATE_RUNNING;
        logMsg("Resumed at command %u/%u", cmdIndex, cmdCount);
    }
}

void commandsStop()
{
    state = STATE_IDLE;
    cmdIndex = 0;
    waitingForMotor = false;
    waitingForDelay = false;
    logMsg("Stopped");
}

void commandsPoll()
{
    if (state != STATE_RUNNING)
        return;

    // If we dispatched a command and motors are still running, wait.
    if (waitingForMotor)
    {
        if (steppersMoving())
            return;
        waitingForMotor = false;

        // Start inter-command delay if configured
        if (cmdDelaySec > 0.0f)
        {
            waitingForDelay = true;
            delayStartMs = millis();
            return;
        }
        cmdIndex++;
    }

    // Wait for inter-command delay to elapse
    if (waitingForDelay)
    {
        if ((millis() - delayStartMs) < (unsigned long)(cmdDelaySec * 1000.0f))
            return;
        waitingForDelay = false;
        cmdIndex++;
    }

    // Check if we've finished
    if (cmdIndex >= cmdCount)
    {
        state = STATE_IDLE;
        logMsg("Execution complete (%u commands)", cmdCount);
        return;
    }

    // Dispatch next command
    uint16_t cmd = cmds[cmdIndex];
    if (cmd == CMD_THREADER_DOWN)
    {
        threaderDown();
    }
    else if (cmd == CMD_THREADER_UP)
    {
        threaderUp();
    }
    else if (cmd == CMD_THREADER_CENTER)
    {
        threaderCenter();
    }
    else
    {
        turntableGoToSlot(cmd);
    }
    waitingForMotor = true;
}

MachineState commandsState() { return state; }
uint16_t commandsIndex() { return cmdIndex; }
uint16_t commandsCount() { return cmdCount; }
