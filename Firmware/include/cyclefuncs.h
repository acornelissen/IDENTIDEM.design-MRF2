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
// ---------------------

#endif // CYCLEFUNCS_H
