#include "setfuncs.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

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
void applyLidarCalibrationProfile()
{
  // Re-apply library-side distance correction after sensor state changes.
  lidar.setDistanceScale(LIDAR_LIBRARY_DISTANCE_SCALE);
  lidar.setDistanceOffset(LIDAR_LIBRARY_DISTANCE_OFFSET_MM);
}

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
  cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES, lens_distance_cm, sizeof(lens_distance_cm));
}

void clearLidarDisplay()
{
  snprintf(distance_cm, sizeof(distance_cm), "...");
  lidar_quality_level = 0;
}
} // namespace

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  static LidarRecoveryState recoveryState = {false, false, 0, 0, 0};

  if (!lidarSensorReady || !lidarEnabled)
  {
    recoveryState = {false, false, 0, 0, 0};
    return;
  }

  const unsigned long now = millis();

  DTSError lidarUpdateError = static_cast<DTSError>(lidar.update());
  last_lidar_error_code = static_cast<int>(lidarUpdateError);
  if (lidarUpdateError == DTSError::NONE)
  {
    DTSMeasurement measurement = lidar.getMeasurement();

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm);
    if (!chosen.valid)
    {
      // Keep main-branch behavior: do not force recovery on filtered/noisy frames.
      if ((now - recoveryState.last_valid_measurement_ms) > LIDAR_NO_DATA_TIMEOUT_MS)
      {
        clearLidarDisplay();
      }
      return;
    }

    updateLidarRecoveryState(recoveryState, LidarRecoveryEvent::VALID_MEASUREMENT, now);
    lidar_quality_level = chosen.quality_level;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || strcmp(distance_cm, "...") == 0 || strcmp(distance_cm, "Zzz") == 0)
    {
      formatDistanceDisplay(distance, distance_cm, sizeof(distance_cm));
      prev_distance = distance;
    }
    return;
  }

  LidarRecoveryEvent event = (lidarUpdateError == DTSError::TIMEOUT)
                                 ? LidarRecoveryEvent::TIMEOUT
                                 : LidarRecoveryEvent::ERROR;
  LidarRecoveryDecision recoveryDecision = updateLidarRecoveryState(recoveryState, event, now);

  if (recoveryDecision.clear_display)
  {
    clearLidarDisplay();
  }

  if (!recoveryDecision.attempt_recovery)
  {
    return;
  }

  lidar_recovery_count++;
  lidar.clearError();
  DTSError resetStatus = lidar.resetState();
  DTSError enableStatus = static_cast<DTSError>(lidar.enableSensor());
  bool recovered = (resetStatus == DTSError::NONE && enableStatus == DTSError::NONE);
  if (recovered)
  {
    applyLidarCalibrationProfile();
  }
  noteLidarRecoveryAttemptResult(recoveryState, recovered, now);
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  static bool spikeFilterInitialized = false;
  static int stableReading = 0;
  static int pendingReading = 0;
  static uint8_t pendingCount = 0;

  if (!adsReady)
  {
    return stableReading;
  }

  if (LENS_ADC_QUIET_DELAY_MS > 0)
  {
    delay(LENS_ADC_QUIET_DELAY_MS);
  }

  long adcSampleTotal = 0;
  for (int i = 0; i < LENS_ADC_SAMPLE_COUNT; i++)
  {
    adcSampleTotal += theads.readADC_SingleEnded(LENS_ADC_PIN);
    if (LENS_ADC_SAMPLE_DELAY_US > 0)
    {
      delayMicroseconds(LENS_ADC_SAMPLE_DELAY_US);
    }
  }

  int sensorVal = static_cast<int>(adcSampleTotal / LENS_ADC_SAMPLE_COUNT);
  if (ui_mode == UiMode::Main)
  {
    sensorVal += LENS_ADC_MAIN_OFFSET;
  }

  int smoothedReading = calcMovingAvg(sensorVal);
  if (!spikeFilterInitialized)
  {
    spikeFilterInitialized = true;
    stableReading = smoothedReading;
    pendingReading = smoothedReading;
    pendingCount = 0;
    return stableReading;
  }

  if (abs(smoothedReading - stableReading) <= LENS_SPIKE_DELTA_THRESHOLD)
  {
    stableReading = smoothedReading;
    pendingCount = 0;
    return stableReading;
  }

  if (pendingCount == 0 || abs(smoothedReading - pendingReading) > LENS_SPIKE_DELTA_THRESHOLD)
  {
    pendingReading = smoothedReading;
    pendingCount = 1;
    return stableReading;
  }

  pendingCount++;
  if (pendingCount >= LENS_SPIKE_CONFIRMATION_COUNT)
  {
    stableReading = smoothedReading;
    pendingCount = 0;
  }

  return stableReading;
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
    snprintf(lens_distance_cm, sizeof(lens_distance_cm), "Inf.");
    return;
  }

  setLensDistanceFromCm(estimate.distance_cm);
}

void setFilmCounter()
{
  static EncoderFilterState encoderFilterState = {};
  static bool rawPositionInitialized = false;
  static int lastRawEncoderPosition = 0;
  const unsigned long now = millis();

  if (!encoderReady)
  {
    return;
  }

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
  }
  else
  {
    int rawDelta = encoder_position - lastRawEncoderPosition;
    if (abs(rawDelta) >= FILM_COUNTER_ACTIVITY_MIN_DELTA)
    {
      registerActivity();
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

  FilmCounterEstimate estimate = estimateFilmCounter(
      film_formats[selected_format],
      encoder_value,
      frame_one_offset,
      frame_spacing_offset);
  if (!estimate.valid)
  {
    return;
  }

  film_counter = estimate.frame;
  frame_progress = estimate.progress;
  savePrefs(false, PREFS_DIRTY_FILM);
}

void setVoltage()
{
  if (!batteryGaugeReady)
  {
    return;
  }

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

  if (!lightMeterReady)
  {
    lux = 0.0f;
    ev_readout = NAN;
    snprintf(shutter_speed, sizeof(shutter_speed), "No meter");
    return;
  }

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

  formatShutterSpeed(lux, aperture, iso, shutter_speed, sizeof(shutter_speed));
  prev_lux = lux;
  prev_iso = iso;
  prev_aperture = aperture;
}

void toggleLidar(bool lidarStatusParam)
{
  if (!lidarSensorReady)
  {
    lidarEnabled = false;
    return;
  }

  if (lidarStatusParam == lidarEnabled)
  {
    return;
  }

  DTSError status = lidarStatusParam ? static_cast<DTSError>(lidar.enableSensor())
                                     : static_cast<DTSError>(lidar.disableSensor());
  if (status == DTSError::NONE)
  {
    if (lidarStatusParam)
    {
      applyLidarCalibrationProfile();
    }
    lidarEnabled = lidarStatusParam;
  }
}
// ---------------------
