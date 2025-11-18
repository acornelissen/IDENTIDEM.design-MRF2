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

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  if (lidar.update())
  { // Get data from Lidar
    distance = (lidar.getDistance() / 10) + LIDAR_OFFSET;
    if (distance != prev_distance)
    {
      if (distance <= LIDAR_OFFSET)
      {
        distance_cm = "> " + String(DISTANCE_MAX) + "m";
      }
      else if (distance < DISTANCE_MIN)
      {
        distance_cm = "< " + String(DISTANCE_MIN) + "cm";
      }
      else
      {
        distance_cm = cmToReadable(distance, 1);
      }
      prev_distance = distance;
    }
  }
  else
  {
    distance_cm = "...";
  }
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  float sensorVal = round(theads.readADC_SingleEnded(LENS_ADC_PIN));
  // Make sure your sensor's + and GND are connected the right way around.
  // You want the value to increase as the focus distance increases.
  // 1m should be smallest, 10m should be largest. If not, swap the wires.

  int filteredVal = rejectOutliers(0, sensorVal);
  float finalVal = calcMovingAvg(0, filteredVal);

  Serial.print(sensorVal);
  Serial.print(" | ");
  Serial.print(filteredVal);
  Serial.print(" | ");
  Serial.println(finalVal);

  return finalVal;
}

void setLensDistance()
{

  lens_sensor_reading = getLensSensorReading();

  if (lens_sensor_reading != prev_lens_sensor_reading)
  {

    if (abs(lens_sensor_reading - prev_lens_sensor_reading) > 3)
    {
      lastActivityTime = millis();
    }

    prev_lens_sensor_reading = lens_sensor_reading;

    for (int i = 0; i < sizeof(lenses[selected_lens].sensor_reading) / sizeof(lenses[selected_lens].sensor_reading[0]); i++)
    {
      if (lens_sensor_reading < lenses[selected_lens].sensor_reading[0])
      {
        lens_distance_raw = lenses[selected_lens].distance[0] * 100;
        lens_distance_cm = cmToReadable(lens_distance_raw, 1);
      }
      else if (lens_sensor_reading > lenses[selected_lens].sensor_reading[sizeof(lenses[selected_lens].sensor_reading) / sizeof(lenses[selected_lens].sensor_reading[0]) - 1] + LENS_INF_THRESHOLD)
      {
        lens_distance_raw = 9999999;
        lens_distance_cm = "Inf.";
      }
      else if (lens_sensor_reading == lenses[selected_lens].sensor_reading[i])
      {
        lens_distance_raw = lenses[selected_lens].distance[i] * 100;
        lens_distance_cm = cmToReadable(lens_distance_raw, 1);
      }
      else if (lens_sensor_reading > lenses[selected_lens].sensor_reading[i] && lens_sensor_reading < lenses[selected_lens].sensor_reading[i + 1])
      {
        float distance_val = lenses[selected_lens].distance[i] + (lens_sensor_reading - lenses[selected_lens].sensor_reading[i]) * (lenses[selected_lens].distance[i + 1] - lenses[selected_lens].distance[i]) / (lenses[selected_lens].sensor_reading[i + 1] - lenses[selected_lens].sensor_reading[i]);
        lens_distance_raw = distance_val * 100;
        lens_distance_cm = cmToReadable(lens_distance_raw, 1);
      }
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
        // Check if the encoder value is within +/- 2 of the next frame
        if (abs(encoder_value - film_formats[selected_format].sensor[i + 1]) <= 1)
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
      else if (film_formats[selected_format].frame[i] == 99 && encoder_value >= film_formats[selected_format].sensor[i])
      {
        film_counter = 99;
        frame_progress = 0;
      }
    }
    savePrefs();
  }
}

void setVoltage()
{
  bat_per = maxlipo.cellPercent();
  if (bat_per > 100)
  {
    bat_per = 100;
  }

  if (bat_per != prev_bat_per)
  {
    prev_bat_per = bat_per;
  }
}

void setLightMeter()
{
  lux = lightMeter.readLightLevel();

  if (lux != prev_lux)
  {
    prev_lux = lux;
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

      float speed = round(((aperture * aperture) * K) / (lux * iso) * 1000.0) / 1000.0;

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

      char print_speed[10];
      dtostrf(speed, 4, 1, print_speed); // dtostrf is not standard C++, but common in Arduino

      for (int i = 0; i < sizeof(speed_ranges) / sizeof(speed_ranges[0]); i++)
      {
        if (speed_ranges[i].lower <= speed && speed < speed_ranges[i].upper)
        {
          strcpy(print_speed, speed_ranges[i].print_speed_range);
          break;
        }
      }

      if (speed >= 1)
      {
        char print_speed_raw[10];
        dtostrf(speed, 4, 2, print_speed_raw);
        shutter_speed = strcat(print_speed_raw, " sec.");
      }
      else
      {
        shutter_speed = strcat(print_speed, " sec.");
      }
    }
  }
}

void toggleLidar(bool lidarStatusParam) // Renamed parameter to avoid conflict with global
{
  // The original function parameter `lidarStatus` shadows a potential global variable
  // and the assignments `lidarStatus = false;` or `lidarStatus = true;` inside the function
  // only affect the local parameter, not a global state if one was intended to be modified.
  // Assuming the function's purpose is to send a command based on the parameter.
  // DTS6012M doesn't support enable/disable output commands
  // Sensor runs continuously when powered
  if (lidarStatusParam == false)
  {
    // Note: DTS6012M cannot be disabled via software command
    // If there's a global `bool lidarStatus` that needs updating, it should be done explicitly.
    // For example: ::lidarStatus = false;
  }
  else
  {
    // Note: DTS6012M is always enabled when powered
    // For example: ::lidarStatus = true;
  }
}
// ---------------------