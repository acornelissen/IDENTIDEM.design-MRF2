#ifndef CYCLEFUNCS_H
#define CYCLEFUNCS_H

#include <Arduino.h> // For String type used in cycleApertures

// Functions to cycle values
// ---------------------
void cycleApertures(String direction);
void cycleISOs();
void cycleLenses();
void cycleCalibLenses();
void cycleFormats();
// ---------------------

#endif // CYCLEFUNCS_H