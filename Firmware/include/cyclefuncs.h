#ifndef CYCLEFUNCS_H
#define CYCLEFUNCS_H

enum class CycleDirection
{
  Up,
  Down
};

// Functions to cycle values
// ---------------------
void cycleApertures(CycleDirection direction);
void cycleISOs();
void cycleLenses();
void cycleCalibLenses();
void cycleFormats();
void cycleExposureCompensation(CycleDirection direction);
void cycleMeterSmoothing();
void toggleEvReadout();
void cycleSleepTimeoutMode();
const char *getSleepTimeoutModeLabel(int timeout_mode);
unsigned long getSleepTimeoutModeMs(int timeout_mode);
// ---------------------

#endif // CYCLEFUNCS_H
