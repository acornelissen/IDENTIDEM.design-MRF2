#ifndef LENSES_H
#define LENSES_H

#include <stddef.h> // For size_t

constexpr int LENS_DISTANCE_POINT_COUNT = 10;
constexpr int LENS_APERTURE_COUNT = 9;

struct Lens
{
  int id;
  const char *name;
  float focal_mm;
  int sensor_reading[LENS_DISTANCE_POINT_COUNT];
  float distance[LENS_DISTANCE_POINT_COUNT];
  float apertures[LENS_APERTURE_COUNT];
  int framelines[4];
  bool calibrated;
};

extern Lens lenses[];
extern const size_t NUM_LENSES; // Declare the size constant
int getLensDistancePointCount(const Lens &lens);

#endif // LENSES_H
