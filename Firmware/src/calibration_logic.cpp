#include "calibration_logic.h"

#include <Arduino.h>
#include "mrfconstants.h"

namespace
{
void sortAscending(int *values, int count)
{
  for (int i = 1; i < count; i++)
  {
    int key = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > key)
    {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = key;
  }
}
} // namespace

bool computeStableCalibrationReading(
    const int *samples,
    int sample_count,
    int outlier_max_delta,
    int min_inlier_count,
    int max_inlier_spread,
    int &averaged_reading)
{
  if (!samples || sample_count <= 0)
  {
    return false;
  }

  int sorted[CALIB_SAMPLE_COUNT];
  if (sample_count > static_cast<int>(sizeof(sorted) / sizeof(sorted[0])))
  {
    return false;
  }

  for (int i = 0; i < sample_count; i++)
  {
    sorted[i] = samples[i];
  }
  sortAscending(sorted, sample_count);
  const int median = sorted[(sample_count - 1) / 2];

  long inlierSum = 0;
  int inlierCount = 0;
  int maxDeviationFromMedian = 0;
  for (int i = 0; i < sample_count; i++)
  {
    if (abs(samples[i] - median) <= outlier_max_delta)
    {
      inlierSum += samples[i];
      inlierCount++;
      int deviation = abs(samples[i] - median);
      maxDeviationFromMedian = max(maxDeviationFromMedian, deviation);
    }
  }

  if (inlierCount < min_inlier_count)
  {
    return false;
  }
  // Check max distance from median rather than total min-to-max spread.
  // This is more forgiving of gentle monotonic lens drift where all samples
  // trend in one direction but each individual reading stays close to centre.
  if (maxDeviationFromMedian > max_inlier_spread)
  {
    return false;
  }

  averaged_reading = static_cast<int>((inlierSum + (inlierCount / 2)) / inlierCount);
  return true;
}

bool validateMonotonicCalibration(const int *readings, int reading_count, int min_step)
{
  if (!readings || reading_count <= 1)
  {
    return true;
  }

  int direction = 0;
  for (int i = 1; i < reading_count; i++)
  {
    int delta = readings[i] - readings[i - 1];
    if (abs(delta) < min_step)
    {
      return false;
    }

    int stepDirection = (delta > 0) ? 1 : -1;
    if (direction == 0)
    {
      direction = stepDirection;
    }
    else if (stepDirection != direction)
    {
      return false;
    }
  }

  return true;
}
