#ifndef LENS_LOGIC_H
#define LENS_LOGIC_H

#include "lenses.h"

struct LensDistanceEstimate
{
  bool valid;
  bool is_infinity;
  int distance_cm;
};

int findLensSnapIndex(const Lens &lens, int sensor_reading);
LensDistanceEstimate estimateLensDistance(const Lens &lens, int sensor_reading);

#endif // LENS_LOGIC_H
