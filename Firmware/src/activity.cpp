#include "activity.h"

#include <Arduino.h>

#include "globals.h"
#include "mrfconstants.h"
#include "cyclefuncs.h"

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
  static bool bootGraceInitialized = false;
  static unsigned long bootGraceStartMs = 0;
  if (!bootGraceInitialized)
  {
    bootGraceInitialized = true;
    bootGraceStartMs = now_ms;
  }

  // Clamp invalid activity timestamps to avoid accidental immediate sleep.
  if (lastActivityTime > now_ms)
  {
    lastActivityTime = now_ms;
  }

  // Never enter sleep during the initial post-boot grace period.
  if ((now_ms - bootGraceStartMs) < SLEEP_BOOT_GRACE_MS)
  {
    sleepMode = false;
    return;
  }

  // Keep the device awake while navigating menus/calibration flows.
  if (ui_mode != UiMode::Main)
  {
    return;
  }

  unsigned long sleepTimeoutMs = getSleepTimeoutModeMs(sleep_timeout_mode);
  if (sleepTimeoutMs == 0)
  {
    sleepMode = false;
    return;
  }

  if (now_ms - lastActivityTime > sleepTimeoutMs)
  {
    sleepMode = true;
  }
}

unsigned long getIdleDurationMs(unsigned long now_ms)
{
  return (now_ms > lastActivityTime) ? (now_ms - lastActivityTime) : 0;
}
