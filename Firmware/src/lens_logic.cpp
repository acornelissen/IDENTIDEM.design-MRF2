#include "lens_logic.h"

#include <Arduino.h>

#include "mrfconstants.h"

int findLensSnapIndex(const Lens &lens, int sensor_reading)
{
  const int reading_count = sizeof(lens.sensor_reading) / sizeof(lens.sensor_reading[0]);
  int snap_index = -1;
  int snap_delta = max(LENS_SNAP_DEADZONE, LENS_SNAP_DEADZONE_FAR) + 1;

  for (int i = 0; i < reading_count; i++)
  {
    int delta = abs(sensor_reading - lens.sensor_reading[i]);
    int snap_deadzone = (lens.distance[i] >= LENS_SNAP_FAR_DISTANCE_M) ? LENS_SNAP_DEADZONE_FAR : LENS_SNAP_DEADZONE;
    if (delta <= snap_deadzone && delta < snap_delta)
    {
      snap_delta = delta;
      snap_index = i;
    }
  }

  return snap_index;
}

LensDistanceEstimate estimateLensDistance(const Lens &lens, int sensor_reading)
{
  const int reading_count = sizeof(lens.sensor_reading) / sizeof(lens.sensor_reading[0]);
  const int last_index = reading_count - 1;

  LensDistanceEstimate result = {false, false, 0};

  if (sensor_reading < lens.sensor_reading[0])
  {
    result.valid = true;
    result.distance_cm = static_cast<int>(lens.distance[0] * CM_PER_METER);
    return result;
  }

  const int last_sensor = lens.sensor_reading[last_index];
  if (sensor_reading > last_sensor + LENS_INF_THRESHOLD)
  {
    result.valid = true;
    result.is_infinity = true;
    result.distance_cm = LENS_INFINITY_RAW;
    return result;
  }

  for (int i = 0; i < reading_count; i++)
  {
    if (sensor_reading == lens.sensor_reading[i])
    {
      result.valid = true;
      result.distance_cm = static_cast<int>(lens.distance[i] * CM_PER_METER);
      return result;
    }
  }

  for (int i = 0; i < last_index; i++)
  {
    const int left_sensor = lens.sensor_reading[i];
    const int right_sensor = lens.sensor_reading[i + 1];
    if (left_sensor < sensor_reading && sensor_reading < right_sensor)
    {
      const float left_distance = lens.distance[i];
      const float right_distance = lens.distance[i + 1];
      float interpolated = left_distance +
                           (sensor_reading - left_sensor) * (right_distance - left_distance) / (right_sensor - left_sensor);
      result.valid = true;
      result.distance_cm = static_cast<int>(interpolated * CM_PER_METER);
      return result;
    }
  }

  if (sensor_reading >= last_sensor)
  {
    result.valid = true;
    result.distance_cm = static_cast<int>(lens.distance[last_index] * CM_PER_METER);
    return result;
  }

  return result;
}
