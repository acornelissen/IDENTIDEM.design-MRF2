#include "helpers.h"

#include <Arduino.h>
#include <Preferences.h> // For Preferences object
#include <stddef.h>      // For offsetof
#include <stdint.h>      // For uint16_t, uint8_t
#include <stdio.h>       // For snprintf
#include <stdlib.h>      // For malloc/free
#include <string.h>      // For memcpy

#include "globals.h"
#include "formats.h"
#include "lenses.h"       // Now includes NUM_LENSES
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

namespace
{
const char *PREFS_NAMESPACE = "mrf";
const char *PREFS_KEY_SCHEMA = "schema";
const char *PREFS_KEY_LEGACY_LENSES = "lenses";
const char *PREFS_KEY_LENS_COUNT = "lc_count";
const uint16_t PREFS_SCHEMA_VERSION = 2;
const unsigned long PREFS_FLUSH_DELAY_MS = 2000;

bool prefsDirty = false;
unsigned long prefsLastDirtyMs = 0;

void getLensReadingsKey(size_t lensIndex, char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, "lc_sr_%u", static_cast<unsigned int>(lensIndex));
}

void getLensCalibratedKey(size_t lensIndex, char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, "lc_ok_%u", static_cast<unsigned int>(lensIndex));
}

void writeLensCalibrationPrefs()
{
  prefs.putUInt(PREFS_KEY_LENS_COUNT, static_cast<uint32_t>(NUM_LENSES));

  for (size_t lensIndex = 0; lensIndex < NUM_LENSES; lensIndex++)
  {
    char readingsKey[16] = {0};
    char calibratedKey[16] = {0};
    getLensReadingsKey(lensIndex, readingsKey, sizeof(readingsKey));
    getLensCalibratedKey(lensIndex, calibratedKey, sizeof(calibratedKey));

    prefs.putBytes(readingsKey, lenses[lensIndex].sensor_reading, sizeof(lenses[lensIndex].sensor_reading));
    prefs.putBool(calibratedKey, lenses[lensIndex].calibrated);
  }
}

void writePrefsToOpenNamespace()
{
  prefs.putUShort(PREFS_KEY_SCHEMA, PREFS_SCHEMA_VERSION);
  prefs.putInt("iso", iso);
  prefs.putInt("iso_index", iso_index);
  prefs.putFloat("aperture", aperture);
  prefs.putInt("aperture_index", aperture_index);
  prefs.putInt("selected_format", selected_format);
  prefs.putInt("selected_lens", selected_lens);
  prefs.putBool("parallax", parallaxEnabled);
  prefs.putInt("film_counter", film_counter);
  prefs.putInt("encoder_value", encoder_value);
  prefs.putInt("prev_encoder_value", prev_encoder_value);
  writeLensCalibrationPrefs();
}

void writePrefsNow()
{
  prefs.begin(PREFS_NAMESPACE, false);
  writePrefsToOpenNamespace();
  prefs.end();
}

void clampLoadedState()
{
  if (iso_index < 0 || iso_index >= static_cast<int>(sizeof(ISOS) / sizeof(ISOS[0])))
  {
    iso_index = DEFAULT_ISO_INDEX;
  }
  iso = ISOS[iso_index];

  if (selected_lens < 0 || selected_lens >= static_cast<int>(NUM_LENSES))
  {
    selected_lens = (DEFAULT_SELECTED_LENS >= 0 && DEFAULT_SELECTED_LENS < static_cast<int>(NUM_LENSES))
                        ? DEFAULT_SELECTED_LENS
                        : 0;
  }

  if (selected_format < 0 || selected_format >= static_cast<int>(NUM_FILM_FORMATS))
  {
    selected_format = (DEFAULT_SELECTED_FORMAT >= 0 && DEFAULT_SELECTED_FORMAT < static_cast<int>(NUM_FILM_FORMATS))
                          ? DEFAULT_SELECTED_FORMAT
                          : 0;
  }

  const int aperture_count = sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]);
  int fallback_aperture_index = getFirstNonZeroAperture();
  if (fallback_aperture_index < 0)
  {
    fallback_aperture_index = 0;
  }

  if (aperture_index < 0 || aperture_index >= aperture_count || lenses[selected_lens].apertures[aperture_index] == 0)
  {
    aperture_index = fallback_aperture_index;
  }

  if (lenses[selected_lens].apertures[aperture_index] == 0)
  {
    aperture_index = fallback_aperture_index;
  }
  aperture = lenses[selected_lens].apertures[aperture_index];

  if (film_counter < 0)
  {
    film_counter = 0;
  }
  if (encoder_value < 0)
  {
    encoder_value = 0;
  }
  if (prev_encoder_value < 0)
  {
    prev_encoder_value = 0;
  }
}

void loadLensCalibrationSchemaV2()
{
  size_t storedLensCount = static_cast<size_t>(prefs.getUInt(PREFS_KEY_LENS_COUNT, 0));
  size_t loadCount = (storedLensCount < NUM_LENSES) ? storedLensCount : NUM_LENSES;

  for (size_t lensIndex = 0; lensIndex < loadCount; lensIndex++)
  {
    char readingsKey[16] = {0};
    char calibratedKey[16] = {0};
    getLensReadingsKey(lensIndex, readingsKey, sizeof(readingsKey));
    getLensCalibratedKey(lensIndex, calibratedKey, sizeof(calibratedKey));

    if (prefs.getBytesLength(readingsKey) == sizeof(lenses[lensIndex].sensor_reading))
    {
      prefs.getBytes(readingsKey, lenses[lensIndex].sensor_reading, sizeof(lenses[lensIndex].sensor_reading));
    }

    lenses[lensIndex].calibrated = prefs.getBool(calibratedKey, lenses[lensIndex].calibrated);
  }
}

bool migrateLegacyLensPrefs()
{
  const size_t legacyBytes = prefs.getBytesLength(PREFS_KEY_LEGACY_LENSES);
  const size_t expectedBytes = NUM_LENSES * sizeof(Lens);
  if (legacyBytes == 0 || legacyBytes != expectedBytes)
  {
    return false;
  }

  uint8_t *buffer = static_cast<uint8_t *>(malloc(legacyBytes));
  if (!buffer)
  {
    return false;
  }

  size_t bytesRead = prefs.getBytes(PREFS_KEY_LEGACY_LENSES, buffer, legacyBytes);
  if (bytesRead != legacyBytes)
  {
    free(buffer);
    return false;
  }

  for (size_t lensIndex = 0; lensIndex < NUM_LENSES; lensIndex++)
  {
    uint8_t *lensBytes = buffer + (lensIndex * sizeof(Lens));
    memcpy(
        lenses[lensIndex].sensor_reading,
        lensBytes + offsetof(Lens, sensor_reading),
        sizeof(lenses[lensIndex].sensor_reading));

    bool calibrated = false;
    memcpy(&calibrated, lensBytes + offsetof(Lens, calibrated), sizeof(calibrated));
    lenses[lensIndex].calibrated = calibrated;
  }

  free(buffer);
  return true;
}
} // namespace

// Helper functions
// ---------------------
int getFirstNonZeroAperture()
{
  const int aperture_count = sizeof(lenses[selected_lens].apertures) / sizeof(lenses[selected_lens].apertures[0]);
  for (int i = 0; i < aperture_count; i++)
  {
    if (lenses[selected_lens].apertures[i] != 0)
    {
      return i;
    }
  }
  return -1;
}

void loadPrefs()
{
  prefs.begin(PREFS_NAMESPACE, false);

  iso_index = prefs.getInt("iso_index", DEFAULT_ISO_INDEX);
  iso = prefs.getInt("iso", DEFAULT_ISO);
  aperture_index = prefs.getInt("aperture_index", 0);
  aperture = prefs.getFloat("aperture", 0.0f);
  selected_lens = prefs.getInt("selected_lens", DEFAULT_SELECTED_LENS);
  selected_format = prefs.getInt("selected_format", DEFAULT_SELECTED_FORMAT);
  parallaxEnabled = prefs.getBool("parallax", true);
  film_counter = prefs.getInt("film_counter", 0);
  encoder_value = prefs.getInt("encoder_value", 0);
  prev_encoder_value = prefs.getInt("prev_encoder_value", 0);

  bool migratedLegacy = false;
  uint16_t schemaVersion = prefs.getUShort(PREFS_KEY_SCHEMA, 0);
  if (schemaVersion >= PREFS_SCHEMA_VERSION)
  {
    loadLensCalibrationSchemaV2();
  }
  else
  {
    migratedLegacy = migrateLegacyLensPrefs();
  }

  clampLoadedState();

  if (migratedLegacy)
  {
    writePrefsToOpenNamespace();
    prefs.remove(PREFS_KEY_LEGACY_LENSES);
  }

  prefs.end();
  prefsDirty = false;
}

void savePrefs(bool force)
{
  prefsDirty = true;
  prefsLastDirtyMs = millis();

  if (force)
  {
    writePrefsNow();
    prefsDirty = false;
  }
}

void flushPrefsIfDirty()
{
  if (!prefsDirty)
  {
    return;
  }

  unsigned long now = millis();
  if ((now - prefsLastDirtyMs) < PREFS_FLUSH_DELAY_MS)
  {
    return;
  }

  writePrefsNow();
  prefsDirty = false;
}

String cmToReadable(int cm, int places)
{
  if (cm < CM_PER_METER)
  {
    return String(cm) + "cm";
  }
  else
  {
    return String(float(cm) / CM_PER_METER, places) + "m";
  }
}

int calcMovingAvg(int sensorVal)
{
  sampleTotal = sampleTotal - samples[curReadIndex];

  samples[curReadIndex] = sensorVal;
  sampleTotal = sampleTotal + samples[curReadIndex];
  curReadIndex = curReadIndex + 1;

  if (curReadIndex >= SMOOTHING_WINDOW_SIZE)
  {
    curReadIndex = 0;
  }

  sampleAvg = sampleTotal / SMOOTHING_WINDOW_SIZE;
  return sampleAvg;
}

int_fast16_t getFocusRadius()
{
  int minRadius = FOCUS_RADIUS_MIN;
  int maxRadius = FOCUS_RADIUS_MAX;

  // Arduino.h usually provides min/max macros or functions
  // and abs. If not, <algorithm> for std::min/max or manual.
  // Assuming Arduino's min/max/abs are available.
  int radius = min(maxRadius, max(minRadius, abs(distance - lens_distance_raw)));

  return radius;
}
// ---------------------
