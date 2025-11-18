#ifndef FORMATS_H
#define FORMATS_H

#include <Arduino.h> // For String type
#include <stddef.h> // For size_t
struct FilmFormat
{
  int id;
  String name;
  int sensor[22];
  int frame[22];
  int frame_fill[4];
};

extern FilmFormat film_formats[];
extern const size_t NUM_FILM_FORMATS; // Declare the size constant

#endif // FORMATS_H
