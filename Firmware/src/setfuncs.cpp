#include "setfuncs.h"

#include <Arduino.h>
#include <string.h> // For strcpy, strcat
#include <math.h>   // For round, abs

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h"
#include "formats.h"
#include "helpers.h"
#include "cyclefuncs.h" // For cycleApertures

static int applyLidarCalibrationCm(int raw_cm)
{
  if (raw_cm <= 0)
  {
    return raw_cm;
  }

  if (raw_cm >= static_cast<int>(LIDAR_CAL_CUTOFF_CM))
  {
    return raw_cm;
  }

  if (LIDAR_CAL_REF_RAW_CM <= 0.0f || LIDAR_CAL_REF_TRUE_CM <= 0.0f || LIDAR_CAL_REF_RAW_CM >= LIDAR_CAL_CUTOFF_CM)
  {
    return raw_cm;
  }

  static float exponent = 0.0f;
  static bool exponent_init = false;
  if (!exponent_init)
  {
    exponent = logf(LIDAR_CAL_REF_TRUE_CM / LIDAR_CAL_REF_RAW_CM) /
               logf(LIDAR_CAL_REF_RAW_CM / LIDAR_CAL_CUTOFF_CM);
    exponent_init = true;
  }

  float raw_cm_f = static_cast<float>(raw_cm);
  float scaled = raw_cm_f * powf(raw_cm_f / LIDAR_CAL_CUTOFF_CM, exponent);
  return static_cast<int>(roundf(scaled));
}

static int applyLidarResidualCorrectionCm(int corrected_cm)
{
  if (corrected_cm <= 0 || LIDAR_RESIDUAL_POINT_COUNT <= 0)
  {
    return corrected_cm;
  }

  if (corrected_cm <= LIDAR_RESIDUAL_DIST_CM[0])
  {
    return corrected_cm + LIDAR_RESIDUAL_DELTA_CM[0];
  }

  for (int i = 1; i < LIDAR_RESIDUAL_POINT_COUNT; i++)
  {
    if (corrected_cm <= LIDAR_RESIDUAL_DIST_CM[i])
    {
      int x0 = LIDAR_RESIDUAL_DIST_CM[i - 1];
      int x1 = LIDAR_RESIDUAL_DIST_CM[i];
      int y0 = LIDAR_RESIDUAL_DELTA_CM[i - 1];
      int y1 = LIDAR_RESIDUAL_DELTA_CM[i];
      float t = static_cast<float>(corrected_cm - x0) / static_cast<float>(x1 - x0);
      int residual_delta = static_cast<int>(roundf(static_cast<float>(y0) + (static_cast<float>(y1 - y0) * t)));
      return corrected_cm + residual_delta;
    }
  }

  return corrected_cm + LIDAR_RESIDUAL_DELTA_CM[LIDAR_RESIDUAL_POINT_COUNT - 1];
}

static int applyLidarDoubleCorrectionCm(int raw_cm)
{
  int curve_corrected_cm = applyLidarCalibrationCm(raw_cm);
  return applyLidarResidualCorrectionCm(curve_corrected_cm);
}

static int qualityBaseScore(DataQuality quality)
{
  switch (quality)
  {
  case DataQuality::EXCELLENT:
    return 80;
  case DataQuality::GOOD:
    return 65;
  case DataQuality::FAIR:
    return 45;
  case DataQuality::POOR:
    return 25;
  default:
    return 0;
  }
}

static bool getLensPriorCm(int &lens_prior_cm)
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

struct LidarCandidate
{
  bool valid;
  int distance_cm;
  int confidence;
};

static LidarCandidate buildLidarCandidate(uint16_t raw_distance_mm,
                                          uint16_t intensity,
                                          DataQuality quality,
                                          bool secondary_candidate,
                                          int previous_distance_cm,
                                          bool has_lens_prior,
                                          int lens_prior_cm)
{
  LidarCandidate candidate = {false, 0, 0};

  if (raw_distance_mm == DTS_INVALID_DISTANCE || intensity < LIDAR_FUSION_MIN_INTENSITY)
  {
    return candidate;
  }

  int raw_cm = static_cast<int>(raw_distance_mm) / LIDAR_DISTANCE_DIVISOR;
  if (raw_cm <= 0)
  {
    return candidate;
  }

  int corrected_cm = applyLidarDoubleCorrectionCm(raw_cm);
  if (corrected_cm <= 0)
  {
    return candidate;
  }

  int confidence = qualityBaseScore(quality);
  confidence += min(20, static_cast<int>(intensity / 150));

  if (previous_distance_cm > 0)
  {
    confidence -= min(25, abs(corrected_cm - previous_distance_cm) / 8);
  }

  if (has_lens_prior)
  {
    float prior_weight = (quality == DataQuality::EXCELLENT) ? LIDAR_LENS_PRIOR_WEIGHT_EXCELLENT : LIDAR_LENS_PRIOR_WEIGHT_GOOD;
    int prior_penalty = static_cast<int>(roundf(static_cast<float>(abs(corrected_cm - lens_prior_cm)) * prior_weight));
    confidence -= min(20, prior_penalty);
  }

  if (secondary_candidate)
  {
    confidence -= 2;
  }

  confidence = constrain(confidence, 0, 100);
  if (confidence == 0)
  {
    return candidate;
  }

  candidate.valid = true;
  candidate.distance_cm = corrected_cm;
  candidate.confidence = confidence;
  return candidate;
}

static void setDistanceDisplayString(int corrected_cm)
{
  if (corrected_cm <= 0)
  {
    distance_cm = "> " + String(DISTANCE_MAX) + "m";
  }
  else if (corrected_cm > (DISTANCE_MAX * CM_PER_METER))
  {
    distance_cm = "> " + String(DISTANCE_MAX) + "m";
  }
  else if (corrected_cm < DISTANCE_MIN)
  {
    distance_cm = "< " + String(DISTANCE_MIN) + "cm";
  }
  else
  {
    distance_cm = cmToReadable(corrected_cm, DISTANCE_DECIMAL_PLACES);
  }
}

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
    DTSMeasurement measurement = lidar.getMeasurement();

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    DataQuality secondary_quality = measurement.secondaryQuality;
    if (secondary_quality == DataQuality::INVALID && measurement.secondaryDistance_mm != DTS_INVALID_DISTANCE)
    {
      secondary_quality = measurement.primaryQuality;
    }

    LidarCandidate primary = buildLidarCandidate(measurement.primaryDistance_mm,
                                                 measurement.primaryIntensity,
                                                 measurement.primaryQuality,
                                                 false,
                                                 prev_distance,
                                                 has_lens_prior,
                                                 lens_prior_cm);

    LidarCandidate secondary = buildLidarCandidate(measurement.secondaryDistance_mm,
                                                   measurement.secondaryIntensity,
                                                   secondary_quality,
                                                   true,
                                                   prev_distance,
                                                   has_lens_prior,
                                                   lens_prior_cm);

    LidarCandidate chosen = primary;
    if (!chosen.valid || (secondary.valid && secondary.confidence > chosen.confidence))
    {
      chosen = secondary;
    }

    if (!chosen.valid)
    {
      return;
    }

    lastValidLidarMeasurementMs = millis();

    int next_distance_cm = chosen.distance_cm;
    if (prev_distance > 0)
    {
      if (chosen.confidence >= LIDAR_CONFIDENCE_HIGH)
      {
        distance = static_cast<int16_t>(next_distance_cm);
      }
      else if (chosen.confidence >= LIDAR_CONFIDENCE_MEDIUM)
      {
        float blended = (static_cast<float>(prev_distance) * (1.0f - LIDAR_MEDIUM_CONF_BLEND)) +
                        (static_cast<float>(next_distance_cm) * LIDAR_MEDIUM_CONF_BLEND);
        distance = static_cast<int16_t>(roundf(blended));
      }
      else
      {
        distance = prev_distance;
      }
    }
    else
    {
      distance = static_cast<int16_t>(next_distance_cm);
    }

    if (distance != prev_distance || distance_cm == "...")
    {
      setDistanceDisplayString(distance);
      prev_distance = distance;
    }
  }
  else if (lidarUpdateError == DTSError::TIMEOUT)
  {
    if (millis() - lastValidLidarMeasurementMs > LIDAR_NO_DATA_TIMEOUT_MS)
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
  // Make sure your sensor's + and GND are connected the right way around.
  // You want the value to increase as the focus distance increases.
  // 1m should be smallest, 10m should be largest. If not, swap the wires.

  int filteredVal = rejectOutliers(LENS_SENSOR_CHANNEL, sensorVal);
  int finalVal = calcMovingAvg(LENS_SENSOR_CHANNEL, filteredVal);
  return finalVal;
}

void setLensDistance()
{
  static int prevSnapIndex = -1;
  static int prevSnapLens = -1;
  const int readingCount = sizeof(lenses[selected_lens].sensor_reading) / sizeof(lenses[selected_lens].sensor_reading[0]);

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

  if (lenses[selected_lens].calibrated)
  {
    int snapDelta = max(LENS_SNAP_DEADZONE, LENS_SNAP_DEADZONE_FAR) + 1;
    for (int i = 0; i < readingCount; i++)
    {
      int delta = abs(lens_sensor_reading - lenses[selected_lens].sensor_reading[i]);
      int snapDeadzone = LENS_SNAP_DEADZONE;
      if (lenses[selected_lens].distance[i] >= LENS_SNAP_FAR_DISTANCE_M)
      {
        snapDeadzone = LENS_SNAP_DEADZONE_FAR;
      }
      if (delta <= snapDeadzone && delta < snapDelta)
      {
        snapDelta = delta;
        snapIndex = i;
      }
    }

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
    lastActivityTime = millis();
    if (sleepMode == true)
    {
      sleepMode = false;
    }
  }

  if (lenses[selected_lens].calibrated && snapIndex >= 0)
  {
    lens_distance_raw = lenses[selected_lens].distance[snapIndex] * CM_PER_METER;
    lens_distance_cm = cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES);
    return;
  }

  for (int i = 0; i < readingCount; i++)
  {
    if (lens_sensor_reading < lenses[selected_lens].sensor_reading[0])
    {
      lens_distance_raw = lenses[selected_lens].distance[0] * CM_PER_METER;
      lens_distance_cm = cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES);
    }
    else if (lens_sensor_reading > lenses[selected_lens].sensor_reading[readingCount - 1] + LENS_INF_THRESHOLD)
    {
      lens_distance_raw = LENS_INFINITY_RAW;
      lens_distance_cm = "Inf.";
    }
    else if (lens_sensor_reading == lenses[selected_lens].sensor_reading[i])
    {
      lens_distance_raw = lenses[selected_lens].distance[i] * CM_PER_METER;
      lens_distance_cm = cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES);
    }
    else if (i + 1 < readingCount &&
             lens_sensor_reading > lenses[selected_lens].sensor_reading[i] &&
             lens_sensor_reading < lenses[selected_lens].sensor_reading[i + 1])
    {
      float distance_val = lenses[selected_lens].distance[i] + (lens_sensor_reading - lenses[selected_lens].sensor_reading[i]) * (lenses[selected_lens].distance[i + 1] - lenses[selected_lens].distance[i]) / (lenses[selected_lens].sensor_reading[i + 1] - lenses[selected_lens].sensor_reading[i]);
      lens_distance_raw = distance_val * CM_PER_METER;
      lens_distance_cm = cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES);
    }
  }
}

void setFilmCounter()
{
  int encoder_position = encoder.getEncoderPosition();

  if (encoder_position != prev_encoder_value && encoder_position > prev_encoder_value)
  {
    lastActivityTime = millis();

    if (sleepMode == true)
    {
      sleepMode = false;
    }

    encoder_value = encoder_position;
    prev_encoder_value = encoder_value;

    for (int i = 0; i < sizeof(film_formats[selected_format].sensor) / sizeof(film_formats[selected_format].sensor[0]); i++)
    {
      if (film_formats[selected_format].sensor[i] == encoder_value)
      {
        film_counter = film_formats[selected_format].frame[i];
        frame_progress = 0;
      }
      else if (film_formats[selected_format].sensor[i] < encoder_value && encoder_value < film_formats[selected_format].sensor[i + 1])
      {
        // Check if the encoder value is within the snap threshold of the next frame
        if (abs(encoder_value - film_formats[selected_format].sensor[i + 1]) <= FILM_COUNTER_SNAP_THRESHOLD)
        {
          // Snap to the next frame
          film_counter = film_formats[selected_format].frame[i + 1];
          frame_progress = 0;
        }
        else
        {
          film_counter = film_formats[selected_format].frame[i];
          frame_progress = static_cast<float>(encoder_value - film_formats[selected_format].sensor[i]) / (film_formats[selected_format].sensor[i + 1] - film_formats[selected_format].sensor[i]);
        }
      }
      else if (film_formats[selected_format].frame[i] == FILM_COUNTER_END && encoder_value >= film_formats[selected_format].sensor[i])
      {
        film_counter = FILM_COUNTER_END;
        frame_progress = 0;
      }
    }
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
    if (lux <= 0)
    {
      shutter_speed = "Dark!";
    }
    else
    {
      if (aperture == 0)
      {
        cycleApertures("up");
      }

      float speed = round(((aperture * aperture) * K) / (lux * iso) * LIGHTMETER_SPEED_ROUND_SCALE) / LIGHTMETER_SPEED_ROUND_SCALE;

      const float SPEED_TOO_FAST_THRESHOLD = 0.001f;
      struct SpeedRange
      {
        float lower;
        float upper;
        const char *print_speed_range;
      };

      SpeedRange speed_ranges[] = {
          {0.001, 0.002, "1/1000"},
          {0.002, 0.004, "1/500"},
          {0.004, 0.008, "1/250"},
          {0.008, 0.016, "1/125"},
          {0.016, 0.033, "1/60"},
          {0.033, 0.066, "1/30"},
          {0.066, 0.125, "1/15"},
          {0.125, 0.250, "1/8"},
          {0.250, 0.500, "1/4"},
          {0.500, 1, "1/2"}};

      if (speed < SPEED_TOO_FAST_THRESHOLD)
      {
        shutter_speed = "Bright!";
      }
      else if (speed >= SPEED_SECONDS_THRESHOLD)
      {
        char print_speed_raw[SPEED_TEXT_BUFFER_LEN];
        dtostrf(speed, SPEED_TEXT_WIDTH, SPEED_TEXT_DECIMALS_LONG, print_speed_raw);
        shutter_speed = strcat(print_speed_raw, " sec.");
      }
      else
      {
        char print_speed[SPEED_TEXT_BUFFER_LEN];
        dtostrf(speed, SPEED_TEXT_WIDTH, SPEED_TEXT_DECIMALS_SHORT, print_speed); // dtostrf is not standard C++, but common in Arduino

        for (int i = 0; i < sizeof(speed_ranges) / sizeof(speed_ranges[0]); i++)
        {
          if (speed_ranges[i].lower <= speed && speed < speed_ranges[i].upper)
          {
            strcpy(print_speed, speed_ranges[i].print_speed_range);
            break;
          }
        }
        shutter_speed = strcat(print_speed, " sec.");
      }
    }
    prev_lux = lux;
    prev_iso = iso;
    prev_aperture = aperture;
  }
}

void toggleLidar(bool lidarStatusParam) // Renamed parameter to avoid conflict with global
{
  if (lidarStatusParam == false)
  {
    if (lidarEnabled)
    {
      DTSError disableError = static_cast<DTSError>(lidar.disableSensor());
      if (disableError == DTSError::NONE)
      {
        lidarEnabled = false;
      }
    }
  }
  else
  {
    if (!lidarEnabled)
    {
      DTSError enableError = static_cast<DTSError>(lidar.enableSensor());
      if (enableError == DTSError::NONE)
      {
        lidarEnabled = true;
      }
    }
  }
}
// ---------------------
