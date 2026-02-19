#include "inputs.h"

#include <Arduino.h>
#include <string.h> // For memset

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h"
#include "helpers.h"
#include "cyclefuncs.h"
#include "setfuncs.h"
#include "activity.h"

// Functions to check and act on button presses
// ---------------------
static void resetFrameCounter()
{
  encoder.setEncoderPosition(0); // Reset encoder related values
  encoder_value = 0;
  prev_encoder_value = 0;
  film_counter = 0;
  frame_progress = 0;
  prev_frame_progress = 0;
  savePrefs(true);
}

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

bool captureStableCalibReading(int &averagedReading)
{
  int samples[CALIB_SAMPLE_COUNT];
  for (int i = 0; i < CALIB_SAMPLE_COUNT; i++)
  {
    samples[i] = getLensSensorReading();
    delay(CALIB_SAMPLE_DELAY_MS);
  }

  int sortedSamples[CALIB_SAMPLE_COUNT];
  memcpy(sortedSamples, samples, sizeof(samples));
  sortAscending(sortedSamples, CALIB_SAMPLE_COUNT);
  const int median = sortedSamples[CALIB_SAMPLE_COUNT / 2];

  long inlierSum = 0;
  int inlierCount = 0;
  int minInlier = 32767;
  int maxInlier = -32768;

  for (int i = 0; i < CALIB_SAMPLE_COUNT; i++)
  {
    if (abs(samples[i] - median) <= CALIB_OUTLIER_MAX_DELTA)
    {
      inlierSum += samples[i];
      inlierCount++;
      minInlier = min(minInlier, samples[i]);
      maxInlier = max(maxInlier, samples[i]);
    }
  }

  if (inlierCount < CALIB_MIN_INLIER_COUNT)
  {
    return false;
  }

  if ((maxInlier - minInlier) > CALIB_INLIER_SPREAD_MAX)
  {
    return false;
  }

  averagedReading = static_cast<int>((inlierSum + (inlierCount / 2)) / inlierCount);
  return true;
}

bool isMonotonicCalibSequenceWithCandidate(int candidateReading)
{
  if (current_calib_distance == 0)
  {
    return true;
  }

  int direction = 0;
  for (int i = 1; i <= current_calib_distance; i++)
  {
    int previous = calib_distance_set[i - 1];
    int current = (i == current_calib_distance) ? candidateReading : calib_distance_set[i];
    int delta = current - previous;

    if (abs(delta) < CALIB_MONOTONIC_MIN_STEP)
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
} // namespace

void checkButtons()
{
  lbutton.update();
  if (lbutton.rose() && lbutton.previousDuration() < BUTTON_SHORT_PRESS_MAX_MS)
  {
    bool wasSleeping = sleepMode;
    registerActivity();
    if (!wasSleeping)
    {
      if (ui_mode == "main")
      {
        cycleApertures(CycleDirection::Down);
      }
      else if (ui_mode == "config")
      {
        config_step++;
        if (config_step > CONFIG_STEP_MAX)
        {
          config_step = 0;
        }
      }
      else if (ui_mode == "calib")
      {
        if (calib_step == 0)
        {
          calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
          cycleCalibLenses();
        }
        else if (calib_step == 1)
        {
          int averagedReading = 0;
          if (!captureStableCalibReading(averagedReading))
          {
            calib_capture_status = CALIB_CAPTURE_STATUS_UNSTABLE;
          }
          else if (!isMonotonicCalibSequenceWithCandidate(averagedReading))
          {
            calib_capture_status = CALIB_CAPTURE_STATUS_NON_MONOTONIC;
          }
          else
          {
            calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
            calib_distance_set[current_calib_distance] = averagedReading;
            current_calib_distance++;
            if (current_calib_distance >= CALIB_DISTANCE_COUNT)
            {
              lenses[calib_lens].calibrated = true;
              for (int i = 0; i < sizeof(calib_distance_set) / sizeof(calib_distance_set[0]); i++)
              {
                lenses[calib_lens].sensor_reading[i] = calib_distance_set[i];
              }
              selected_lens = calib_lens;
              savePrefs(true);
              ui_mode = "config";
            }
          }
        }
      }
      else if (ui_mode == "reset_confirm")
      {
        ui_mode = "config";
        config_step = 5;
      }
    }
  }

  rbutton.update();
  if (rbutton.isPressed() && rbutton.currentDuration() >= BUTTON_LONG_PRESS_MIN_MS) 
  {
    registerActivity();
    if (ui_mode == "main")
    {
      ui_mode = "config";
    }
  }
  else if (rbutton.rose() && rbutton.previousDuration() < BUTTON_SHORT_PRESS_MAX_MS)
  {
    bool wasSleeping = sleepMode;
    registerActivity();
    if (!wasSleeping)
    {
        if (ui_mode == "main")
        {
          cycleApertures(CycleDirection::Up);
        }
        else if (ui_mode == "config")
        {
          if (config_step == 0) cycleISOs();
          else if (config_step == 1) cycleFormats();
          else if (config_step == 2) {
            cycleLenses();
            int non_zero_aperture_index = getFirstNonZeroAperture();
            if (non_zero_aperture_index < 0) non_zero_aperture_index = 0;
            aperture = lenses[selected_lens].apertures[non_zero_aperture_index];
            aperture_index = non_zero_aperture_index;
          }
          else if (config_step == 3) {
            parallaxEnabled = !parallaxEnabled;
            savePrefs();
          }
          else if (config_step == 4) {
            calib_step = 0;
            calib_lens = selected_lens; // Use current selected lens for calibration
            current_calib_distance = 0;
            calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
            memset(calib_distance_set, 0, sizeof(calib_distance_set));
            ui_mode = "calib";
          }
          else if (config_step == 5) {
            ui_mode = "reset_confirm";
          }
          else if (config_step == 6) {
            ui_mode = "main"; config_step = 0;
          }
        }
        else if (ui_mode == "calib")
        {
          if (calib_step == 0)
          {
            calib_step = 1;
            calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
          }
          else if (calib_step == 1) {
            calib_step = 0;
            calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
            ui_mode = "config";
          }
        }
        else if (ui_mode == "reset_confirm")
        {
          resetFrameCounter();
          ui_mode = "main";
          config_step = 0;
        }
    }
  }
}
// ---------------------
