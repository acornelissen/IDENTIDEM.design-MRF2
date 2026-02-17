#include "film_counter_logic.h"

#include <Arduino.h>

#include "mrfconstants.h"

FilmCounterEstimate estimateFilmCounter(const FilmFormat &film_format, int encoder_value)
{
  FilmCounterEstimate result = {false, 0, 0.0f};

  const int frame_count = sizeof(film_format.sensor) / sizeof(film_format.sensor[0]);
  const int last_frame_index = frame_count - 1;

  if (encoder_value >= film_format.sensor[last_frame_index])
  {
    result.valid = true;
    result.frame = FILM_COUNTER_END;
    return result;
  }

  for (int i = 0; i < frame_count; i++)
  {
    if (film_format.sensor[i] == encoder_value)
    {
      result.valid = true;
      result.frame = film_format.frame[i];
      return result;
    }
  }

  for (int i = 0; i < last_frame_index; i++)
  {
    if (film_format.sensor[i] < encoder_value && encoder_value < film_format.sensor[i + 1])
    {
      result.valid = true;
      if (abs(encoder_value - film_format.sensor[i + 1]) <= FILM_COUNTER_SNAP_THRESHOLD)
      {
        result.frame = film_format.frame[i + 1];
        result.progress = 0.0f;
      }
      else
      {
        result.frame = film_format.frame[i];
        result.progress = static_cast<float>(encoder_value - film_format.sensor[i]) /
                          static_cast<float>(film_format.sensor[i + 1] - film_format.sensor[i]);
      }
      return result;
    }
  }

  return result;
}
