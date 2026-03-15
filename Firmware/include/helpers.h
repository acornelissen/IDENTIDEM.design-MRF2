#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h> // For Arduino utility macros and int_fast16_t typedefs
#include <stdint.h>
#include <stddef.h>

enum PrefsDirtyGroup : uint8_t
{
  PREFS_DIRTY_SETTINGS = 1U << 0,
  PREFS_DIRTY_FILM = 1U << 1,
  PREFS_DIRTY_LENS_CAL = 1U << 2,
  PREFS_DIRTY_ALL = PREFS_DIRTY_SETTINGS | PREFS_DIRTY_FILM | PREFS_DIRTY_LENS_CAL
};
// Helper functions
// ---------------------
int getFirstNonZeroAperture();

void loadPrefs();

void savePrefs(bool force = false, uint8_t dirtyMask = PREFS_DIRTY_ALL);
void flushPrefsIfDirty();
void performFactoryReset();

void cmToReadable(int cm, int places, char *buffer, size_t bufferSize);

int calcMovingAvg(int sensorVal);

int_fast16_t getFocusRadius();
// ---------------------

#endif // HELPERS_H
