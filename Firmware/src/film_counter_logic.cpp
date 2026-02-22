#include "film_counter_logic.h"

#include <Arduino.h>

#include "mrfconstants.h"

FilmCounterEstimate estimateFilmCounter(
    const FilmFormat &film_format,
    int encoder_value,
    int frame_one_offset,
    int frame_spacing_offset)
{
  FilmCounterEstimate result = {false, 0, 0.0f};

  int frame_count = sizeof(film_format.sensor) / sizeof(film_format.sensor[0]);
  while (frame_count > 1 && film_format.sensor[frame_count - 1] == 0 && film_format.frame[frame_count - 1] == 0)
  {
    frame_count--;
  }
  if (frame_count < 2)
  {
    return result;
  }
  const int last_frame_index = frame_count - 1;
  int adjusted_sensor_points[22] = {};
  adjusted_sensor_points[0] = film_format.sensor[0];
  for (int i = 1; i <= last_frame_index; i++)
  {
    long adjusted = static_cast<long>(film_format.sensor[i]) +
                    static_cast<long>(frame_one_offset) +
                    static_cast<long>(frame_spacing_offset) * static_cast<long>(i - 1);
    int minAllowed = adjusted_sensor_points[i - 1] + 1;
    if (adjusted < minAllowed)
    {
      adjusted = minAllowed;
    }
    adjusted_sensor_points[i] = static_cast<int>(adjusted);
  }

  if (encoder_value >= adjusted_sensor_points[last_frame_index])
  {
    result.valid = true;
    result.frame = FILM_COUNTER_END;
    return result;
  }

  for (int i = 0; i < frame_count; i++)
  {
    if (adjusted_sensor_points[i] == encoder_value)
    {
      result.valid = true;
      result.frame = film_format.frame[i];
      return result;
    }
  }

  for (int i = 0; i < last_frame_index; i++)
  {
    if (adjusted_sensor_points[i] < encoder_value && encoder_value < adjusted_sensor_points[i + 1])
    {
      result.valid = true;
      if (abs(encoder_value - adjusted_sensor_points[i + 1]) <= FILM_COUNTER_SNAP_THRESHOLD)
      {
        result.frame = film_format.frame[i + 1];
        result.progress = 0.0f;
      }
      else
      {
        result.frame = film_format.frame[i];
        result.progress = static_cast<float>(encoder_value - adjusted_sensor_points[i]) /
                          static_cast<float>(adjusted_sensor_points[i + 1] - adjusted_sensor_points[i]);
      }
      return result;
    }
  }

  return result;
}

void resetEncoderFilterState(EncoderFilterState &state, int initial_position, unsigned long now_ms)
{
  state.initialized = true;
  state.stable_position = max(0, initial_position);
  state.candidate_position = state.stable_position;
  state.candidate_direction = 0;
  state.candidate_since_ms = now_ms;
}

EncoderFilterDecision updateEncoderFilter(
    EncoderFilterState &state, int raw_position, unsigned long now_ms, bool allow_rewind)
{
  if (!state.initialized)
  {
    resetEncoderFilterState(state, raw_position, now_ms);
  }

  raw_position = max(0, raw_position);
  int delta = raw_position - state.stable_position;
  if (delta == 0)
  {
    state.candidate_direction = 0;
    state.candidate_position = state.stable_position;
    state.candidate_since_ms = now_ms;
    return {false, state.stable_position};
  }

  int direction = (delta > 0) ? 1 : -1;
  if (direction < 0 && !allow_rewind)
  {
    state.candidate_direction = 0;
    state.candidate_position = state.stable_position;
    state.candidate_since_ms = now_ms;
    return {false, state.stable_position};
  }

  int hysteresis = (direction > 0) ? FILM_COUNTER_FORWARD_HYSTERESIS : FILM_COUNTER_REWIND_HYSTERESIS;
  unsigned long debounceMs =
      (direction > 0) ? FILM_COUNTER_FORWARD_DEBOUNCE_MS : FILM_COUNTER_REWIND_DEBOUNCE_MS;
  if (abs(delta) < hysteresis)
  {
    state.candidate_direction = 0;
    state.candidate_position = state.stable_position;
    state.candidate_since_ms = now_ms;
    return {false, state.stable_position};
  }

  if (state.candidate_direction != direction)
  {
    state.candidate_direction = direction;
    state.candidate_position = raw_position;
    state.candidate_since_ms = now_ms;
    return {false, state.stable_position};
  }

  state.candidate_position = raw_position;
  if ((now_ms - state.candidate_since_ms) < debounceMs)
  {
    return {false, state.stable_position};
  }

  state.stable_position = state.candidate_position;
  state.candidate_direction = 0;
  state.candidate_position = state.stable_position;
  state.candidate_since_ms = now_ms;
  return {true, state.stable_position};
}
