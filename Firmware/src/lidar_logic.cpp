#include "lidar_logic.h"

#include <math.h>

#include "mrfconstants.h"

namespace
{
int applyLidarCalibrationCm(int raw_cm)
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

int applyLidarResidualCorrectionCm(int corrected_cm)
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

int applyLidarDoubleCorrectionCm(int raw_cm)
{
  int curve_corrected_cm = applyLidarCalibrationCm(raw_cm);
  return applyLidarResidualCorrectionCm(curve_corrected_cm);
}

int qualityBaseScore(DataQuality quality)
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

int qualityLevelFromDataQuality(DataQuality quality)
{
  switch (quality)
  {
  case DataQuality::POOR:
    return 1;
  case DataQuality::FAIR:
    return 2;
  case DataQuality::GOOD:
    return 3;
  case DataQuality::EXCELLENT:
    return 4;
  default:
    return 0;
  }
}

LidarCandidate buildLidarCandidate(uint16_t raw_distance_mm,
                                   uint16_t intensity,
                                   DataQuality quality,
                                   bool secondary_candidate,
                                   int previous_distance_cm,
                                   bool has_lens_prior,
                                   int lens_prior_cm)
{
  LidarCandidate candidate = {false, 0, 0, 0};

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
  candidate.quality_level = qualityLevelFromDataQuality(quality);
  return candidate;
}
} // namespace

LidarCandidate chooseBestLidarCandidate(const DTSMeasurement &measurement,
                                        int previous_distance_cm,
                                        bool has_lens_prior,
                                        int lens_prior_cm)
{
  DataQuality secondary_quality = measurement.secondaryQuality;
  if (secondary_quality == DataQuality::INVALID && measurement.secondaryDistance_mm != DTS_INVALID_DISTANCE)
  {
    secondary_quality = measurement.primaryQuality;
  }

  LidarCandidate primary = buildLidarCandidate(measurement.primaryDistance_mm,
                                               measurement.primaryIntensity,
                                               measurement.primaryQuality,
                                               false,
                                               previous_distance_cm,
                                               has_lens_prior,
                                               lens_prior_cm);

  LidarCandidate secondary = buildLidarCandidate(measurement.secondaryDistance_mm,
                                                 measurement.secondaryIntensity,
                                                 secondary_quality,
                                                 true,
                                                 previous_distance_cm,
                                                 has_lens_prior,
                                                 lens_prior_cm);

  if (!primary.valid || (secondary.valid && secondary.confidence > primary.confidence))
  {
    return secondary;
  }

  return primary;
}

int blendLidarDistance(int previous_distance_cm, int next_distance_cm, int confidence)
{
  if (previous_distance_cm <= 0)
  {
    return next_distance_cm;
  }

  if (confidence >= LIDAR_CONFIDENCE_HIGH)
  {
    return next_distance_cm;
  }

  if (confidence >= LIDAR_CONFIDENCE_MEDIUM)
  {
    float blended = (static_cast<float>(previous_distance_cm) * (1.0f - LIDAR_MEDIUM_CONF_BLEND)) +
                    (static_cast<float>(next_distance_cm) * LIDAR_MEDIUM_CONF_BLEND);
    return static_cast<int>(roundf(blended));
  }

  return previous_distance_cm;
}

String formatDistanceDisplay(int corrected_cm)
{
  if (corrected_cm <= 0 || corrected_cm > (DISTANCE_MAX * CM_PER_METER))
  {
    return "> " + String(DISTANCE_MAX) + "m";
  }
  if (corrected_cm < DISTANCE_MIN)
  {
    return "< " + String(DISTANCE_MIN) + "cm";
  }
  if (corrected_cm < CM_PER_METER)
  {
    return String(corrected_cm) + "cm";
  }
  return String(static_cast<float>(corrected_cm) / CM_PER_METER, DISTANCE_DECIMAL_PLACES) + "m";
}
