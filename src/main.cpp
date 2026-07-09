#include <Arduino.h>
#include "config.h"
#include "logbuf.h"
#include "steppers.h"
#include "commands.h"
#include "webserver.h"

void setup()
{
  Serial.begin(115200);

  logMsg("String Art Controller booting...");

  steppersInit();
  commandsInit();
  webserverInit();

  logMsg("Ready — waiting for commands");
}

void loop()
{
  // Advance homing state machine if active
  if (threaderHomingBusy())
  {
    threaderHomePoll();
  }

  // Advance command execution state machine
  commandsPoll();

  // Log limit switch press/release
  steppersPollLimitSwitch();
}
