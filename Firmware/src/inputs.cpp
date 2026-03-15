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
#include "interface.h"

// Functions to check and act on button presses
// ---------------------
static void resetFrameCounter()
{
  if (encoderReady)
  {
    encoder.setEncoderPosition(0); // Reset encoder related values
  }
  encoder_value = 0;
  prev_encoder_value = 0;
  film_counter = 0;
  frame_progress = 0;
  prev_frame_progress = 0;
  savePrefs(true);
}

namespace
{
int getCalibrationPointCountForLens(const Lens &lens)
{
  int pointCount = getLensDistancePointCount(lens);
  if (pointCount <= 0)
  {
    pointCount = CALIB_DISTANCE_COUNT;
  }
  return min(pointCount, CALIB_DISTANCE_COUNT);
}

bool captureStableCalibReading(int &averagedReading)
{
  int calibSamples[CALIB_SAMPLE_COUNT];
  for (int i = 0; i < CALIB_SAMPLE_COUNT; i++)
  {
    calibSamples[i] = getLensSensorReading();
    delay(CALIB_SAMPLE_DELAY_MS);
  }

  return computeStableCalibrationReading(
      calibSamples,
      CALIB_SAMPLE_COUNT,
      CALIB_OUTLIER_MAX_DELTA,
      CALIB_MIN_INLIER_COUNT,
      CALIB_INLIER_SPREAD_MAX,
      averagedReading);
}

void advanceMenuStep(int maxStep)
{
  if (++config_step > maxStep)
  {
    config_step = 0;
  }
}

bool isMonotonicCalibSequenceWithCandidate(int candidateReading)
{
  const int calibrationPointCount = getCalibrationPointCountForLens(lenses[calib_lens]);
  if (current_calib_distance >= calibrationPointCount)
  {
    return false;
  }

  int readingCount = current_calib_distance + 1;
  int readings[CALIB_DISTANCE_COUNT];
  for (int i = 0; i < current_calib_distance; i++)
  {
    readings[i] = calib_distance_set[i];
  }
  readings[current_calib_distance] = candidateReading;

  return validateMonotonicCalibration(readings, readingCount, CALIB_MONOTONIC_MIN_STEP);
}

void handleLeftButtonShortPress()
{
  bool wasSleeping = sleepMode;
  registerActivity();
  if (wasSleeping)
  {
    return;
  }

  if (ui_mode == UiMode::Main)
  {
    cycleApertures(CycleDirection::Down);
  }
  else if (ui_mode == UiMode::Config)
  {
    advanceMenuStep(CONFIG_ROOT_STEP_MAX);
  }
  else if (ui_mode == UiMode::ConfigFilm)
  {
    advanceMenuStep(CONFIG_FILM_STEP_MAX);
  }
  else if (ui_mode == UiMode::ConfigLens)
  {
    advanceMenuStep(CONFIG_LENS_STEP_MAX);
  }
  else if (ui_mode == UiMode::ConfigMeter)
  {
    advanceMenuStep(CONFIG_METER_STEP_MAX);
  }
  else if (ui_mode == UiMode::ConfigUi)
  {
    advanceMenuStep(CONFIG_UI_STEP_MAX);
  }
  else if (ui_mode == UiMode::Calib)
  {
    if (calib_step == 0)
    {
      calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
      cycleCalibLenses();
    }
    else if (calib_step == 1)
    {
      // Do not allow a new capture attempt while the previous error is
      // still being held on screen for the minimum display time.
      if (calib_capture_status != CALIB_CAPTURE_STATUS_NONE &&
          (millis() - calib_capture_status_ms) < CALIB_ERROR_HOLD_MS)
      {
        // Ignore this press; error message still visible.
      }
      else
      {
        const int calibrationPointCount = getCalibrationPointCountForLens(lenses[calib_lens]);
        int averagedReading = 0;
        if (!captureStableCalibReading(averagedReading))
        {
          calib_capture_status = CALIB_CAPTURE_STATUS_UNSTABLE;
          calib_capture_status_ms = millis();
        }
        else if (!isMonotonicCalibSequenceWithCandidate(averagedReading))
        {
          calib_capture_status = CALIB_CAPTURE_STATUS_NON_MONOTONIC;
          calib_capture_status_ms = millis();
        }
        else
        {
          calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
          calib_distance_set[current_calib_distance] = averagedReading;
          current_calib_distance++;

          // Brief green LED pulse to confirm successful capture.
          // Invalidate prev_frame_progress so the next external UI draw
          // restores the correct progress colour.
          if (statusPixelReady)
          {
            sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(0, 255, 0));
            sspixel.show();
            delay(80);
            prev_frame_progress = -1.0f;
          }
          if (current_calib_distance >= calibrationPointCount)
          {
            lenses[calib_lens].calibrated = true;
            const int sensorPointCount = sizeof(lenses[calib_lens].sensor_reading) /
                                         sizeof(lenses[calib_lens].sensor_reading[0]);
            for (int i = 0; i < sensorPointCount; i++)
            {
              lenses[calib_lens].sensor_reading[i] = (i < calibrationPointCount) ? calib_distance_set[i] : 0;
            }
            selected_lens = calib_lens;
            savePrefs(true);

            // Show full-screen success and pulse LED before leaving calibration.
            drawCalibCompleteUI();

            if (statusPixelReady)
            {
              for (int i = 0; i < CALIB_COMPLETE_LED_PULSES; i++)
              {
                sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(0, 255, 0));
                sspixel.show();
                delay(CALIB_COMPLETE_LED_ON_MS);
                sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_OFF_R, NEOPIXEL_OFF_G, NEOPIXEL_OFF_B));
                sspixel.show();
                if (i < CALIB_COMPLETE_LED_PULSES - 1)
                {
                  delay(CALIB_COMPLETE_LED_OFF_MS);
                }
              }
              prev_frame_progress = -1.0f;
            }

            delay(CALIB_COMPLETE_HOLD_MS);
            config_step = CONFIG_LENS_STEP_CALIB;
            ui_mode = UiMode::ConfigLens;
          }
        }
      }
    }
  }
  else if (ui_mode == UiMode::ResetConfirm)
  {
    ui_mode = UiMode::Config;
    config_step = CONFIG_ROOT_STEP_RESET;
  }
  else if (ui_mode == UiMode::Health)
  {
    ui_mode = UiMode::Config;
    config_step = CONFIG_ROOT_STEP_HEALTH;
  }
  else if (ui_mode == UiMode::FactoryResetConfirm)
  {
    ui_mode = UiMode::Health;
  }
}

void handleRightButtonLongPress()
{
  registerActivity();
  if (ui_mode == UiMode::Main)
  {
    ui_mode = UiMode::Config;
  }
  else if (ui_mode == UiMode::Health)
  {
    ui_mode = UiMode::FactoryResetConfirm;
  }
}

void handleRightButtonShortPress()
{
  bool wasSleeping = sleepMode;
  registerActivity();
  if (wasSleeping)
  {
    return;
  }

  if (ui_mode == UiMode::Main)
  {
    cycleApertures(CycleDirection::Up);
  }
  else if (ui_mode == UiMode::Config)
  {
    if (config_step == CONFIG_ROOT_STEP_FILM_MENU) {
      config_step = CONFIG_FILM_STEP_FORMAT;
      ui_mode = UiMode::ConfigFilm;
    }
    else if (config_step == CONFIG_ROOT_STEP_LENS_MENU) {
      config_step = CONFIG_LENS_STEP_LENS;
      ui_mode = UiMode::ConfigLens;
    }
    else if (config_step == CONFIG_ROOT_STEP_METER_MENU) {
      config_step = CONFIG_METER_STEP_ISO;
      ui_mode = UiMode::ConfigMeter;
    }
    else if (config_step == CONFIG_ROOT_STEP_UI_MENU) {
      config_step = CONFIG_UI_STEP_HORIZON_LANDSCAPE;
      ui_mode = UiMode::ConfigUi;
    }
    else if (config_step == CONFIG_ROOT_STEP_RESET) {
      ui_mode = UiMode::ResetConfirm;
    }
    else if (config_step == CONFIG_ROOT_STEP_HEALTH) {
      ui_mode = UiMode::Health;
    }
    else if (config_step == CONFIG_ROOT_STEP_EXIT) {
      ui_mode = UiMode::Main;
      config_step = 0;
    }
  }
  else if (ui_mode == UiMode::ConfigFilm)
  {
    if (config_step == CONFIG_FILM_STEP_FORMAT)
    {
      cycleFormats();
    }
    else if (config_step == CONFIG_FILM_STEP_CURRENT_FRAME)
    {
      cycleCurrentFrame();
    }
    else if (config_step == CONFIG_FILM_STEP_FRAME_ONE_OFFSET)
    {
      cycleFrameOneOffset();
    }
    else if (config_step == CONFIG_FILM_STEP_FRAME_SPACING)
    {
      cycleFrameSpacingOffset();
    }
    else if (config_step == CONFIG_FILM_STEP_BACK)
    {
      config_step = CONFIG_ROOT_STEP_FILM_MENU;
      ui_mode = UiMode::Config;
    }
  }
  else if (ui_mode == UiMode::ConfigLens)
  {
    if (config_step == CONFIG_LENS_STEP_LENS) {
      cycleLenses();
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
      ui_mode = UiMode::Calib;
    }
    else if (config_step == CONFIG_LENS_STEP_BACK) {
      config_step = CONFIG_ROOT_STEP_LENS_MENU;
      ui_mode = UiMode::Config;
    }
  }
  else if (ui_mode == UiMode::ConfigMeter)
  {
    if (config_step == CONFIG_METER_STEP_ISO) {
      cycleISOs();
    }
    else if (config_step == CONFIG_METER_STEP_EV_COMP) {
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
      ui_mode = UiMode::Config;
    }
  }
  else if (ui_mode == UiMode::ConfigUi)
  {
    if (config_step == CONFIG_UI_STEP_HORIZON_LANDSCAPE) {
      cycleLevelTrimLandscape();
    }
    else if (config_step == CONFIG_UI_STEP_HORIZON_PORTRAIT_POS) {
      cycleLevelTrimPortraitPos();
    }
    else if (config_step == CONFIG_UI_STEP_HORIZON_PORTRAIT_NEG) {
      cycleLevelTrimPortraitNeg();
    }
    else if (config_step == CONFIG_UI_STEP_SLEEP_TIMEOUT) {
      cycleSleepTimeoutMode();
    }
    else if (config_step == CONFIG_UI_STEP_LIDAR_IDLE_TIMEOUT) {
      cycleLidarIdleTimeoutMode();
    }
    else if (config_step == CONFIG_UI_STEP_BACK) {
      config_step = CONFIG_ROOT_STEP_UI_MENU;
      ui_mode = UiMode::Config;
    }
  }
  else if (ui_mode == UiMode::Calib)
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
      ui_mode = UiMode::ConfigLens;
    }
  }
  else if (ui_mode == UiMode::ResetConfirm)
  {
    resetFrameCounter();
    ui_mode = UiMode::Main;
    config_step = 0;
  }
  else if (ui_mode == UiMode::Health)
  {
    if (!lidarSensorReady)
    {
      retryLidarInit();
    }
    else
    {
      ui_mode = UiMode::Config;
      config_step = CONFIG_ROOT_STEP_HEALTH;
    }
  }
  else if (ui_mode == UiMode::FactoryResetConfirm)
  {
    performFactoryReset();
  }
}
} // namespace

void checkButtons()
{
  lbutton.update();
  if (lbutton.rose() && lbutton.previousDuration() < BUTTON_SHORT_PRESS_MAX_MS)
  {
    handleLeftButtonShortPress();
  }

  rbutton.update();
  if (rbutton.isPressed() && rbutton.currentDuration() >= BUTTON_LONG_PRESS_MIN_MS)
  {
    handleRightButtonLongPress();
  }
  else if (rbutton.rose() && rbutton.previousDuration() < BUTTON_SHORT_PRESS_MAX_MS)
  {
    handleRightButtonShortPress();
  }
}
// ---------------------
