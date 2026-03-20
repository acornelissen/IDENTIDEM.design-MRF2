#include "lidar_logic.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

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

int priorPenaltyCapForQuality(DataQuality quality)
{
  switch (quality)
  {
  case DataQuality::EXCELLENT:
    return LIDAR_PRIOR_PENALTY_MAX_EXCELLENT;
  case DataQuality::GOOD:
    return LIDAR_PRIOR_PENALTY_MAX_GOOD;
  case DataQuality::FAIR:
    return LIDAR_PRIOR_PENALTY_MAX_FAIR;
  case DataQuality::POOR:
    return LIDAR_PRIOR_PENALTY_MAX_POOR;
  default:
    return LIDAR_PRIOR_PENALTY_MAX_POOR;
  }
}

float priorWeightForQuality(DataQuality quality)
{
  if (quality == DataQuality::EXCELLENT)
  {
    return LIDAR_LENS_PRIOR_WEIGHT_EXCELLENT;
  }
  return LIDAR_LENS_PRIOR_WEIGHT_GOOD;
}

float priorRangeScaleForDistanceCm(int corrected_cm)
{
  if (corrected_cm <= LIDAR_PRIOR_RANGE_NEAR_CM)
  {
    return LIDAR_PRIOR_RANGE_SCALE_NEAR;
  }
  if (corrected_cm <= LIDAR_PRIOR_RANGE_MID_CM)
  {
    return LIDAR_PRIOR_RANGE_SCALE_MID;
  }
  if (corrected_cm <= LIDAR_PRIOR_RANGE_FAR_CM)
  {
    return LIDAR_PRIOR_RANGE_SCALE_FAR;
  }
  return LIDAR_PRIOR_RANGE_SCALE_VERY_FAR;
}

int minIntensityThresholdForDistanceCm(int raw_cm)
{
  if (raw_cm <= LIDAR_FUSION_INTENSITY_NEAR_RANGE_CM)
  {
    return LIDAR_FUSION_MIN_INTENSITY;
  }
  if (raw_cm <= LIDAR_FUSION_INTENSITY_MID_RANGE_CM)
  {
    return LIDAR_FUSION_MIN_INTENSITY_MID;
  }
  if (raw_cm <= LIDAR_FUSION_INTENSITY_FAR_RANGE_CM)
  {
    return LIDAR_FUSION_MIN_INTENSITY_FAR;
  }
  return LIDAR_FUSION_MIN_INTENSITY_MAX_RANGE;
}

int snrTargetPermilleForDistanceCm(int raw_cm)
{
  if (raw_cm <= LIDAR_FUSION_INTENSITY_NEAR_RANGE_CM)
  {
    return LIDAR_SNR_PERMILLE_TARGET_NEAR;
  }
  if (raw_cm <= LIDAR_FUSION_INTENSITY_MID_RANGE_CM)
  {
    return LIDAR_SNR_PERMILLE_TARGET_MID;
  }
  if (raw_cm <= LIDAR_FUSION_INTENSITY_FAR_RANGE_CM)
  {
    return LIDAR_SNR_PERMILLE_TARGET_FAR;
  }
  return LIDAR_SNR_PERMILLE_TARGET_MAX_RANGE;
}

int computeSnrPermille(uint16_t intensity, uint16_t sunlight_base)
{
  if (sunlight_base == 0)
  {
    return -1;
  }

  int sunlight = max(1, static_cast<int>(sunlight_base));
  return static_cast<int>((static_cast<unsigned long>(intensity) * 1000UL) /
                          static_cast<unsigned long>(sunlight));
}

bool shouldRejectBySnrFloor(int raw_cm, uint16_t intensity, uint16_t sunlight_base)
{
  int snr_permille = computeSnrPermille(intensity, sunlight_base);
  if (snr_permille < 0)
  {
    return false;
  }

  int min_intensity = minIntensityThresholdForDistanceCm(raw_cm);
  int hard_reject_intensity = min_intensity * LIDAR_SNR_HARD_REJECT_INTENSITY_MULTIPLIER;
  return snr_permille < LIDAR_SNR_PERMILLE_HARD_REJECT &&
         static_cast<int>(intensity) < hard_reject_intensity;
}

int snrPenaltyForAmbientLight(int raw_cm,
                              uint16_t intensity,
                              uint16_t sunlight_base,
                              int penalty_max)
{
  int snr_permille = computeSnrPermille(intensity, sunlight_base);
  if (snr_permille < 0)
  {
    return 0;
  }

  int target_snr = snrTargetPermilleForDistanceCm(raw_cm);
  if (snr_permille >= target_snr)
  {
    return 0;
  }

  int deficit = target_snr - snr_permille;
  int penalty = (deficit + (LIDAR_SNR_PENALTY_DIVISOR - 1)) / LIDAR_SNR_PENALTY_DIVISOR;
  return min(penalty_max, penalty);
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

LidarCandidate buildFallbackLidarCandidate(uint16_t raw_distance_mm,
                                           uint16_t intensity,
                                           uint16_t sunlight_base,
                                           DataQuality quality,
                                           int previous_distance_cm)
{
  LidarCandidate candidate = {false, 0, 0, 0};
  if (raw_distance_mm == DTS_INVALID_DISTANCE)
  {
    return candidate;
  }

  int raw_cm = static_cast<int>(raw_distance_mm) / LIDAR_DISTANCE_DIVISOR;
  if (raw_cm <= 0)
  {
    return candidate;
  }

  if (shouldRejectBySnrFloor(raw_cm, intensity, sunlight_base))
  {
    return candidate;
  }

  // Only allow distance-only fallback when there is at least some return signal
  // or the sensor reports non-invalid quality.
  if (intensity < LIDAR_FALLBACK_MIN_INTENSITY && quality == DataQuality::INVALID)
  {
    return candidate;
  }

  int corrected_cm = applyLidarDoubleCorrectionCm(raw_cm);
  if (corrected_cm <= 0)
  {
    return candidate;
  }

  int confidence = LIDAR_FALLBACK_BASE_CONFIDENCE + (qualityBaseScore(quality) / 8);
  if (previous_distance_cm > 0)
  {
    confidence -= min(LIDAR_TEMPORAL_PENALTY_MAX,
                      abs(corrected_cm - previous_distance_cm) / LIDAR_TEMPORAL_PENALTY_DIVISOR);
  }
  confidence -= snrPenaltyForAmbientLight(
      raw_cm, intensity, sunlight_base, LIDAR_SNR_FALLBACK_PENALTY_MAX);
  confidence = constrain(confidence, 0, LIDAR_FALLBACK_MAX_CONFIDENCE);
  if (confidence <= 0)
  {
    return candidate;
  }

  candidate.valid = true;
  candidate.distance_cm = corrected_cm;
  candidate.confidence = confidence;
  candidate.quality_level = max(1, qualityLevelFromDataQuality(quality));
  return candidate;
}

LidarCandidate buildLidarCandidate(uint16_t raw_distance_mm,
                                   uint16_t intensity,
                                   uint16_t sunlight_base,
                                   DataQuality quality,
                                   bool secondary_candidate,
                                   int previous_distance_cm,
                                   bool has_lens_prior,
                                   int lens_prior_cm)
{
  LidarCandidate candidate = {false, 0, 0, 0};

  if (raw_distance_mm == DTS_INVALID_DISTANCE)
  {
    return candidate;
  }

  int raw_cm = static_cast<int>(raw_distance_mm) / LIDAR_DISTANCE_DIVISOR;
  if (raw_cm <= 0)
  {
    return candidate;
  }

  if (intensity < minIntensityThresholdForDistanceCm(raw_cm))
  {
    return candidate;
  }

  if (shouldRejectBySnrFloor(raw_cm, intensity, sunlight_base))
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
    confidence -= min(LIDAR_TEMPORAL_PENALTY_MAX,
                      abs(corrected_cm - previous_distance_cm) / LIDAR_TEMPORAL_PENALTY_DIVISOR);
  }
  confidence -= snrPenaltyForAmbientLight(raw_cm, intensity, sunlight_base, LIDAR_SNR_PENALTY_MAX);

  if (has_lens_prior)
  {
    int prior_error_cm = abs(corrected_cm - lens_prior_cm);
    int effective_error_cm = max(0, prior_error_cm - LIDAR_PRIOR_DEADBAND_CM);
    float prior_weight = priorWeightForQuality(quality) * priorRangeScaleForDistanceCm(corrected_cm);
    int prior_penalty = static_cast<int>(roundf(static_cast<float>(effective_error_cm) * prior_weight));
    confidence -= min(priorPenaltyCapForQuality(quality), prior_penalty);
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

LidarCandidate fuseLidarCandidates(const LidarCandidate &primary, const LidarCandidate &secondary)
{
  int weight_sum = max(1, primary.confidence + secondary.confidence);
  int weighted_distance_sum = (primary.distance_cm * primary.confidence) +
                              (secondary.distance_cm * secondary.confidence);
  int fused_distance_cm = static_cast<int>(roundf(static_cast<float>(weighted_distance_sum) /
                                                  static_cast<float>(weight_sum)));
  int fused_confidence = constrain(((primary.confidence + secondary.confidence) / 2) +
                                       LIDAR_FUSION_CONF_BONUS,
                                   0,
                                   100);
  int fused_quality_level = max(primary.quality_level, secondary.quality_level);
  return {true, fused_distance_cm, fused_confidence, fused_quality_level};
}
} // namespace

LidarCandidate chooseBestLidarCandidate(const DTSMeasurement &measurement,
                                        int previous_distance_cm,
                                        bool has_lens_prior,
                                        int lens_prior_cm)
{
  LidarCandidate primary = buildLidarCandidate(measurement.primaryDistance_mm,
                                               measurement.primaryIntensity,
                                               measurement.sunlightBase,
                                               measurement.primaryQuality,
                                               false,
                                               previous_distance_cm,
                                               has_lens_prior,
                                               lens_prior_cm);

  // Only build secondary candidate when a dual-peak target is present
  // (valid distance AND non-zero intensity, matching library hasSecondaryTarget() logic).
  bool has_secondary = measurement.secondaryDistance_mm != DTS_INVALID_DISTANCE &&
                       measurement.secondaryIntensity > 0;

  LidarCandidate secondary = {};
  if (has_secondary)
  {
    DataQuality secondary_quality = measurement.secondaryQuality;
    if (secondary_quality == DataQuality::INVALID)
    {
      secondary_quality = measurement.primaryQuality;
    }
    secondary = buildLidarCandidate(measurement.secondaryDistance_mm,
                                    measurement.secondaryIntensity,
                                    measurement.sunlightBase,
                                    secondary_quality,
                                    true,
                                    previous_distance_cm,
                                    has_lens_prior,
                                    lens_prior_cm);
  }

  if (primary.valid && secondary.valid &&
      abs(primary.distance_cm - secondary.distance_cm) <= LIDAR_FUSION_AGREE_DELTA_CM)
  {
    return fuseLidarCandidates(primary, secondary);
  }

  if (primary.valid || secondary.valid)
  {
    if (!primary.valid || (secondary.valid && secondary.confidence > primary.confidence))
    {
      return secondary;
    }
    return primary;
  }

  // If quality/intensity gating rejects both candidates at long range, keep a
  // low-confidence distance fallback to avoid prolonged "..." dropouts.
  LidarCandidate fallbackPrimary = buildFallbackLidarCandidate(measurement.primaryDistance_mm,
                                                               measurement.primaryIntensity,
                                                               measurement.sunlightBase,
                                                               measurement.primaryQuality,
                                                               previous_distance_cm);
  if (!has_secondary)
  {
    return fallbackPrimary;
  }

  DataQuality fb_secondary_quality = measurement.secondaryQuality;
  if (fb_secondary_quality == DataQuality::INVALID)
  {
    fb_secondary_quality = measurement.primaryQuality;
  }
  LidarCandidate fallbackSecondary = buildFallbackLidarCandidate(measurement.secondaryDistance_mm,
                                                                 measurement.secondaryIntensity,
                                                                 measurement.sunlightBase,
                                                                 fb_secondary_quality,
                                                                 previous_distance_cm);

  if (fallbackPrimary.valid && fallbackSecondary.valid &&
      abs(fallbackPrimary.distance_cm - fallbackSecondary.distance_cm) <= LIDAR_FUSION_AGREE_DELTA_CM)
  {
    return fuseLidarCandidates(fallbackPrimary, fallbackSecondary);
  }
  if (!fallbackPrimary.valid || (fallbackSecondary.valid && fallbackSecondary.confidence > fallbackPrimary.confidence))
  {
    return fallbackSecondary;
  }
  return fallbackPrimary;
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

  // Avoid "stuck" readings in harsh light where confidence drops but still carries signal.
  float confidence_ratio = static_cast<float>(confidence) / static_cast<float>(LIDAR_CONFIDENCE_MEDIUM);
  float blend_weight = LIDAR_LOW_CONF_BLEND_MIN +
                       ((LIDAR_LOW_CONF_BLEND_MAX - LIDAR_LOW_CONF_BLEND_MIN) * confidence_ratio);
  blend_weight = constrain(blend_weight, LIDAR_LOW_CONF_BLEND_MIN, LIDAR_LOW_CONF_BLEND_MAX);
  float blended = (static_cast<float>(previous_distance_cm) * (1.0f - blend_weight)) +
                  (static_cast<float>(next_distance_cm) * blend_weight);
  int blended_cm = static_cast<int>(roundf(blended));
  int upper_bound = previous_distance_cm + LIDAR_LOW_CONF_MAX_STEP_CM;
  int lower_bound = previous_distance_cm - LIDAR_LOW_CONF_MAX_STEP_CM;
  return constrain(blended_cm, lower_bound, upper_bound);
}

void formatDistanceDisplay(int corrected_cm, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  if (corrected_cm <= 0)
  {
    snprintf(buffer, bufferSize, "<%dcm", LIDAR_DISPLAY_MIN_CM);
    return;
  }
  if (corrected_cm > LIDAR_DISPLAY_INF_THRESHOLD_CM)
  {
    snprintf(buffer, bufferSize, "Inf.");
    return;
  }
  if (corrected_cm < LIDAR_DISPLAY_MIN_CM)
  {
    snprintf(buffer, bufferSize, "<%dcm", LIDAR_DISPLAY_MIN_CM);
    return;
  }
  if (corrected_cm < CM_PER_METER)
  {
    snprintf(buffer, bufferSize, "%dcm", corrected_cm);
    return;
  }
  snprintf(buffer,
           bufferSize,
           "%.*fm",
           DISTANCE_DECIMAL_PLACES,
           static_cast<float>(corrected_cm) / static_cast<float>(CM_PER_METER));
}
