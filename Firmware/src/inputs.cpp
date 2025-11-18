#include "inputs.h"

#include <Arduino.h>
#include <string.h> // For memset

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h"
#include "helpers.h"
#include "cyclefuncs.h"

// Functions to check and act on button presses
// ---------------------
void checkButtons()
{
  lbutton.update();
  if (lbutton.rose() && lbutton.previousDuration() < 1000)
  {
    lastActivityTime = millis();
    if (sleepMode == true) {
      sleepMode = false;
    }
    else {
      if (ui_mode == "main")
      {
        cycleApertures("down");
      }
      else if (ui_mode == "config")
      {
        config_step++;
        if (config_step > 5) // MAX_CONFIG_STEPS might be better
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
        if (calib_step == 1) // This should likely be an else if
        {
          calib_distance_set[current_calib_distance] = lens_sensor_reading;
          current_calib_distance++;
          if (current_calib_distance >= sizeof(CALIB_DISTANCES) / sizeof(CALIB_DISTANCES[0]))
          {
            lenses[calib_lens].calibrated = true;
            for (int i = 0; i < sizeof(calib_distance_set) / sizeof(calib_distance_set[0]); i++)
            {
              lenses[calib_lens].sensor_reading[i] = calib_distance_set[i];
            }
            savePrefs();
            selected_lens = calib_lens; // Ensure this is the desired behavior
            ui_mode = "config";
          }
        }
      }
    }
  }

  rbutton.update();
  if (rbutton.isPressed() && rbutton.currentDuration() >= 3000) 
  {
    lastActivityTime = millis();
    if (ui_mode == "main")
    {
      ui_mode = "config";
    }
  }
  else if (rbutton.rose() && rbutton.previousDuration() < 1000)
  {
    lastActivityTime = millis();
    if (sleepMode == true) {
      sleepMode = false;
    }
    else {
        if (ui_mode == "main")
        {
          cycleApertures("up");
        }
        else if (ui_mode == "config")
        {
          if (config_step == 0) cycleISOs();
          else if (config_step == 1) cycleFormats();
          else if (config_step == 2) {
            cycleLenses();
            int non_zero_aperture_index = getFirstNonZeroAperture();
            aperture = lenses[selected_lens].apertures[non_zero_aperture_index];
            aperture_index = non_zero_aperture_index;
          }
          else if (config_step == 3) {
            calib_step = 0;
            calib_lens = selected_lens; // Use current selected lens for calibration
            current_calib_distance = 0;
            memset(calib_distance_set, 0, sizeof(calib_distance_set));
            ui_mode = "calib";
          }
          else if (config_step == 4) {
            encoder.setEncoderPosition(0); // Reset encoder related values
            encoder_value = 0; prev_encoder_value = 0;
            film_counter = 0; frame_progress = 0; prev_frame_progress = 0;
            savePrefs();
            ui_mode = "main"; config_step = 0;
          }
          else if (config_step == 5) {
            ui_mode = "main"; config_step = 0;
          }
        }
        else if (ui_mode == "calib")
        {
          if (calib_step == 0) calib_step = 1;
          else if (calib_step == 1) { // This should likely be an else if
            calib_step = 0; ui_mode = "config";
          }
        }
    }
  }
}
// ---------------------