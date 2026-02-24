#ifndef FORMATS_H
#define FORMATS_H

#include <Arduino.h> // For String type
#include <stddef.h> // For size_t

constexpr size_t FILM_FORMAT_POINT_CAPACITY = 24;

struct FilmFormat
{
  int id;
  String name;
  int sensor[FILM_FORMAT_POINT_CAPACITY];
  int frame[FILM_FORMAT_POINT_CAPACITY];
  float frame_mm_width;
  float frame_mm_height;
};

extern FilmFormat film_formats[];
extern const size_t NUM_FILM_FORMATS; // Declare the size constant
int getFilmFormatPointCount(const FilmFormat &film_format);
int getFilmFormatMaxFrame(const FilmFormat &film_format);

#endif // FORMATS_H
