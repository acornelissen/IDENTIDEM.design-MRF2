#include "helpers.h"

#include <Arduino.h>
#include <Preferences.h> // For Preferences object
#include <string.h>      // For memcpy

#include "globals.h"
#include "lenses.h"       // Now includes NUM_LENSES
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

// Helper functions
// ---------------------
float getFirstNonZeroAperture()
{
  for (int i = 0; i < sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]); i++)
  {
    if (lenses[selected_lens].apertures[i] != 0)
    {
      return i; // This returns index, function name suggests it returns aperture value.
                // Keeping original logic.
    }
  }
  return -1;
}

void loadPrefs()
{
  prefs.begin("mrf", false);

  if (prefs.isKey("iso"))
  {
    iso = prefs.getInt("iso", 400);
    iso_index = prefs.getInt("iso_index", 5);

    // Use the constant for the lenses array size defined in lenses.cpp and declared in lenses.h
    byte tempLenses[NUM_LENSES * sizeof(Lens)]; // Allocate based on number of elements * size of element
    prefs.getBytes("lenses", tempLenses, NUM_LENSES * sizeof(Lens));
    memcpy(lenses, tempLenses, NUM_LENSES * sizeof(Lens));

    selected_lens = prefs.getInt("selected_lens", 1);

    int non_zero_aperture_index = getFirstNonZeroAperture();

    aperture = prefs.getFloat("aperture", lenses[selected_lens].apertures[non_zero_aperture_index]);
    aperture_index = prefs.getInt("aperture_index", non_zero_aperture_index);

    film_counter = prefs.getInt("film_counter", 0);
    encoder_value = prefs.getInt("encoder_value", 0);
    prev_encoder_value = prefs.getInt("prev_encoder_value", 0);
    selected_format = prefs.getInt("selected_format", 3); // Moved this down, order doesn't strictly matter here but grouped with other loaded prefs
  }

  prefs.end();
}

void savePrefs()
{
  prefs.begin("mrf", false);
  prefs.putInt("iso", iso);
  prefs.putInt("iso_index", iso_index);
  prefs.putFloat("aperture", aperture);
  prefs.putInt("aperture_index", aperture_index);
  prefs.putInt("selected_format", selected_format);
  prefs.putInt("selected_lens", selected_lens);
  prefs.putInt("film_counter", film_counter);
  prefs.putInt("encoder_value", encoder_value);
  prefs.putInt("prev_encoder_value", prev_encoder_value);
  prefs.putBytes("lenses", (byte *)lenses, NUM_LENSES * sizeof(Lens));
  prefs.end();
}

String cmToReadable(int cm, int places)
{
  if (cm < 100)
  {
    return String(cm) + "cm";
  }
  else
  {
    return String(float(cm) / 100, places) + "m";
  }
}

int calcMovingAvg(int index, int sensorVal)
{
  int readIndex = curReadIndex[index];
  sampleTotal[index] = sampleTotal[index] - (samples[index][readIndex]);

  samples[index][readIndex] = sensorVal;
  sampleTotal[index] = sampleTotal[index] + samples[index][readIndex];
  curReadIndex[index] = curReadIndex[index] + 1;

  if (curReadIndex[index] >= SMOOTHING_WINDOW_SIZE)
  {
    curReadIndex[index] = 0;
  }

  sampleAvg[index] = sampleTotal[index] / SMOOTHING_WINDOW_SIZE;
  return sampleAvg[index];
}

int rejectOutliers(int index, int sensorVal)
{
  static int lastValidValue[2] = {0, 0};

  if (lastValidValue[index] == 0)
  {
    lastValidValue[index] = sensorVal;
    return sensorVal;
  }

  if (abs(sensorVal - lastValidValue[index]) > LENS_OUTLIER_THRESHOLD)
  {
    return lastValidValue[index];
  }

  lastValidValue[index] = sensorVal;
  return sensorVal;
}

int_fast16_t getFocusRadius()
{
  int minRadius = 3;
  int maxRadius = 30;

  // Arduino.h usually provides min/max macros or functions
  // and abs. If not, <algorithm> for std::min/max or manual.
  // Assuming Arduino's min/max/abs are available.
  int radius = min(maxRadius, max(minRadius, abs(distance - lens_distance_raw)));

  return radius;
}
// ---------------------