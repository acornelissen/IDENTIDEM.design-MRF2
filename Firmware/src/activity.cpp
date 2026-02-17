#include "activity.h"

#include <Arduino.h>

#include "globals.h"
#include "mrfconstants.h"

void registerActivity()
{
  lastActivityTime = millis();
  if (sleepMode)
  {
    sleepMode = false;
  }
}

void updateSleepMode(unsigned long now_ms)
{
  if (now_ms - lastActivityTime > SLEEPTIMEOUT)
  {
    sleepMode = true;
  }
}
