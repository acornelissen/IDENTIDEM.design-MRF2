#include "setfuncs.h"

#include <Arduino.h>

#include "activity.h"
#include "cyclefuncs.h"
#include "film_counter_logic.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "lens_logic.h"
#include "lenses.h"
#include "lidar_logic.h"
#include "lidar_recovery_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"
#include "formats.h"

namespace
{
bool getLensPriorCm(int &lens_prior_cm)
{
  if (!lenses[selected_lens].calibrated)
  {
    return false;
  }

  if (lens_distance_raw <= 0 || lens_distance_raw == LENS_INFINITY_RAW)
  {
    return false;
  }

  lens_prior_cm = lens_distance_raw;
  return true;
}

void setLensDistanceFromCm(int distance_cm)
{
  lens_distance_raw = distance_cm;
  lens_distance_cm = cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES);
}

void clearLidarDisplay()
{
  if (distance_cm != "...")
  {
    distance_cm = "...";
  }
  lidar_quality_level = 0;
}
} // namespace

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  static LidarRecoveryState recoveryState = {false, false, 0, 0, 0};

  if (!lidarEnabled)
  {
    recoveryState = {false, false, 0, 0, 0};
    return;
  }

  const unsigned long now = millis();
  LidarRecoveryDecision recoveryDecision = updateLidarRecoveryState(
      recoveryState, LidarRecoveryEvent::NO_VALID_MEASUREMENT, now);

  DTSError lidarUpdateError = static_cast<DTSError>(lidar.update());
  if (lidarUpdateError == DTSError::NONE)
  {
    DTSMeasurement measurement = lidar.getMeasurement();

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm);
    if (!chosen.valid)
    {
      recoveryDecision = updateLidarRecoveryState(
          recoveryState, LidarRecoveryEvent::NO_VALID_MEASUREMENT, now);
      if (recoveryDecision.clear_display)
      {
        clearLidarDisplay();
      }
      return;
    }

    updateLidarRecoveryState(recoveryState, LidarRecoveryEvent::VALID_MEASUREMENT, now);
    lidar_quality_level = chosen.quality_level;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || distance_cm == "...")
    {
      distance_cm = formatDistanceDisplay(distance);
      prev_distance = distance;
    }
    return;
  }

  LidarRecoveryEvent event = (lidarUpdateError == DTSError::TIMEOUT)
                                 ? LidarRecoveryEvent::TIMEOUT
                                 : LidarRecoveryEvent::ERROR;
  recoveryDecision = updateLidarRecoveryState(recoveryState, event, now);

  if (recoveryDecision.clear_display)
  {
    clearLidarDisplay();
  }

  if (!recoveryDecision.attempt_recovery)
  {
    return;
  }

  lidar.clearError();
  DTSError resetStatus = lidar.resetState();
  DTSError enableStatus = static_cast<DTSError>(lidar.enableSensor());
  bool recovered = (resetStatus == DTSError::NONE && enableStatus == DTSError::NONE);
  noteLidarRecoveryAttemptResult(recoveryState, recovered, now);
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  if (LENS_ADC_QUIET_DELAY_MS > 0)
  {
    delay(LENS_ADC_QUIET_DELAY_MS);
  }

  long sampleTotal = 0;
  for (int i = 0; i < LENS_ADC_SAMPLE_COUNT; i++)
  {
    sampleTotal += theads.readADC_SingleEnded(LENS_ADC_PIN);
    if (LENS_ADC_SAMPLE_DELAY_US > 0)
    {
      delayMicroseconds(LENS_ADC_SAMPLE_DELAY_US);
    }
  }

  int sensorVal = static_cast<int>(sampleTotal / LENS_ADC_SAMPLE_COUNT);
  if (ui_mode == "main")
  {
    sensorVal += LENS_ADC_MAIN_OFFSET;
  }

  return calcMovingAvg(sensorVal);
}

void setLensDistance()
{
  static int prevSnapIndex = -1;
  static int prevSnapLens = -1;

  const Lens &lens = lenses[selected_lens];

  if (selected_lens != prevSnapLens)
  {
    prevSnapLens = selected_lens;
    prevSnapIndex = -1;
  }

  if (lens_sensor_reading == prev_lens_sensor_reading)
  {
    return;
  }

  int prevReading = prev_lens_sensor_reading;
  prev_lens_sensor_reading = lens_sensor_reading;

  bool activityDetected = abs(lens_sensor_reading - prevReading) > LENS_ACTIVITY_THRESHOLD;
  int snapIndex = -1;

  if (lens.calibrated)
  {
    snapIndex = findLensSnapIndex(lens, lens_sensor_reading);
    if (snapIndex >= 0 && snapIndex != prevSnapIndex)
    {
      activityDetected = true;
    }
    prevSnapIndex = snapIndex;
  }
  else
  {
    prevSnapIndex = -1;
  }

  if (activityDetected)
  {
    registerActivity();
  }

  if (lens.calibrated && snapIndex >= 0)
  {
    setLensDistanceFromCm(static_cast<int>(lens.distance[snapIndex] * CM_PER_METER));
    return;
  }

  LensDistanceEstimate estimate = estimateLensDistance(lens, lens_sensor_reading);
  if (!estimate.valid)
  {
    return;
  }

  if (estimate.is_infinity)
  {
    lens_distance_raw = LENS_INFINITY_RAW;
    lens_distance_cm = "Inf.";
    return;
  }

  setLensDistanceFromCm(estimate.distance_cm);
}

void setFilmCounter()
{
  static EncoderFilterState encoderFilterState = {};
  static bool rawPositionInitialized = false;
  static int lastRawEncoderPosition = 0;
  static unsigned long lastRawEncoderActivityMs = 0;
  const unsigned long now = millis();
  if (!encoderFilterState.initialized || encoderFilterState.stable_position != prev_encoder_value)
  {
    resetEncoderFilterState(encoderFilterState, prev_encoder_value, now);
    rawPositionInitialized = false;
  }

  int encoder_position = encoder.getEncoderPosition();

  if (!rawPositionInitialized)
  {
    rawPositionInitialized = true;
    lastRawEncoderPosition = encoder_position;
    lastRawEncoderActivityMs = now;
  }
  else
  {
    int rawDelta = encoder_position - lastRawEncoderPosition;
    if (abs(rawDelta) >= FILM_COUNTER_ACTIVITY_MIN_DELTA)
    {
      if ((now - lastRawEncoderActivityMs) >= FILM_COUNTER_ACTIVITY_DEBOUNCE_MS)
      {
        registerActivity();
        lastRawEncoderActivityMs = now;
      }
      lastRawEncoderPosition = encoder_position;
    }
  }

  EncoderFilterDecision decision =
      updateEncoderFilter(encoderFilterState, encoder_position, now, FILM_COUNTER_ALLOW_REWIND);
  if (!decision.accepted)
  {
    return;
  }

  registerActivity();

  encoder_value = decision.accepted_position;
  prev_encoder_value = encoder_value;

  FilmCounterEstimate estimate = estimateFilmCounter(film_formats[selected_format], encoder_value);
  if (!estimate.valid)
  {
    return;
  }

  film_counter = estimate.frame;
  frame_progress = estimate.progress;
  savePrefs();
}

void setVoltage()
{
  bat_per = maxlipo.cellPercent();
  if (bat_per > BATTERY_PERCENT_MAX)
  {
    bat_per = BATTERY_PERCENT_MAX;
  }

  if (bat_per != prev_bat_per)
  {
    prev_bat_per = bat_per;
  }
}

void setLightMeter()
{
  static bool smoothingInitialized = false;
  static float smoothedLux = 0.0f;

  float rawLux = lightMeter.readLightLevel();
  if (rawLux < 0.0f)
  {
    rawLux = 0.0f;
  }

  float alpha = getMeterSmoothingAlpha(meter_smoothing_mode);
  if (!smoothingInitialized || alpha >= 1.0f)
  {
    smoothedLux = rawLux;
    smoothingInitialized = true;
  }
  else
  {
    smoothedLux = (rawLux * alpha) + (smoothedLux * (1.0f - alpha));
  }

  float exposureCompEv = static_cast<float>(exposure_comp_thirds) / 3.0f;
  lux = applyExposureCompensationToLux(smoothedLux, exposureCompEv);
  ev_readout = calculateEV100(smoothedLux);

  if (aperture == 0 && lux > 0)
  {
    cycleApertures(CycleDirection::Up);
  }

  shutter_speed = formatShutterSpeed(lux, aperture, iso);
  prev_lux = lux;
  prev_iso = iso;
  prev_aperture = aperture;
}

void toggleLidar(bool lidarStatusParam)
{
  if (lidarStatusParam == lidarEnabled)
  {
    return;
  }

  DTSError status = lidarStatusParam ? static_cast<DTSError>(lidar.enableSensor())
                                     : static_cast<DTSError>(lidar.disableSensor());
  if (status == DTSError::NONE)
  {
    lidarEnabled = lidarStatusParam;
  }
}
// ---------------------
