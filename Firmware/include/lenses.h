#ifndef LENSES_H
#define LENSES_H

#include <Arduino.h> // For String type
#include <stddef.h> // For size_t
struct Lens
{
  int id;
  String name;
  int sensor_reading[7];
  float distance[7];
  float apertures[9];
  int framelines[4];
  bool calibrated;
};

extern Lens lenses[];
extern const size_t NUM_LENSES; // Declare the size constant

#endif // LENSES_H