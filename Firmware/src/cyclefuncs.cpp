#include "cyclefuncs.h"

#include <Arduino.h>

#include "globals.h"
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

int cycleFrameTuningValue(int current)
{
  current++;
  if (current > FRAME_TUNING_MAX)
  {
    current = FRAME_TUNING_MIN;
  }
  return current;
}
} // namespace

// Functions to cycle values
// ---------------------
void cycleApertures(CycleDirection direction)
{
  const int aperture_count = sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]);
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
      savePrefs();
      return;
    }
  }

  aperture_index = 0;
  aperture = lenses[selected_lens].apertures[aperture_index];
  savePrefs();
}

void cycleISOs()
{
  iso_index++;
  if (iso_index >= sizeof(ISOS) / sizeof(ISOS[0]))
  {
    iso_index = 0;
  }
  iso = ISOS[iso_index];
  savePrefs();
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

  savePrefs();
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
  savePrefs();
}

void cycleFrameOneOffset()
{
  frame_one_offset = cycleFrameTuningValue(frame_one_offset);
  savePrefs();
}

void cycleFrameSpacingOffset()
{
  frame_spacing_offset = cycleFrameTuningValue(frame_spacing_offset);
  savePrefs();
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
  savePrefs();
}

void cycleMeterSmoothing()
{
  meter_smoothing_mode++;
  if (meter_smoothing_mode > LIGHTMETER_SMOOTHING_MODE_MAX)
  {
    meter_smoothing_mode = LIGHTMETER_SMOOTHING_MODE_MIN;
  }
  savePrefs();
}

void toggleEvReadout()
{
  show_ev_readout = !show_ev_readout;
  savePrefs();
}

void cycleSleepTimeoutMode()
{
  sleep_timeout_mode++;
  if (sleep_timeout_mode > SLEEP_TIMEOUT_MODE_MAX)
  {
    sleep_timeout_mode = SLEEP_TIMEOUT_MODE_MIN;
  }
  savePrefs();
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
