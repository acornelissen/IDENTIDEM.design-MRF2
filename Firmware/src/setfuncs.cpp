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
} // namespace

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  static unsigned long lastValidLidarMeasurementMs = 0;

  if (!lidarEnabled)
  {
    return;
  }

  DTSError lidarUpdateError = static_cast<DTSError>(lidar.update());
  if (lidarUpdateError == DTSError::NONE)
  {
    const unsigned long now = millis();
    DTSMeasurement measurement = lidar.getMeasurement();

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm);
    if (!chosen.valid)
    {
      return;
    }

    lastValidLidarMeasurementMs = now;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || distance_cm == "...")
    {
      distance_cm = formatDistanceDisplay(distance);
      prev_distance = distance;
    }
  }
  else if (lidarUpdateError == DTSError::TIMEOUT)
  {
    const unsigned long now = millis();
    if (now - lastValidLidarMeasurementMs > LIDAR_NO_DATA_TIMEOUT_MS)
    {
      distance_cm = "...";
    }
  }
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

  int filteredVal = rejectOutliers(LENS_SENSOR_CHANNEL, sensorVal);
  return calcMovingAvg(LENS_SENSOR_CHANNEL, filteredVal);
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
  int encoder_position = encoder.getEncoderPosition();

  if (encoder_position != prev_encoder_value && encoder_position > prev_encoder_value)
  {
    registerActivity();

    encoder_value = encoder_position;
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
  lux = lightMeter.readLightLevel();

  if (lux != prev_lux || iso != prev_iso || aperture != prev_aperture)
  {
    if (aperture == 0 && lux > 0)
    {
      cycleApertures(CycleDirection::Up);
    }

    shutter_speed = formatShutterSpeed(lux, aperture, iso);

    prev_lux = lux;
    prev_iso = iso;
    prev_aperture = aperture;
  }
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
