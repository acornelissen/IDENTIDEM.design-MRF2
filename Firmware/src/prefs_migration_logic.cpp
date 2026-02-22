#include "prefs_migration_logic.h"

#include <stddef.h>
#include <string.h>

size_t expectedLegacyLensBlobSize(size_t lens_count)
{
  return lens_count * sizeof(Lens);
}

PrefsLoadMode selectPrefsLoadMode(
    uint16_t stored_schema,
    uint16_t current_schema,
    size_t legacy_blob_bytes,
    size_t expected_legacy_blob_bytes)
{
  if (stored_schema >= current_schema)
  {
    return PrefsLoadMode::LOAD_SCHEMA;
  }

  if (legacy_blob_bytes > 0 && legacy_blob_bytes == expected_legacy_blob_bytes)
  {
    return PrefsLoadMode::MIGRATE_LEGACY;
  }

  return PrefsLoadMode::LOAD_DEFAULTS;
}

bool applyLegacyLensBlob(
    const uint8_t *legacy_blob,
    size_t legacy_blob_bytes,
    Lens *target_lenses,
    size_t target_lens_count)
{
  if (!legacy_blob || !target_lenses || target_lens_count == 0)
  {
    return false;
  }

  const size_t expected_bytes = expectedLegacyLensBlobSize(target_lens_count);
  if (legacy_blob_bytes != expected_bytes)
  {
    return false;
  }

  for (size_t lens_index = 0; lens_index < target_lens_count; lens_index++)
  {
    const uint8_t *lens_bytes = legacy_blob + (lens_index * sizeof(Lens));
    memcpy(
        target_lenses[lens_index].sensor_reading,
        lens_bytes + offsetof(Lens, sensor_reading),
        sizeof(target_lenses[lens_index].sensor_reading));

    bool calibrated = false;
    memcpy(&calibrated, lens_bytes + offsetof(Lens, calibrated), sizeof(calibrated));
    target_lenses[lens_index].calibrated = calibrated;
  }

  return true;
}
