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
#include "calibration_logic.h"

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
bool captureStableCalibReading(int &averagedReading)
{
  int samples[CALIB_SAMPLE_COUNT];
  for (int i = 0; i < CALIB_SAMPLE_COUNT; i++)
  {
    samples[i] = getLensSensorReading();
    delay(CALIB_SAMPLE_DELAY_MS);
  }

  return computeStableCalibrationReading(
      samples,
      CALIB_SAMPLE_COUNT,
      CALIB_OUTLIER_MAX_DELTA,
      CALIB_MIN_INLIER_COUNT,
      CALIB_INLIER_SPREAD_MAX,
      averagedReading);
}

bool isMonotonicCalibSequenceWithCandidate(int candidateReading)
{
  int readingCount = current_calib_distance + 1;
  int readings[CALIB_DISTANCE_COUNT];
  for (int i = 0; i < current_calib_distance; i++)
  {
    readings[i] = calib_distance_set[i];
  }
  readings[current_calib_distance] = candidateReading;

  return validateMonotonicCalibration(readings, readingCount, CALIB_MONOTONIC_MIN_STEP);
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
        if (config_step > CONFIG_ROOT_STEP_MAX)
        {
          config_step = 0;
        }
      }
      else if (ui_mode == "config_lens")
      {
        config_step++;
        if (config_step > CONFIG_LENS_STEP_MAX)
        {
          config_step = 0;
        }
      }
      else if (ui_mode == "config_meter")
      {
        config_step++;
        if (config_step > CONFIG_METER_STEP_MAX)
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
              config_step = CONFIG_LENS_STEP_CALIB;
              ui_mode = "config_lens";
            }
          }
        }
      }
      else if (ui_mode == "reset_confirm")
      {
        ui_mode = "config";
        config_step = CONFIG_ROOT_STEP_RESET;
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
          if (config_step == CONFIG_ROOT_STEP_ISO) cycleISOs();
          else if (config_step == CONFIG_ROOT_STEP_FORMAT) cycleFormats();
          else if (config_step == CONFIG_ROOT_STEP_SLEEP_TIMEOUT) {
            cycleSleepTimeoutMode();
          }
          else if (config_step == CONFIG_ROOT_STEP_LENS_MENU) {
            config_step = CONFIG_LENS_STEP_LENS;
            ui_mode = "config_lens";
          }
          else if (config_step == CONFIG_ROOT_STEP_METER_MENU) {
            config_step = CONFIG_METER_STEP_EV_COMP;
            ui_mode = "config_meter";
          }
          else if (config_step == CONFIG_ROOT_STEP_RESET) {
            ui_mode = "reset_confirm";
          }
          else if (config_step == CONFIG_ROOT_STEP_EXIT) {
            ui_mode = "main"; config_step = 0;
          }
        }
        else if (ui_mode == "config_lens")
        {
          if (config_step == CONFIG_LENS_STEP_LENS) {
            cycleLenses();
            int non_zero_aperture_index = getFirstNonZeroAperture();
            if (non_zero_aperture_index < 0) non_zero_aperture_index = 0;
            aperture = lenses[selected_lens].apertures[non_zero_aperture_index];
            aperture_index = non_zero_aperture_index;
          }
          else if (config_step == CONFIG_LENS_STEP_PARALLAX) {
            parallaxEnabled = !parallaxEnabled;
            savePrefs();
          }
          else if (config_step == CONFIG_LENS_STEP_CALIB) {
            calib_step = 0;
            calib_lens = selected_lens; // Use current selected lens for calibration
            current_calib_distance = 0;
            calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
            memset(calib_distance_set, 0, sizeof(calib_distance_set));
            ui_mode = "calib";
          }
          else if (config_step == CONFIG_LENS_STEP_BACK) {
            config_step = CONFIG_ROOT_STEP_LENS_MENU;
            ui_mode = "config";
          }
        }
        else if (ui_mode == "config_meter")
        {
          if (config_step == CONFIG_METER_STEP_EV_COMP) {
            cycleExposureCompensation(CycleDirection::Up);
          }
          else if (config_step == CONFIG_METER_STEP_SMOOTHING) {
            cycleMeterSmoothing();
          }
          else if (config_step == CONFIG_METER_STEP_EV_READOUT) {
            toggleEvReadout();
          }
          else if (config_step == CONFIG_METER_STEP_BACK) {
            config_step = CONFIG_ROOT_STEP_METER_MENU;
            ui_mode = "config";
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
            config_step = CONFIG_LENS_STEP_CALIB;
            ui_mode = "config_lens";
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
