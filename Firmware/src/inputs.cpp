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
          cycleCalibLenses();
        }
        else if (calib_step == 1)
        {
          long sum = 0;
          for (int i = 0; i < CALIB_SAMPLE_COUNT; i++)
          {
            sum += getLensSensorReading();
            delay(CALIB_SAMPLE_DELAY_MS);
          }
          int averagedReading = static_cast<int>(sum / CALIB_SAMPLE_COUNT);
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
          if (calib_step == 0) calib_step = 1;
          else if (calib_step == 1) {
            calib_step = 0; ui_mode = "config";
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
