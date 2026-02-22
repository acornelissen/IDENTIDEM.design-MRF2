#include <string>
#include <cstring>

#include <unity.h>

#include "film_counter_logic.h"
#include "formats.h"
#include "calibration_logic.h"
#include "lens_logic.h"
#include "lidar_recovery_logic.h"
#include "lidar_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"
#include "prefs_migration_logic.h"

// Limit the test scope to the core logic modules only.
#include "../../src/calibration_logic.cpp"
#include "../../src/film_counter_logic.cpp"
#include "../../src/formats.cpp"
#include "../../src/lens_logic.cpp"
#include "../../src/lidar_recovery_logic.cpp"
#include "../../src/lenses.cpp"
#include "../../src/lidar_logic.cpp"
#include "../../src/lightmeter_logic.cpp"
#include "../../src/prefs_migration_logic.cpp"

namespace
{
Lens makeTestLens()
{
  Lens lens = {
      999,
      "TEST",
      90.0f,
      {100, 200, 300, 400, 500, 600, 700},
      {1.0f, 1.2f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f},
      {0.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f},
      {0, 0, 0, 0},
      true};
  return lens;
}
} // namespace

void setUp() {}
void tearDown() {}

void test_frame_counter_exact_and_interpolation()
{
  const FilmFormat &format = film_formats[3]; // 6x7

  FilmCounterEstimate exact = estimateFilmCounter(format, 140);
  TEST_ASSERT_TRUE(exact.valid);
  TEST_ASSERT_EQUAL_INT(1, exact.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, exact.progress);

  FilmCounterEstimate interpolated = estimateFilmCounter(format, 150);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_EQUAL_INT(1, interpolated.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f / 34.0f, interpolated.progress);
}

void test_frame_counter_snap_and_roll_end()
{
  const FilmFormat &format = film_formats[3]; // 6x7

  FilmCounterEstimate snapped = estimateFilmCounter(format, 173);
  TEST_ASSERT_TRUE(snapped.valid);
  TEST_ASSERT_EQUAL_INT(2, snapped.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snapped.progress);

  FilmCounterEstimate end = estimateFilmCounter(format, 600);
  TEST_ASSERT_TRUE(end.valid);
  TEST_ASSERT_EQUAL_INT(FILM_COUNTER_END, end.frame);
}

void test_frame_counter_frame_offset_and_spacing()
{
  const FilmFormat &format = film_formats[3]; // 6x7

  FilmCounterEstimate shiftedStart = estimateFilmCounter(format, 145, 5, 0);
  TEST_ASSERT_TRUE(shiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(1, shiftedStart.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, shiftedStart.progress);

  FilmCounterEstimate beforeShiftedStart = estimateFilmCounter(format, 140, 5, 0);
  TEST_ASSERT_TRUE(beforeShiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(0, beforeShiftedStart.frame);

  FilmCounterEstimate widerSpacingFrame2 = estimateFilmCounter(format, 176, 0, 2);
  TEST_ASSERT_TRUE(widerSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(2, widerSpacingFrame2.frame);

  FilmCounterEstimate beforeWiderSpacingFrame2 = estimateFilmCounter(format, 174, 0, 2);
  TEST_ASSERT_TRUE(beforeWiderSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(1, beforeWiderSpacingFrame2.frame);
}

void test_encoder_filter_forward_hysteresis_and_debounce()
{
  EncoderFilterState state = {};
  resetEncoderFilterState(state, 100, 0);

  EncoderFilterDecision jitter = updateEncoderFilter(state, 101, 10, false);
  TEST_ASSERT_FALSE(jitter.accepted);

  EncoderFilterDecision beginMove = updateEncoderFilter(state, 103, 20, false);
  TEST_ASSERT_FALSE(beginMove.accepted);

  EncoderFilterDecision bounceBack = updateEncoderFilter(state, 101, 30, false);
  TEST_ASSERT_FALSE(bounceBack.accepted);

  EncoderFilterDecision rearmMove = updateEncoderFilter(state, 103, 40, false);
  TEST_ASSERT_FALSE(rearmMove.accepted);

  EncoderFilterDecision tooSoon = updateEncoderFilter(state, 104, 60, false);
  TEST_ASSERT_FALSE(tooSoon.accepted);

  EncoderFilterDecision accepted = updateEncoderFilter(state, 105, 80, false);
  TEST_ASSERT_TRUE(accepted.accepted);
  TEST_ASSERT_EQUAL_INT(105, accepted.accepted_position);
}

void test_encoder_filter_reverse_requires_rewind_mode()
{
  EncoderFilterState disabledRewind = {};
  resetEncoderFilterState(disabledRewind, 200, 0);

  EncoderFilterDecision reverseIgnored = updateEncoderFilter(disabledRewind, 195, 200, false);
  TEST_ASSERT_FALSE(reverseIgnored.accepted);

  EncoderFilterState enabledRewind = {};
  resetEncoderFilterState(enabledRewind, 300, 0);

  EncoderFilterDecision belowHysteresis = updateEncoderFilter(enabledRewind, 298, 10, true);
  TEST_ASSERT_FALSE(belowHysteresis.accepted);

  EncoderFilterDecision beginReverse = updateEncoderFilter(enabledRewind, 295, 20, true);
  TEST_ASSERT_FALSE(beginReverse.accepted);

  EncoderFilterDecision reverseTooSoon = updateEncoderFilter(enabledRewind, 294, 80, true);
  TEST_ASSERT_FALSE(reverseTooSoon.accepted);

  EncoderFilterDecision reverseAccepted = updateEncoderFilter(enabledRewind, 292, 150, true);
  TEST_ASSERT_TRUE(reverseAccepted.accepted);
  TEST_ASSERT_EQUAL_INT(292, reverseAccepted.accepted_position);
}

void test_lidar_timeout_recovery_and_backoff()
{
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  LidarRecoveryDecision timeoutEarly =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1000);
  TEST_ASSERT_FALSE(timeoutEarly.attempt_recovery);

  LidarRecoveryDecision timeoutLate =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1700);
  TEST_ASSERT_TRUE(timeoutLate.clear_display);
  TEST_ASSERT_TRUE(timeoutLate.attempt_recovery);

  noteLidarRecoveryAttemptResult(state, false, 1700);
  TEST_ASSERT_TRUE(state.recovering);
  TEST_ASSERT_GREATER_THAN_UINT32(1700, state.next_recovery_attempt_ms);

  LidarRecoveryDecision waitBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, state.next_recovery_attempt_ms - 1);
  TEST_ASSERT_FALSE(waitBackoff.attempt_recovery);

  LidarRecoveryDecision afterBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, state.next_recovery_attempt_ms);
  TEST_ASSERT_TRUE(afterBackoff.attempt_recovery);

  noteLidarRecoveryAttemptResult(state, true, state.next_recovery_attempt_ms);
  TEST_ASSERT_FALSE(state.recovering);
  TEST_ASSERT_EQUAL_INT(0, state.consecutive_errors);
}

void test_calibration_validation_stable_and_monotonic()
{
  const int stableSamples[8] = {300, 301, 299, 300, 302, 298, 301, 350};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(stableSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_TRUE(stable);
  TEST_ASSERT_INT_WITHIN(2, 300, averagedReading);

  const int unstableSamples[8] = {300, 320, 280, 310, 260, 340, 300, 280};
  TEST_ASSERT_FALSE(computeStableCalibrationReading(unstableSamples, 8, 6, 5, 10, averagedReading));

  const int increasing[4] = {330, 320, 310, 300};
  TEST_ASSERT_TRUE(validateMonotonicCalibration(increasing, 4, 1));

  const int nonMonotonic[4] = {330, 320, 325, 300};
  TEST_ASSERT_FALSE(validateMonotonicCalibration(nonMonotonic, 4, 1));
}

void test_prefs_migration_mode_and_blob_apply()
{
  size_t expectedBytes = expectedLegacyLensBlobSize(2);
  TEST_ASSERT_EQUAL_UINT32(sizeof(Lens) * 2, expectedBytes);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::LOAD_SCHEMA),
      static_cast<int>(selectPrefsLoadMode(2, 2, expectedBytes, expectedBytes)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::MIGRATE_LEGACY),
      static_cast<int>(selectPrefsLoadMode(0, 2, expectedBytes, expectedBytes)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::LOAD_DEFAULTS),
      static_cast<int>(selectPrefsLoadMode(0, 2, expectedBytes - 1, expectedBytes)));

  Lens legacySource[2] = {
      {1001, "LEGACY-A", 1.0f, {1, 2, 3, 4, 5, 6, 7}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, true},
      {1002, "LEGACY-B", 1.0f, {10, 20, 30, 40, 50, 60, 70}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, false}};

  uint8_t legacyBlob[sizeof(legacySource)] = {};
  memcpy(legacyBlob, legacySource, sizeof(legacyBlob));

  Lens targets[2] = {
      {2001, "TARGET-A", 1.0f, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, false},
      {2002, "TARGET-B", 1.0f, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, true}};

  TEST_ASSERT_TRUE(applyLegacyLensBlob(legacyBlob, sizeof(legacyBlob), targets, 2));
  TEST_ASSERT_EQUAL_INT(1, targets[0].sensor_reading[0]);
  TEST_ASSERT_EQUAL_INT(70, targets[1].sensor_reading[6]);
  TEST_ASSERT_TRUE(targets[0].calibrated);
  TEST_ASSERT_FALSE(targets[1].calibrated);

  TEST_ASSERT_FALSE(applyLegacyLensBlob(legacyBlob, sizeof(legacyBlob) - 1, targets, 2));
}

void test_lidar_candidate_selection_and_blend()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 1200;
  measurement.primaryIntensity = 180;
  measurement.primaryQuality = DataQuality::FAIR;
  measurement.secondaryDistance_mm = 1000;
  measurement.secondaryIntensity = 1200;
  measurement.secondaryQuality = DataQuality::EXCELLENT;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_EQUAL_INT(4, candidate.quality_level);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);

  TEST_ASSERT_EQUAL_INT(200, blendLidarDistance(100, 200, 80));
  TEST_ASSERT_EQUAL_INT(135, blendLidarDistance(100, 200, 60));
  TEST_ASSERT_EQUAL_INT(124, blendLidarDistance(100, 200, 40));
}

void test_lidar_invalid_and_display_formatting()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 1000;
  measurement.primaryIntensity = 50; // Below LIDAR_FUSION_MIN_INTENSITY
  measurement.primaryQuality = DataQuality::GOOD;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);

  TEST_ASSERT_EQUAL_STRING("<15cm", formatDistanceDisplay(4).c_str());
  TEST_ASSERT_EQUAL_STRING("75cm", formatDistanceDisplay(75).c_str());
  TEST_ASSERT_EQUAL_STRING("10.5m", formatDistanceDisplay(1050).c_str());
  TEST_ASSERT_EQUAL_STRING("Inf.", formatDistanceDisplay(1051).c_str());
  TEST_ASSERT_EQUAL_STRING("Inf.", formatDistanceDisplay(1900).c_str());
  TEST_ASSERT_EQUAL_STRING("1.5m", formatDistanceDisplay(150).c_str());
}

void test_lidar_low_confidence_tracks_beyond_previous_distance()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 6000;   // 6.0m target
  measurement.primaryIntensity = 100;      // low/harsh-light like return, but valid
  measurement.primaryQuality = DataQuality::POOR;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  int filtered_distance_cm = 250;
  const int lens_prior_cm = 250;
  for (int i = 0; i < 8; i++)
  {
    LidarCandidate candidate =
        chooseBestLidarCandidate(measurement, filtered_distance_cm, true, lens_prior_cm);
    TEST_ASSERT_TRUE(candidate.valid);
    filtered_distance_cm =
        blendLidarDistance(filtered_distance_cm, candidate.distance_cm, candidate.confidence);
  }

  TEST_ASSERT_GREATER_THAN_INT(350, filtered_distance_cm);
}

void test_lidar_dynamic_intensity_threshold_accepts_mid_range()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 3200; // 3.2m
  measurement.primaryIntensity = LIDAR_FUSION_MIN_INTENSITY_MID;
  measurement.primaryQuality = DataQuality::GOOD;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
}

void test_lidar_far_fallback_prevents_dropouts()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 4200; // 4.2m
  measurement.primaryIntensity = LIDAR_FALLBACK_MIN_INTENSITY + 1;
  measurement.primaryQuality = DataQuality::INVALID;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 250, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(1, candidate.quality_level);
}

void test_lidar_candidate_fusion_when_returns_agree()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 3000;     // 300cm
  measurement.primaryIntensity = 180;
  measurement.primaryQuality = DataQuality::GOOD;
  measurement.secondaryDistance_mm = 3120;   // 312cm (within fusion delta)
  measurement.secondaryIntensity = 170;
  measurement.secondaryQuality = DataQuality::FAIR;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_INT_WITHIN(2, 305, candidate.distance_cm);
  TEST_ASSERT_GREATER_THAN_INT(55, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(3, candidate.quality_level);
}

void test_lidar_lens_prior_is_range_weighted()
{
  DTSMeasurement nearMeasurement = {};
  nearMeasurement.primaryDistance_mm = 2000; // 200cm
  nearMeasurement.primaryIntensity = 200;
  nearMeasurement.primaryQuality = DataQuality::GOOD;
  nearMeasurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  nearMeasurement.secondaryIntensity = 0;
  nearMeasurement.secondaryQuality = DataQuality::INVALID;

  DTSMeasurement farMeasurement = nearMeasurement;
  farMeasurement.primaryDistance_mm = 8000; // 800cm

  // Keep equal prior mismatch (100cm) in both ranges; near range should be penalized more.
  LidarCandidate nearCandidate = chooseBestLidarCandidate(nearMeasurement, 0, true, 100);
  LidarCandidate farCandidate = chooseBestLidarCandidate(farMeasurement, 0, true, 700);

  TEST_ASSERT_TRUE(nearCandidate.valid);
  TEST_ASSERT_TRUE(farCandidate.valid);
  TEST_ASSERT_LESS_THAN_INT(farCandidate.confidence, nearCandidate.confidence);
}

void test_lidar_sunlight_penalizes_confidence_without_dropping_valid_measurement()
{
  DTSMeasurement lowAmbientMeasurement = {};
  lowAmbientMeasurement.primaryDistance_mm = 4500; // 450cm
  lowAmbientMeasurement.primaryIntensity = 40;
  lowAmbientMeasurement.sunlightBase = 25;
  lowAmbientMeasurement.primaryQuality = DataQuality::GOOD;
  lowAmbientMeasurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  lowAmbientMeasurement.secondaryIntensity = 0;
  lowAmbientMeasurement.secondaryQuality = DataQuality::INVALID;

  DTSMeasurement highAmbientMeasurement = lowAmbientMeasurement;
  highAmbientMeasurement.sunlightBase = 650;

  LidarCandidate lowAmbientCandidate = chooseBestLidarCandidate(lowAmbientMeasurement, 0, false, 0);
  LidarCandidate highAmbientCandidate = chooseBestLidarCandidate(highAmbientMeasurement, 0, false, 0);

  TEST_ASSERT_TRUE(lowAmbientCandidate.valid);
  TEST_ASSERT_TRUE(highAmbientCandidate.valid);
  TEST_ASSERT_LESS_THAN_INT(lowAmbientCandidate.confidence, highAmbientCandidate.confidence);
}

void test_lidar_sunlight_hard_rejects_very_low_snr_measurement()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 7000; // 700cm
  measurement.primaryIntensity = LIDAR_FUSION_MIN_INTENSITY_FAR;
  measurement.sunlightBase = 1500;
  measurement.primaryQuality = DataQuality::GOOD;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);
}

void test_lens_snap_and_distance_estimation()
{
  Lens lens = makeTestLens();

  TEST_ASSERT_EQUAL_INT(1, findLensSnapIndex(lens, 201));
  TEST_ASSERT_EQUAL_INT(4, findLensSnapIndex(lens, 503));
  TEST_ASSERT_EQUAL_INT(-1, findLensSnapIndex(lens, 102));

  LensDistanceEstimate below = estimateLensDistance(lens, 90);
  TEST_ASSERT_TRUE(below.valid);
  TEST_ASSERT_FALSE(below.is_infinity);
  TEST_ASSERT_EQUAL_INT(100, below.distance_cm);

  LensDistanceEstimate interpolated = estimateLensDistance(lens, 250);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_FALSE(interpolated.is_infinity);
  TEST_ASSERT_EQUAL_INT(135, interpolated.distance_cm);

  LensDistanceEstimate infinity = estimateLensDistance(lens, 706);
  TEST_ASSERT_TRUE(infinity.valid);
  TEST_ASSERT_TRUE(infinity.is_infinity);
  TEST_ASSERT_EQUAL_INT(LENS_INFINITY_RAW, infinity.distance_cm);
}

void test_lightmeter_dark_bright_fraction_and_seconds()
{
  TEST_ASSERT_EQUAL_STRING("Dark!", formatShutterSpeed(0.0f, 8.0f, 400).c_str());
  TEST_ASSERT_EQUAL_STRING("Bright!", formatShutterSpeed(50000.0f, 2.0f, 100).c_str());
  TEST_ASSERT_EQUAL_STRING("1/125 sec.", formatShutterSpeed(320.0f, 8.0f, 400).c_str());
  TEST_ASSERT_EQUAL_STRING("3.20 sec.", formatShutterSpeed(0.5f, 2.0f, 50).c_str());
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_frame_counter_exact_and_interpolation);
  RUN_TEST(test_frame_counter_snap_and_roll_end);
  RUN_TEST(test_frame_counter_frame_offset_and_spacing);
  RUN_TEST(test_encoder_filter_forward_hysteresis_and_debounce);
  RUN_TEST(test_encoder_filter_reverse_requires_rewind_mode);
  RUN_TEST(test_lidar_timeout_recovery_and_backoff);
  RUN_TEST(test_calibration_validation_stable_and_monotonic);
  RUN_TEST(test_prefs_migration_mode_and_blob_apply);
  RUN_TEST(test_lidar_candidate_selection_and_blend);
  RUN_TEST(test_lidar_invalid_and_display_formatting);
  RUN_TEST(test_lidar_low_confidence_tracks_beyond_previous_distance);
  RUN_TEST(test_lidar_dynamic_intensity_threshold_accepts_mid_range);
  RUN_TEST(test_lidar_far_fallback_prevents_dropouts);
  RUN_TEST(test_lidar_candidate_fusion_when_returns_agree);
  RUN_TEST(test_lidar_lens_prior_is_range_weighted);
  RUN_TEST(test_lidar_sunlight_penalizes_confidence_without_dropping_valid_measurement);
  RUN_TEST(test_lidar_sunlight_hard_rejects_very_low_snr_measurement);
  RUN_TEST(test_lens_snap_and_distance_estimation);
  RUN_TEST(test_lightmeter_dark_bright_fraction_and_seconds);
  return UNITY_END();
}
