#include "cyclefuncs.h"

#include <Arduino.h>

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h" // Provides Lens struct, lenses array, and NUM_LENSES
#include "formats.h"  // Provides FilmFormat struct, film_formats array, and NUM_FILM_FORMATS
#include "helpers.h" // For savePrefs

namespace
{
const char *SLEEP_TIMEOUT_MODE_LABELS[SLEEP_TIMEOUT_MODE_COUNT] = {
    "Off",
    "15s",
    "30sec",
    "1m",
    "1m30s",
    "2m"};

const unsigned long SLEEP_TIMEOUT_MODE_MS[SLEEP_TIMEOUT_MODE_COUNT] = {
    0,
    15000,
    30000,
    60000,
    90000,
    120000};

int clampSleepTimeoutMode(int timeout_mode)
{
  return constrain(timeout_mode, SLEEP_TIMEOUT_MODE_MIN, SLEEP_TIMEOUT_MODE_MAX);
}

int cycleValueWrapping(int current, int step, int minVal, int maxVal)
{
  current += step;
  if (current > maxVal)
  {
    current = minVal;
  }
  return current;
}

int getAdjustedSensorPoint(const FilmFormat &film_format, int point_index)
{
  int frame_count = getFilmFormatPointCount(film_format);
  if (frame_count <= 0)
  {
    return 0;
  }

  if (point_index <= 0)
  {
    return film_format.sensor[0];
  }

  if (point_index >= frame_count)
  {
    point_index = frame_count - 1;
  }

  long adjusted = static_cast<long>(film_format.sensor[point_index]) +
                  static_cast<long>(frame_one_offset) +
                  static_cast<long>(frame_spacing_offset) * static_cast<long>(point_index - 1);
  return static_cast<int>(adjusted);
}

int getAdjustedSensorPointForFrame(const FilmFormat &film_format, int frame_value, int fallback_encoder_position)
{
  int frame_count = getFilmFormatPointCount(film_format);
  for (int i = 0; i < frame_count; i++)
  {
    if (film_format.frame[i] == FILM_COUNTER_END)
    {
      break;
    }
    if (film_format.frame[i] == frame_value)
    {
      return getAdjustedSensorPoint(film_format, i);
    }
  }
  return fallback_encoder_position;
}
} // namespace

// Functions to cycle values
// ---------------------
void cycleApertures(CycleDirection direction)
{
  const int aperture_count = LENS_APERTURE_COUNT;
  int step = (direction == CycleDirection::Up) ? 1 : -1;

  for (int attempts = 0; attempts < aperture_count; attempts++)
  {
    aperture_index += step;

    if (aperture_index >= aperture_count)
    {
      aperture_index = 0;
    }
    else if (aperture_index < 0)
    {
      aperture_index = aperture_count - 1;
    }

    if (lenses[selected_lens].apertures[aperture_index] != 0)
    {
      aperture = lenses[selected_lens].apertures[aperture_index];
      savePrefs(false, PREFS_DIRTY_SETTINGS);
      return;
    }
  }

  aperture_index = 0;
  aperture = lenses[selected_lens].apertures[aperture_index];
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleISOs()
{
  iso_index++;
  if (iso_index >= sizeof(ISOS) / sizeof(ISOS[0]))
  {
    iso_index = 0;
  }
  iso = ISOS[iso_index];
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleLenses()
{
  int initial_lens = selected_lens;
  do {
    selected_lens++;
    // Use the NUM_LENSES constant
    if (selected_lens >= NUM_LENSES)
    {
      selected_lens = 0;
    }
    // Prevent infinite loop if no lenses are calibrated, though UI should prevent this.
    if (selected_lens == initial_lens && !lenses[selected_lens].calibrated) {
        // Potentially handle case where no calibrated lenses are available if needed
        break;
    }
  } while (!lenses[selected_lens].calibrated);

  // Clamp aperture to the new lens's valid range so callers never
  // index past the end of the aperture array.
  int firstValid = getFirstNonZeroAperture();
  if (firstValid < 0)
  {
    firstValid = 0;
  }
  aperture_index = firstValid;
  aperture = lenses[selected_lens].apertures[aperture_index];

  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleCalibLenses()
{
  calib_lens++;
  // Use the NUM_LENSES constant
  if (calib_lens >= NUM_LENSES)
  {
    calib_lens = 0;
  }
}

void cycleFormats()
{
  selected_format++;
  // Use the NUM_FILM_FORMATS constant
  if (selected_format >= NUM_FILM_FORMATS)
  {
    selected_format = 0;
  }
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleCurrentFrame()
{
  const FilmFormat &selectedFilmFormat = film_formats[selected_format];
  int maxFrame = getFilmFormatMaxFrame(selectedFilmFormat);
  int currentFrame = constrain(film_counter, 0, maxFrame);

  int nextFrame = currentFrame + 1;
  if (nextFrame > maxFrame)
  {
    nextFrame = 0;
  }

  int nextEncoderPosition =
      getAdjustedSensorPointForFrame(selectedFilmFormat, nextFrame, encoder_value);
  if (encoderReady)
  {
    encoder.setEncoderPosition(nextEncoderPosition);
  }

  encoder_value = nextEncoderPosition;
  prev_encoder_value = nextEncoderPosition;
  film_counter = nextFrame;
  frame_progress = 0.0f;
  prev_frame_progress = 0.0f;
  savePrefs(false, PREFS_DIRTY_FILM);
}

void cycleFrameOneOffset()
{
  frame_one_offset = cycleValueWrapping(frame_one_offset, 1, FRAME_TUNING_MIN, FRAME_TUNING_MAX);
  savePrefs(false, PREFS_DIRTY_FILM);
}

void cycleFrameSpacingOffset()
{
  frame_spacing_offset = cycleValueWrapping(frame_spacing_offset, 1, FRAME_TUNING_MIN, FRAME_TUNING_MAX);
  savePrefs(false, PREFS_DIRTY_FILM);
}

void cycleExposureCompensation(CycleDirection direction)
{
  int step = (direction == CycleDirection::Up) ? 1 : -1;
  exposure_comp_thirds += step;
  if (exposure_comp_thirds > LIGHTMETER_EV_COMP_MAX_THIRDS)
  {
    exposure_comp_thirds = LIGHTMETER_EV_COMP_MIN_THIRDS;
  }
  else if (exposure_comp_thirds < LIGHTMETER_EV_COMP_MIN_THIRDS)
  {
    exposure_comp_thirds = LIGHTMETER_EV_COMP_MAX_THIRDS;
  }
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleMeterSmoothing()
{
  meter_smoothing_mode++;
  if (meter_smoothing_mode > LIGHTMETER_SMOOTHING_MODE_MAX)
  {
    meter_smoothing_mode = LIGHTMETER_SMOOTHING_MODE_MIN;
  }
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void toggleEvReadout()
{
  show_ev_readout = !show_ev_readout;
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleLevelTrimLandscape()
{
  level_trim_landscape_deci_deg = cycleValueWrapping(level_trim_landscape_deci_deg, LEVEL_TRIM_STEP_DECI_DEG, LEVEL_TRIM_MIN_DECI_DEG, LEVEL_TRIM_MAX_DECI_DEG);
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleLevelTrimPortraitPos()
{
  level_trim_portrait_pos_deci_deg = cycleValueWrapping(level_trim_portrait_pos_deci_deg, LEVEL_TRIM_STEP_DECI_DEG, LEVEL_TRIM_MIN_DECI_DEG, LEVEL_TRIM_MAX_DECI_DEG);
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleLevelTrimPortraitNeg()
{
  level_trim_portrait_neg_deci_deg = cycleValueWrapping(level_trim_portrait_neg_deci_deg, LEVEL_TRIM_STEP_DECI_DEG, LEVEL_TRIM_MIN_DECI_DEG, LEVEL_TRIM_MAX_DECI_DEG);
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleSleepTimeoutMode()
{
  sleep_timeout_mode = cycleValueWrapping(sleep_timeout_mode, 1, SLEEP_TIMEOUT_MODE_MIN, SLEEP_TIMEOUT_MODE_MAX);
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

void cycleLidarIdleTimeoutMode()
{
  lidar_idle_timeout_mode = cycleValueWrapping(lidar_idle_timeout_mode, 1, SLEEP_TIMEOUT_MODE_MIN, SLEEP_TIMEOUT_MODE_MAX);
  savePrefs(false, PREFS_DIRTY_SETTINGS);
}

const char *getSleepTimeoutModeLabel(int timeout_mode)
{
  int clampedMode = clampSleepTimeoutMode(timeout_mode);
  return SLEEP_TIMEOUT_MODE_LABELS[clampedMode];
}

unsigned long getSleepTimeoutModeMs(int timeout_mode)
{
  int clampedMode = clampSleepTimeoutMode(timeout_mode);
  return SLEEP_TIMEOUT_MODE_MS[clampedMode];
}
// ---------------------
