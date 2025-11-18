#include "cyclefuncs.h"

#include <Arduino.h>

#include "globals.h"
#include "mrfconstants.h"
#include "lenses.h" // Provides Lens struct, lenses array, and NUM_LENSES
#include "formats.h"  // Provides FilmFormat struct, film_formats array, and NUM_FILM_FORMATS
#include "helpers.h" // For savePrefs

// Functions to cycle values
// ---------------------
void cycleApertures(String direction)
{
  if (direction == "up")
  {
    aperture_index++;
    if (aperture_index >= sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]))
    {
      aperture_index = 0;
    }
    if (lenses[selected_lens].apertures[aperture_index] == 0)
    {
      cycleApertures("up"); // Recursive call to skip zero values
      return; // Important to return after recursive call to avoid double savePrefs()
    }
  }
  else if (direction == "down")
  {
    aperture_index--;
    if (aperture_index < 0)
    {
      aperture_index = sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]) - 1;
    }
    if (lenses[selected_lens].apertures[aperture_index] == 0)
    {
      cycleApertures("down"); // Recursive call to skip zero values
      return; // Important to return after recursive call
    }
  }

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
  // savePrefs() was here, but typically calibration cycling doesn't save until a value is set.
  // Keeping original behavior:
  savePrefs(); 
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
// ---------------------