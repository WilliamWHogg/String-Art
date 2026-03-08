#pragma once
#include <stdint.h>

// Ring-buffer log that replaces serial monitor.
// Call logMsg() from anywhere; the web UI polls /api/log to read entries.

void logMsg(const char *fmt, ...);

struct LogEntry
{
    uint32_t id;
    char msg[LOG_MSG_LEN];
};

// Returns entries with id > sinceId.  Writes up to maxOut entries into out[].
// Returns the number actually written.  *latestId is set to the newest id.
uint16_t logGetSince(uint32_t sinceId, LogEntry *out, uint16_t maxOut, uint32_t *latestId);
