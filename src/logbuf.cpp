#include "config.h"
#include "logbuf.h"
#include <Arduino.h>
#include <stdarg.h>

static LogEntry ring[LOG_BUF_SIZE];
static uint16_t head = 0;   // next write position in ring[]
static uint32_t nextId = 1; // monotonically increasing id
static uint16_t count = 0;  // entries currently in buffer (≤ LOG_BUF_SIZE)

void logMsg(const char *fmt, ...)
{
    LogEntry &e = ring[head];
    e.id = nextId++;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.msg, LOG_MSG_LEN, fmt, ap);
    va_end(ap);

    head = (head + 1) % LOG_BUF_SIZE;
    if (count < LOG_BUF_SIZE)
        count++;

    // Also echo to serial (useful for initial IP discovery)
    Serial.println(e.msg);
}

uint16_t logGetSince(uint32_t sinceId, LogEntry *out, uint16_t maxOut, uint32_t *latestId)
{
    uint16_t written = 0;
    *latestId = (nextId > 1) ? (nextId - 1) : 0;

    if (count == 0 || maxOut == 0)
        return 0;

    // oldest entry index in the ring
    uint16_t oldest;
    if (count < LOG_BUF_SIZE)
        oldest = 0;
    else
        oldest = head; // head is where the next write goes, so it's the oldest

    for (uint16_t i = 0; i < count && written < maxOut; i++)
    {
        uint16_t idx = (oldest + i) % LOG_BUF_SIZE;
        if (ring[idx].id > sinceId)
        {
            out[written++] = ring[idx];
        }
    }
    return written;
}
