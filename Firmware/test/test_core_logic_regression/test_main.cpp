#include <string>

#include <unity.h>

#include "film_counter_logic.h"
#include "formats.h"
#include "lens_logic.h"
#include "lidar_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"

// Limit the test scope to the core logic modules only.
#include "../../src/film_counter_logic.cpp"
#include "../../src/formats.cpp"
#include "../../src/lens_logic.cpp"
#include "../../src/lenses.cpp"
#include "../../src/lidar_logic.cpp"
#include "../../src/lightmeter_logic.cpp"

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
  TEST_ASSERT_EQUAL_INT(100, blendLidarDistance(100, 200, 40));
}

void test_lidar_invalid_and_display_formatting()
{
  DTSMeasurement measurement = {};
  measurement.primaryDistance_mm = 1000;
  measurement.primaryIntensity = 100; // Below LIDAR_FUSION_MIN_INTENSITY
  measurement.primaryQuality = DataQuality::GOOD;
  measurement.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  measurement.secondaryIntensity = 0;
  measurement.secondaryQuality = DataQuality::INVALID;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);

  TEST_ASSERT_EQUAL_STRING("< 5cm", formatDistanceDisplay(4).c_str());
  TEST_ASSERT_EQUAL_STRING("> 18m", formatDistanceDisplay(1900).c_str());
  TEST_ASSERT_EQUAL_STRING("1.5m", formatDistanceDisplay(150).c_str());
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
  RUN_TEST(test_lidar_candidate_selection_and_blend);
  RUN_TEST(test_lidar_invalid_and_display_formatting);
  RUN_TEST(test_lens_snap_and_distance_estimation);
  RUN_TEST(test_lightmeter_dark_bright_fraction_and_seconds);
  return UNITY_END();
}
