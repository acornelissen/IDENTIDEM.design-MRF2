#ifndef FILM_COUNTER_LOGIC_H
#define FILM_COUNTER_LOGIC_H

#include "formats.h"

struct FilmCounterEstimate
{
  bool valid;
  int frame;
  float progress;
};

FilmCounterEstimate estimateFilmCounter(const FilmFormat &film_format, int encoder_value);

#endif // FILM_COUNTER_LOGIC_H
