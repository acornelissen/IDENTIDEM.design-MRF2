#ifndef FILM_COUNTER_LOGIC_H
#define FILM_COUNTER_LOGIC_H

#include "formats.h"

struct FilmCounterEstimate
{
  bool valid;
  int frame;
  float progress;
};

struct EncoderFilterState
{
  bool initialized;
  int stable_position;
  int candidate_position;
  int candidate_direction;
  unsigned long candidate_since_ms;
};

struct EncoderFilterDecision
{
  bool accepted;
  int accepted_position;
};

FilmCounterEstimate estimateFilmCounter(
    const FilmFormat &film_format,
    int encoder_value,
    int frame_one_offset = 0,
    int frame_spacing_offset = 0);
void resetEncoderFilterState(EncoderFilterState &state, int initial_position, unsigned long now_ms);
EncoderFilterDecision updateEncoderFilter(
    EncoderFilterState &state, int raw_position, unsigned long now_ms, bool allow_rewind);

#endif // FILM_COUNTER_LOGIC_H
