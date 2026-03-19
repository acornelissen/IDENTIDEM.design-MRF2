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
struct LensSpikeFilterState
{
  bool initialized = false;
  int stableReading = 0;
  int pendingReading = 0;
  uint8_t pendingCount = 0;
};

struct LensSnapState
{
  int prevIndex = -1;
  int prevLens = -1;
};

struct LightMeterSmoothingState
{
  bool initialized = false;
  float smoothedLux = 0.0f;
};

LensSpikeFilterState lensSpikeFilter;
LensSnapState lensSnap;
LightMeterSmoothingState lightMeterSmoothing;
LidarRecoveryState lidarRecoveryState = {};

void applyLidarCalibrationProfile()
{
  // Re-apply frame rate and library-side distance correction after sensor state changes.
  lidar.setFrameRate(LIDAR_FRAME_RATE_FPS);
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

} // namespace

void clearLidarDisplay(const char *placeholder)
{
  snprintf(distance_cm, sizeof(distance_cm), "%s", placeholder);
  lidar_quality_level = 0;
}

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  if (!lidarSensorReady || !lidarEnabled)
  {
    lidarRecoveryState = {};
    return;
  }

  const unsigned long now = millis();

  DTSResult lidarUpdateResult = lidar.update();
  last_lidar_error_code = static_cast<int>(static_cast<DTSError>(lidarUpdateResult));
  if (lidar.newDataAvailable())
  {
    DTSMeasurement measurement = lidar.getMeasurement();

    // Use the library's median-filtered distance to reduce jitter at near/mid range.
    uint16_t filtered_mm = lidar.getFilteredDistance();
    if (filtered_mm != DTS_INVALID_DISTANCE && measurement.primaryDistance_mm != DTS_INVALID_DISTANCE)
    {
      measurement.primaryDistance_mm = filtered_mm;
    }

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm);
    if (!chosen.valid)
    {
      // Keep main-branch behavior: do not force recovery on filtered/noisy frames.
      if ((now - lidarRecoveryState.last_valid_measurement_ms) > LIDAR_NO_DATA_TIMEOUT_MS)
      {
        clearLidarDisplay(prev_distance >= LIDAR_FAR_SIGNAL_LOSS_CM ? "Inf." : "...");
      }
      return;
    }

    updateLidarRecoveryState(lidarRecoveryState, LidarRecoveryEvent::VALID_MEASUREMENT, now);
    lidar_quality_level = chosen.quality_level;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || strcmp(distance_cm, "...") == 0 || strcmp(distance_cm, "Inf.") == 0 || strcmp(distance_cm, "Zzz") == 0)
    {
      formatDistanceDisplay(distance, distance_cm, sizeof(distance_cm));
      prev_distance = distance;
    }
    return;
  }

  DTSError lidarUpdateError = static_cast<DTSError>(lidarUpdateResult);
  LidarRecoveryEvent event = (lidarUpdateError == DTSError::TIMEOUT)
                                 ? LidarRecoveryEvent::TIMEOUT
                                 : LidarRecoveryEvent::ERROR;
  LidarRecoveryDecision recoveryDecision = updateLidarRecoveryState(lidarRecoveryState, event, now);

  if (recoveryDecision.clear_display)
  {
    clearLidarDisplay("...");
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
  noteLidarRecoveryAttemptResult(lidarRecoveryState, recovered, now);
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  if (!adsReady)
  {
    return lensSpikeFilter.stableReading;
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
  if (!lensSpikeFilter.initialized)
  {
    lensSpikeFilter.initialized = true;
    lensSpikeFilter.stableReading = smoothedReading;
    lensSpikeFilter.pendingReading = smoothedReading;
    lensSpikeFilter.pendingCount = 0;
    return lensSpikeFilter.stableReading;
  }

  if (abs(smoothedReading - lensSpikeFilter.stableReading) <= LENS_SPIKE_DELTA_THRESHOLD)
  {
    lensSpikeFilter.stableReading = smoothedReading;
    lensSpikeFilter.pendingCount = 0;
    return lensSpikeFilter.stableReading;
  }

  if (lensSpikeFilter.pendingCount == 0 ||
      abs(smoothedReading - lensSpikeFilter.pendingReading) > LENS_SPIKE_DELTA_THRESHOLD)
  {
    lensSpikeFilter.pendingReading = smoothedReading;
    lensSpikeFilter.pendingCount = 1;
    return lensSpikeFilter.stableReading;
  }

  lensSpikeFilter.pendingCount++;
  if (lensSpikeFilter.pendingCount >= LENS_SPIKE_CONFIRMATION_COUNT)
  {
    lensSpikeFilter.stableReading = smoothedReading;
    lensSpikeFilter.pendingCount = 0;
  }

  return lensSpikeFilter.stableReading;
}

void setLensDistance()
{
  const Lens &lens = lenses[selected_lens];

  if (selected_lens != lensSnap.prevLens)
  {
    lensSnap.prevLens = selected_lens;
    lensSnap.prevIndex = -1;
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
    if (snapIndex >= 0 && snapIndex != lensSnap.prevIndex)
    {
      activityDetected = true;
    }
    lensSnap.prevIndex = snapIndex;
  }
  else
  {
    lensSnap.prevIndex = -1;
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

}

void setLightMeter()
{
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
  if (!lightMeterSmoothing.initialized || alpha >= 1.0f)
  {
    lightMeterSmoothing.smoothedLux = rawLux;
    lightMeterSmoothing.initialized = true;
  }
  else
  {
    lightMeterSmoothing.smoothedLux =
        (rawLux * alpha) + (lightMeterSmoothing.smoothedLux * (1.0f - alpha));
  }

  float exposureCompEv = static_cast<float>(exposure_comp_thirds) / 3.0f;
  lux = applyExposureCompensationToLux(lightMeterSmoothing.smoothedLux, exposureCompEv);
  ev_readout = calculateEV100(lightMeterSmoothing.smoothedLux);

  if (aperture == 0 && lux > 0)
  {
    cycleApertures(CycleDirection::Up);
  }

  formatShutterSpeed(lux, aperture, iso, shutter_speed, sizeof(shutter_speed));
  prev_iso = iso;
  prev_aperture = aperture;
}

void retryLidarInit()
{
  if (lidarSensorReady)
  {
    return;
  }

  DTSResult result = lidar.begin(LIDAR_BAUD_RATE, RXD2, TXD2);
  if (result != DTSError::NONE)
  {
    lidarSensorReady = false;
    lidarEnabled = false;
    last_lidar_error_code = static_cast<int>(static_cast<DTSError>(result));
    return;
  }

  lidarSensorReady = true;
  lidarEnabled = true;
  lidar.setFrameRate(LIDAR_FRAME_RATE_FPS);
  lidar.setDistanceScale(LIDAR_LIBRARY_DISTANCE_SCALE);
  lidar.setDistanceOffset(LIDAR_LIBRARY_DISTANCE_OFFSET_MM);
  last_lidar_error_code = 0;
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
