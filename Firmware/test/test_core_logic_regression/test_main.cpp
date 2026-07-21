#include <string>
#include <cstring>

#include <unity.h>

#include "film_counter_logic.h"
#include "formats.h"
#include "formatting_logic.h"
#include "frameline_layout_logic.h"
#include "boot_animation_logic.h"
#include "calibration_logic.h"
#include "lens_logic.h"
#include "lens_spike_logic.h"
#include "lidar_recovery_logic.h"
#include "lidar_logic.h"
#include "lightmeter_logic.h"
#include "lidar_recovery_actions.h"
#include "mrfconstants.h"
#include "prefs_clamp_logic.h"
#include "prefs_keys.h"
#include "prefs_migration_logic.h"
#include "ui_signature_logic.h"

// Limit the test scope to the core logic modules only.
#include "../../src/boot_animation_logic.cpp"
#include "../../src/prefs_clamp_logic.cpp"
#include "../../src/calibration_logic.cpp"
#include "../../src/config_header_logic.cpp"
#include "../../src/film_counter_logic.cpp"
#include "../../src/formats.cpp"
#include "../../src/formatting_logic.cpp"
#include "../../src/frameline_layout_logic.cpp"
#include "../../src/lens_logic.cpp"
#include "../../src/lens_spike_logic.cpp"
#include "../../src/lidar_recovery_logic.cpp"
#include "../../src/lenses.cpp"
#include "../../src/lidar_logic.cpp"
#include "../../src/lightmeter_logic.cpp"
#include "../../src/prefs_migration_logic.cpp"
#include "../../src/ui_signature_logic.cpp"

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

const FilmFormat *findFormatByName(const char *formatName)
{
  for (size_t i = 0; i < NUM_FILM_FORMATS; i++)
  {
    if (strcmp(film_formats[i].name, formatName) == 0)
    {
      return &film_formats[i];
    }
  }
  return nullptr;
}

const Lens *findLensById(int id)
{
  for (size_t i = 0; i < NUM_LENSES; i++)
  {
    if (lenses[i].id == id)
    {
      return &lenses[i];
    }
  }
  return nullptr;
}

// A primary-only return with no secondary peak. Mirrors the most common
// DTS6012M measurement shape in the field. sunlightBase stays 0; tests that
// need a specific ambient level set it after the helper returns.
DTSMeasurement makeLidarMeasurement(uint16_t primaryDistance_mm,
                                    uint16_t primaryIntensity,
                                    DataQuality primaryQuality)
{
  DTSMeasurement m = {};
  m.primaryDistance_mm = primaryDistance_mm;
  m.primaryIntensity = primaryIntensity;
  m.primaryQuality = primaryQuality;
  m.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  m.secondaryIntensity = 0;
  m.secondaryQuality = DataQuality::INVALID;
  return m;
}

DTSMeasurement makeLidarDualPeakMeasurement(uint16_t primaryDistance_mm,
                                            uint16_t primaryIntensity,
                                            DataQuality primaryQuality,
                                            uint16_t secondaryDistance_mm,
                                            uint16_t secondaryIntensity,
                                            DataQuality secondaryQuality)
{
  DTSMeasurement m = makeLidarMeasurement(primaryDistance_mm, primaryIntensity, primaryQuality);
  m.secondaryDistance_mm = secondaryDistance_mm;
  m.secondaryIntensity = secondaryIntensity;
  m.secondaryQuality = secondaryQuality;
  return m;
}
} // namespace

void setUp() {}
void tearDown() {}

void test_frame_counter_exact_and_interpolation()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate exact = estimateFilmCounter(*format, 140);
  TEST_ASSERT_TRUE(exact.valid);
  TEST_ASSERT_EQUAL_INT(1, exact.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, exact.progress);

  FilmCounterEstimate interpolated = estimateFilmCounter(*format, 150);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_EQUAL_INT(1, interpolated.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f / 34.0f, interpolated.progress);
}

void test_frame_counter_snap_and_roll_end()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate snapped = estimateFilmCounter(*format, 173);
  TEST_ASSERT_TRUE(snapped.valid);
  TEST_ASSERT_EQUAL_INT(2, snapped.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snapped.progress);

  FilmCounterEstimate end = estimateFilmCounter(*format, 600);
  TEST_ASSERT_TRUE(end.valid);
  TEST_ASSERT_EQUAL_INT(FILM_COUNTER_END, end.frame);
}

void test_frame_counter_frame_offset_and_spacing()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate shiftedStart = estimateFilmCounter(*format, 145, 5, 0);
  TEST_ASSERT_TRUE(shiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(1, shiftedStart.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, shiftedStart.progress);

  FilmCounterEstimate beforeShiftedStart = estimateFilmCounter(*format, 140, 5, 0);
  TEST_ASSERT_TRUE(beforeShiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(0, beforeShiftedStart.frame);

  FilmCounterEstimate widerSpacingFrame2 = estimateFilmCounter(*format, 176, 0, 2);
  TEST_ASSERT_TRUE(widerSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(2, widerSpacingFrame2.frame);

  FilmCounterEstimate beforeWiderSpacingFrame2 = estimateFilmCounter(*format, 174, 0, 2);
  TEST_ASSERT_TRUE(beforeWiderSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(1, beforeWiderSpacingFrame2.frame);
}

void test_format_3x6_supports_21_frames()
{
  const FilmFormat *format = findFormatByName("3x6");
  TEST_ASSERT_NOT_NULL(format);
  TEST_ASSERT_EQUAL_INT(21, getFilmFormatMaxFrame(*format));
  TEST_ASSERT_EQUAL_INT(23, getFilmFormatPointCount(*format));
  TEST_ASSERT_EQUAL_INT(99, format->frame[getFilmFormatPointCount(*format) - 1]);

  // 3x6 is a 120-film format. Catch regressions to the v10.4.7-and-earlier bug
  // where its sensor[] table was a copy of PANO (35mm film) with one entry
  // appended — leading delta ~37, end sentinel 800 — causing every-other-frame
  // exposure on 120. Real 120 formats have a leader-paper offset of ~130+.
  TEST_ASSERT_GREATER_OR_EQUAL_INT(120, format->sensor[1]);
  TEST_ASSERT_EQUAL_INT(550, format->sensor[getFilmFormatPointCount(*format) - 1]);
}

void test_config_header_autoscales_only_when_9x15_overflows()
{
  // Available width for the title is SCREEN_WIDTH - CONFIG_TITLE_X = 128 - 3.
  const int available = 128 - 3;

  // Headers that fit at 9x15 keep the large font.
  TEST_ASSERT_FALSE(configHeaderNeedsSmallFont(5, available));  // "Setup"
  TEST_ASSERT_FALSE(configHeaderNeedsSmallFont(13, available)); // "Setup > LiDAR" (117px)

  // Headers that would clip fall back to 6x10.
  TEST_ASSERT_TRUE(configHeaderNeedsSmallFont(14, available));  // "Factory Reset?" (126px)
  TEST_ASSERT_TRUE(configHeaderNeedsSmallFont(15, available));  // "Setup > Display" (135px)
  TEST_ASSERT_TRUE(configHeaderNeedsSmallFont(17, available));  // "Display > Horizon" (153px)

  // Boundary: 13 chars fit (117 <= 125), 14 do not (126 > 125).
  TEST_ASSERT_EQUAL_INT(117, configHeaderTextWidthPx(13, 9));
  TEST_ASSERT_EQUAL_INT(126, configHeaderTextWidthPx(14, 9));
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

void test_lidar_recovery_state_machine_survives_many_consecutive_failures()
{
  // Regression guard for the SHAPE of the v10.4.7 incident. The original bug
  // was in applyLidarCalibrationProfile() re-issuing setFrameRate from the
  // recovery path, leaving some DTS6012M units unable to recover; from the
  // state machine's perspective that manifested as the Recoveries counter
  // climbing indefinitely. The hardware-side fix isn't unit-testable (no
  // mock for the DTS library) — what we can pin is that the state machine
  // layer stays well-behaved under that scenario: consecutive_errors must
  // saturate, the schedule must keep advancing, and a single late success
  // must still drop the machine cleanly back to idle.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  // Cross the error threshold so recovery engages.
  for (int i = 0; i < LIDAR_RECOVERY_ERROR_THRESHOLD; i++)
  {
    updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, 100 + i);
  }
  TEST_ASSERT_TRUE(state.recovering);

  // 20 failed recovery attempts. consecutive_errors must stay bounded; each
  // attempt time must be >= the previous one (no schedule going backwards,
  // which would mean an unbounded retry spin).
  for (int i = 0; i < 20; i++)
  {
    unsigned long previousAttemptTime = state.next_recovery_attempt_ms;
    noteLidarRecoveryAttemptResult(state, false, previousAttemptTime);
    TEST_ASSERT_LESS_OR_EQUAL_INT(10, state.consecutive_errors);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(previousAttemptTime, state.next_recovery_attempt_ms);
  }

  // A single late success after 20 failures must still reset the machine.
  noteLidarRecoveryAttemptResult(state, true, state.next_recovery_attempt_ms);
  TEST_ASSERT_FALSE(state.recovering);
  TEST_ASSERT_EQUAL_INT(0, state.consecutive_errors);
}

void test_lidar_recovery_event_mapping_treats_no_new_data_as_benign()
{
  // Library v2.6.0 changed update() to return NO_NEW_DATA (not TIMEOUT) when no
  // complete frame arrived this poll. It must map to the time-based TIMEOUT event,
  // exactly like a real TIMEOUT — never to the count-based ERROR event.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LidarRecoveryEvent::TIMEOUT),
      static_cast<int>(lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LidarRecoveryEvent::TIMEOUT),
      static_cast<int>(lidarRecoveryEventForUpdateError(DTSError::TIMEOUT)));

  // Genuine frame faults still map to the ERROR event so recovery can engage.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LidarRecoveryEvent::ERROR),
      static_cast<int>(lidarRecoveryEventForUpdateError(DTSError::CRC_CHECK_FAILED)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LidarRecoveryEvent::ERROR),
      static_cast<int>(lidarRecoveryEventForUpdateError(DTSError::FRAME_HEADER_INVALID)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LidarRecoveryEvent::ERROR),
      static_cast<int>(lidarRecoveryEventForUpdateError(DTSError::BUFFER_OVERFLOW)));
}

void test_lidar_no_new_data_polls_do_not_trip_spurious_recovery()
{
  // The v2.6.0 hazard: a healthy sensor returns NO_NEW_DATA on every poll between
  // frames. If those mapped to ERROR, LIDAR_RECOVERY_ERROR_THRESHOLD fast polls
  // would trip recovery in milliseconds. With the correct mapping, many rapid
  // no-data polls within the timeout window must NOT trigger recovery.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  for (int i = 0; i < LIDAR_RECOVERY_ERROR_THRESHOLD * 5; i++)
  {
    LidarRecoveryEvent event = lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA);
    // Poll rapidly (1ms apart), staying inside LIDAR_RECOVERY_TIMEOUT_MS.
    LidarRecoveryDecision decision = updateLidarRecoveryState(state, event, 1 + i);
    TEST_ASSERT_FALSE(decision.attempt_recovery);
    TEST_ASSERT_FALSE(state.recovering);
  }
  TEST_ASSERT_EQUAL_INT(0, state.consecutive_errors);

  // Only a sustained stall past LIDAR_RECOVERY_TIMEOUT_MS with no valid frame
  // escalates to recovery.
  LidarRecoveryDecision stalled = updateLidarRecoveryState(
      state, lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA),
      LIDAR_RECOVERY_TIMEOUT_MS + 1);
  TEST_ASSERT_TRUE(stalled.attempt_recovery);
}

void test_lidar_recovery_reset_on_enable_prevents_instant_wake_recovery()
{
  // Wake-from-sleep hazard: the first update() after re-enabling the sensor is
  // inevitably NO_NEW_DATA (no frame can exist yet). If the recovery state still
  // carries the pre-sleep last_valid_measurement_ms, the very first post-wake
  // poll sits past LIDAR_RECOVERY_TIMEOUT_MS and recovery fires instantly —
  // running resetState()+enableSensor() back-to-back with the wake enable (the
  // v10.4.7 no-settle hazard) and incrementing the Health "Recoveries" counter
  // on every wake.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  // Without an enable-time reset, a stale state trips recovery on the first poll.
  unsigned long wake_ms = LIDAR_RECOVERY_TIMEOUT_MS * 10;
  LidarRecoveryState staleState = state;
  LidarRecoveryDecision staleDecision = updateLidarRecoveryState(
      staleState, lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA), wake_ms);
  TEST_ASSERT_TRUE(staleDecision.attempt_recovery);

  // The enable path must reset the timing baseline so post-wake NO_NEW_DATA
  // polls get the full timeout window before recovery is considered.
  resetLidarRecoveryState(state, wake_ms);
  LidarRecoveryDecision decision = updateLidarRecoveryState(
      state, lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA), wake_ms + 1);
  TEST_ASSERT_FALSE(decision.attempt_recovery);
  TEST_ASSERT_FALSE(decision.clear_display);
  TEST_ASSERT_FALSE(state.recovering);
}

void test_calibration_logic_rejects_invalid_input_shapes()
{
  // Defensive guards in calibration_logic.cpp that nothing currently tested
  // was exercising. computeStableCalibrationReading is called with caller-
  // supplied buffers and counts; a future refactor that wires up a new
  // capture path could pass null or empty by mistake, and these tests
  // document the contract.
  int averaged = 0;

  // Null sample buffer → false.
  TEST_ASSERT_FALSE(computeStableCalibrationReading(nullptr, 8, 6, 5, 10, averaged));

  // Non-positive sample count → false.
  const int dummy[1] = {300};
  TEST_ASSERT_FALSE(computeStableCalibrationReading(dummy, 0, 6, 5, 10, averaged));
  TEST_ASSERT_FALSE(computeStableCalibrationReading(dummy, -1, 6, 5, 10, averaged));

  // sample_count larger than the internal sorted-buffer capacity → false.
  TEST_ASSERT_FALSE(
      computeStableCalibrationReading(dummy, CALIB_SAMPLE_COUNT + 1, 6, 5, 10, averaged));

  // validateMonotonicCalibration: empty or single-element sequences are vacuously
  // monotonic. Null pointer also treated as the empty case.
  TEST_ASSERT_TRUE(validateMonotonicCalibration(nullptr, 0, CALIB_MONOTONIC_MIN_STEP));
  TEST_ASSERT_TRUE(validateMonotonicCalibration(dummy, 0, CALIB_MONOTONIC_MIN_STEP));
  TEST_ASSERT_TRUE(validateMonotonicCalibration(dummy, 1, CALIB_MONOTONIC_MIN_STEP));
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

void test_ascending_calibration_requires_increasing_readings()
{
  // Focus calibration must capture readings that rise with distance, so a
  // backwards-wired sensor (descending capture) is rejected rather than stored
  // as a table the estimator would read inverted.
  const int ascending[4] = {261, 268, 276, 287};
  TEST_ASSERT_TRUE(isAscendingCalibration(ascending, 4));
  const int descending[4] = {330, 320, 310, 300};
  TEST_ASSERT_FALSE(isAscendingCalibration(descending, 4));
  // Fewer than two readings: direction is not yet determined, so accept.
  TEST_ASSERT_TRUE(isAscendingCalibration(ascending, 1));
  TEST_ASSERT_TRUE(isAscendingCalibration(nullptr, 0));
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
  DTSMeasurement measurement = makeLidarDualPeakMeasurement(
      1200, 180, DataQuality::FAIR,
      1000, 1200, DataQuality::EXCELLENT);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_EQUAL_INT(4, candidate.quality_level);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);

  TEST_ASSERT_EQUAL_INT(188, blendLidarDistance(100, 200, 80)); // near-range high-conf blend: 100*0.12 + 200*0.88 = 188
  TEST_ASSERT_EQUAL_INT(115, blendLidarDistance(80, 120, 80));  // typical near-range high-conf: 80*0.12 + 120*0.88 = 115
  TEST_ASSERT_EQUAL_INT(210, blendLidarDistance(180, 210, 80)); // just-above-boundary at high conf: pass-through, no blend
  TEST_ASSERT_EQUAL_INT(150, blendLidarDistance(0, 150, 80));   // first reading (previous=0): pass-through regardless of distance
  TEST_ASSERT_EQUAL_INT(135, blendLidarDistance(100, 200, 60));
  TEST_ASSERT_EQUAL_INT(131, blendLidarDistance(100, 200, 40));
}

void test_lidar_plausibility_gate_rejects_overshoot()
{
  // Lens at 1.0m, low-quality LiDAR return far past +overshoot threshold: implausible.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(800, 100, 1));
  // Lens at 1.5m, LiDAR returns 4.0m: implausible (well past the scaled overshoot).
  TEST_ASSERT_TRUE(isLidarReadingImplausible(400, 150, 1));
}

void test_lidar_plausibility_gate_allows_undershoot_and_far_focus()
{
  // Lens at 1.0m, LiDAR returns 1.2m: well within overshoot delta — allowed.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(120, 100, 1));
  // Lens at 1.0m, LiDAR returns 0.6m: undershoot is allowed (e.g. something passed in front).
  TEST_ASSERT_FALSE(isLidarReadingImplausible(60, 100, 1));
  // Lens focused beyond near-range gate boundary: gate disabled regardless of overshoot.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(2000, 500, 1));
  // No lens prior available: gate disabled.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(800, 0, 1));
  // Lens focused at 2.5m is within the gate's coverage (boundary 3m).
  TEST_ASSERT_TRUE(isLidarReadingImplausible(800, 250, 1));
  // Lens at boundary itself (300cm) still gated; lens just past it (301cm) not gated.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(800, 300, 1));
  TEST_ASSERT_FALSE(isLidarReadingImplausible(800, 301, 1));
}

void test_lidar_plausibility_gate_boundary_at_overshoot_delta()
{
  // Lens at 100cm: allowance is the 200cm floor, so 100+200=300 is the boundary.
  // Reading 300cm is NOT implausible (strict greater-than).
  TEST_ASSERT_FALSE(isLidarReadingImplausible(300, 100, 1));
  // 301cm IS implausible.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(301, 100, 1));
}

void test_lidar_plausibility_gate_trusts_confident_readings()
{
  // A confident (GOOD/EXCELLENT) return earns a WIDER overshoot allowance than a
  // low-quality one: prior 100cm gives a non-trusted boundary of 300cm and a
  // trusted boundary of 400cm. A moderate overshoot within the trusted allowance
  // is accepted only when the grade is good.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(350, 100, 3)); // GOOD, within trusted allowance
  TEST_ASSERT_FALSE(isLidarReadingImplausible(350, 100, 4)); // EXCELLENT
  TEST_ASSERT_TRUE(isLidarReadingImplausible(350, 100, 1));  // POOR, past non-trusted boundary
  TEST_ASSERT_TRUE(isLidarReadingImplausible(350, 100, 2));  // FAIR
  // Quality is distance-normalized, so a far beam-miss onto a bright background can
  // still grade GOOD/EXCELLENT. A gross overshoot is gated regardless of grade.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(1000, 100, 4)); // EXCELLENT but 10x the prior
  TEST_ASSERT_TRUE(isLidarReadingImplausible(1000, 100, 1)); // POOR
}

void test_lidar_plausibility_trust_boundaries()
{
  // Trusted boundary at prior 100cm is exactly 400cm (strict greater-than).
  TEST_ASSERT_FALSE(isLidarReadingImplausible(400, 100, 4));
  TEST_ASSERT_TRUE(isLidarReadingImplausible(401, 100, 4));
  // At the gate's far edge (prior 300cm) the trusted allowance is 900cm, boundary 1200cm.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(1200, 300, 4));
  TEST_ASSERT_TRUE(isLidarReadingImplausible(1201, 300, 4));
  // Below ~67cm prior the proportional allowance falls under the 200cm floor, so the
  // trust bonus vanishes: a confident return is gated the same as a low-quality one.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(300, 60, 4));
  TEST_ASSERT_TRUE(isLidarReadingImplausible(300, 60, 1));
}

void test_lidar_plausibility_overshoot_scales_with_prior()
{
  // Parallax beam-miss error scales with distance, so the allowed overshoot grows
  // with the prior. Prior 100cm: allowance is the 200cm floor -> boundary 300.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(600, 100, 1));
  // Prior 250cm: allowance scales up (250*1.5=375) -> boundary 625, so 600 is allowed...
  TEST_ASSERT_FALSE(isLidarReadingImplausible(600, 250, 1));
  // ...but 700 past the scaled boundary is still rejected.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(700, 250, 1));
}

void test_lidar_snr_permille_math()
{
  // No ambient baseline -> -1 sentinel (caller skips SNR logic).
  TEST_ASSERT_EQUAL_INT(-1, computeSnrPermille(100, 0));
  // permille = intensity * 1000 / sunlightBase.
  TEST_ASSERT_EQUAL_INT(1000, computeSnrPermille(100, 100));
  TEST_ASSERT_EQUAL_INT(500, computeSnrPermille(50, 100));
}

void test_lidar_plausibility_hold_releases_on_stable_far_readings()
{
  PlausibilityHoldState state = {};
  // First implausible reading: held — one frame, neither consistent nor capped.
  TEST_ASSERT_FALSE(updatePlausibilityHold(state, 500, 30, 2, 3));
  // Second reading agrees within the stable delta (510 vs 500): a deliberate
  // re-aim at a real far subject, so release immediately.
  TEST_ASSERT_TRUE(updatePlausibilityHold(state, 510, 30, 2, 3));
}

void test_lidar_plausibility_hold_caps_noisy_beam_miss()
{
  PlausibilityHoldState state = {};
  // Jumpy beam-miss returns never settle within the stable delta...
  TEST_ASSERT_FALSE(updatePlausibilityHold(state, 500, 30, 2, 3)); // frame 1
  TEST_ASSERT_FALSE(updatePlausibilityHold(state, 800, 30, 2, 3)); // frame 2 (jumped)
  // ...but the absolute fallthrough cap (3 frames) still releases so the
  // readout can never be pinned to a stale value forever.
  TEST_ASSERT_TRUE(updatePlausibilityHold(state, 400, 30, 2, 3));  // frame 3 hits cap
}

void test_lidar_plausibility_hold_reset_restarts_counts()
{
  PlausibilityHoldState state = {};
  updatePlausibilityHold(state, 500, 30, 2, 3);
  updatePlausibilityHold(state, 510, 30, 2, 3); // would have released
  resetPlausibilityHold(state);
  // After a plausible reading resets the hold, a fresh overshoot is held again.
  TEST_ASSERT_FALSE(updatePlausibilityHold(state, 500, 30, 2, 3));
}

void test_lidar_stable_boost_under_min_frames_does_nothing()
{
  // Below the min-frames threshold: confidence unchanged regardless of base.
  TEST_ASSERT_EQUAL_INT(60, applyStableConfidenceBoost(60, 0));
  TEST_ASSERT_EQUAL_INT(60, applyStableConfidenceBoost(60, LIDAR_STABLE_MIN_FRAMES - 1));
}

void test_lidar_stable_boost_kicks_in_at_min_frames()
{
  // At and above the min-frames threshold: confidence rises by LIDAR_STABLE_CONFIDENCE_BOOST.
  TEST_ASSERT_EQUAL_INT(60 + LIDAR_STABLE_CONFIDENCE_BOOST,
                        applyStableConfidenceBoost(60, LIDAR_STABLE_MIN_FRAMES));
  TEST_ASSERT_EQUAL_INT(70 + LIDAR_STABLE_CONFIDENCE_BOOST,
                        applyStableConfidenceBoost(70, LIDAR_STABLE_MIN_FRAMES + 5));
}

void test_lidar_stable_boost_clamps_at_max()
{
  // Boost cannot push past the cap, so a high-base confidence does not become 'excellent' purely from stability.
  TEST_ASSERT_EQUAL_INT(LIDAR_STABLE_MAX_CONFIDENCE,
                        applyStableConfidenceBoost(90, LIDAR_STABLE_MIN_FRAMES));
  TEST_ASSERT_EQUAL_INT(LIDAR_STABLE_MAX_CONFIDENCE,
                        applyStableConfidenceBoost(99, LIDAR_STABLE_MIN_FRAMES));
}

void test_lidar_sunlight_warn_hysteresis()
{
  // Off and below enter: stays off.
  TEST_ASSERT_FALSE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER - 1));
  // Off and at/above enter: switches on.
  TEST_ASSERT_TRUE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER));
  TEST_ASSERT_TRUE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER + 500));
  // On and between exit/enter: holds (the hysteresis band).
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_ENTER - 1));
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT));
  // On and below exit: switches off.
  TEST_ASSERT_FALSE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT - 1));
  // Boundary: exactly enter triggers; exactly exit holds.
  TEST_ASSERT_FALSE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_EXIT)); // off, below enter
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT));   // on, at exit
}

void test_lidar_rejects_invalid_quality_and_zero_intensity()
{
  // Below LIDAR_FUSION_MIN_INTENSITY and fallback floor; quality is INVALID.
  DTSMeasurement measurement = makeLidarMeasurement(1000, 0, DataQuality::INVALID);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);
}

void test_lidar_distance_display_formatting_covers_each_band()
{
  char formattedDistance[16] = {0};

  formatDistanceDisplay(4, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("<15cm", formattedDistance);
  formatDistanceDisplay(75, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("75cm", formattedDistance);
  // Genuine far readings now display up to the sensor's rated 18m, not "Inf." at 10.5m.
  formatDistanceDisplay(1200, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("12.0m", formattedDistance);
  formatDistanceDisplay(1800, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("18.0m", formattedDistance);
  formatDistanceDisplay(1801, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("Inf.", formattedDistance);
  formatDistanceDisplay(2500, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("Inf.", formattedDistance);
  formatDistanceDisplay(150, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("1.50m", formattedDistance); // near-range precision: 2dp below 2m
}

void test_lidar_signal_loss_placeholder_distinguishes_far_dropout()
{
  // Signal lost while tracking a far subject: likely re-aimed at sky, but not a
  // measurement — show "Inf?" so it cannot be mistaken for a genuine reading.
  TEST_ASSERT_EQUAL_STRING("Inf?", lidarSignalLossPlaceholder(LIDAR_FAR_SIGNAL_LOSS_CM, "5.0m"));
  TEST_ASSERT_EQUAL_STRING("Inf?", lidarSignalLossPlaceholder(1200, "5.0m"));
  // Signal lost near, or with no previous reading: plain dropout.
  TEST_ASSERT_EQUAL_STRING("...", lidarSignalLossPlaceholder(LIDAR_FAR_SIGNAL_LOSS_CM - 1, "75cm"));
  TEST_ASSERT_EQUAL_STRING("...", lidarSignalLossPlaceholder(0, "75cm"));
}

void test_lidar_signal_loss_placeholder_sticks_across_prev_distance_reset()
{
  // clearLidarDisplay zeroes prev_distance after the first dropped frame, so the
  // very next frame would otherwise recompute "..." from prev_distance == 0 and
  // the brief "Inf?" would never be seen. Once the display already marks a far
  // dropout, keep marking it regardless of the reset prev_distance.
  TEST_ASSERT_EQUAL_STRING("Inf?", lidarSignalLossPlaceholder(0, "Inf?"));
  // A genuine far measurement (Inf.) that then drops out also stays a far guess.
  TEST_ASSERT_EQUAL_STRING("Inf?", lidarSignalLossPlaceholder(0, "Inf."));
  // A near dropout in progress must not get promoted to a far guess.
  TEST_ASSERT_EQUAL_STRING("...", lidarSignalLossPlaceholder(0, "..."));
  // Standby placeholder is not a far state — wake must start from a clean "...".
  TEST_ASSERT_EQUAL_STRING("...", lidarSignalLossPlaceholder(0, "Zzz"));
  // A null/empty current display falls back to the prev_distance decision.
  TEST_ASSERT_EQUAL_STRING("Inf?", lidarSignalLossPlaceholder(1200, nullptr));
}

void test_lidar_telemetry_age_formatting_covers_each_band()
{
  char ageText[8] = {0};

  // No frame captured yet (timestamp 0 is the "never" sentinel).
  formatLidarTelemetryAge(5000, 0, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("--", ageText);
  // Fresh frame: millisecond resolution.
  formatLidarTelemetryAge(5084, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("84ms", ageText);
  formatLidarTelemetryAge(5999, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("999ms", ageText);
  // Stale frame: seconds with one decimal.
  formatLidarTelemetryAge(6000, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("1.0s", ageText);
  formatLidarTelemetryAge(7500, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("2.5s", ageText);
  // Very stale: capped so the field never overflows the line.
  formatLidarTelemetryAge(104000, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("99.0s", ageText);
  formatLidarTelemetryAge(205000, 5000, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING(">99s", ageText);
  // millis() wrap: 32-bit unsigned subtraction still yields the true age.
  formatLidarTelemetryAge(84, 0xFFFFFFF0UL, ageText, sizeof(ageText));
  TEST_ASSERT_EQUAL_STRING("100ms", ageText);
}

void test_lidar_low_confidence_tracks_beyond_previous_distance()
{
  // 6.0m target with low/harsh-light-like return, but still valid.
  DTSMeasurement measurement = makeLidarMeasurement(6000, 100, DataQuality::POOR);

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
  // 3.2m at exactly the mid-range minimum intensity.
  DTSMeasurement measurement = makeLidarMeasurement(
      3200, LIDAR_FUSION_MIN_INTENSITY_MID, DataQuality::GOOD);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
}

void test_lidar_far_fallback_prevents_dropouts()
{
  // 4.2m, just above the fallback intensity floor, with INVALID quality.
  DTSMeasurement measurement = makeLidarMeasurement(
      4200, LIDAR_FALLBACK_MIN_INTENSITY + 1, DataQuality::INVALID);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 250, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(1, candidate.quality_level);
}

void test_lidar_candidate_fusion_when_returns_agree()
{
  // 300cm primary + 312cm secondary (within fusion-agree delta) — both valid.
  DTSMeasurement measurement = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::GOOD,
      3120, 170, DataQuality::FAIR);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_INT_WITHIN(2, 305, candidate.distance_cm);
  TEST_ASSERT_GREATER_THAN_INT(55, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(3, candidate.quality_level);
}

void test_near_range_correction_is_bounded_below_by_floor()
{
  // At or above the cutoff, the reading passes through unchanged.
  TEST_ASSERT_EQUAL_INT(150, applyLidarCalibrationCm(150));
  TEST_ASSERT_EQUAL_INT(200, applyLidarCalibrationCm(200));

  // The measured anchor (raw 130 -> ~100cm) is preserved; the floor does not clip it.
  TEST_ASSERT_INT_WITHIN(1, 100, applyLidarCalibrationCm(130));

  // Below the anchor the raw power law collapses (raw 65 -> ~14, raw 50 -> ~7),
  // dropping sub-metre subjects below the display floor. The bound holds them at
  // LIDAR_CAL_MIN_OUTPUT_PCT percent of the raw distance instead.
  TEST_ASSERT_EQUAL_INT(65 * LIDAR_CAL_MIN_OUTPUT_PCT / 100, applyLidarCalibrationCm(65));
  TEST_ASSERT_EQUAL_INT(50 * LIDAR_CAL_MIN_OUTPUT_PCT / 100, applyLidarCalibrationCm(50));
  TEST_ASSERT_EQUAL_INT(100 * LIDAR_CAL_MIN_OUTPUT_PCT / 100, applyLidarCalibrationCm(100));

  // The correction only ever pulls a reading down, never up.
  TEST_ASSERT_LESS_OR_EQUAL_INT(120, applyLidarCalibrationCm(120));
}

void test_lidar_mm_to_cm_conversion_rounds_to_nearest()
{
  // Truncation biased every reading 0-9 mm short (mean -4.5 mm). 1996 mm is
  // 199.6 cm and must read 200, not 199.
  DTSMeasurement measurement = makeLidarMeasurement(1996, 200, DataQuality::GOOD);
  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_EQUAL_INT(200, candidate.distance_cm);

  // Below the .5 boundary still rounds down.
  measurement = makeLidarMeasurement(1994, 200, DataQuality::GOOD);
  candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_EQUAL_INT(199, candidate.distance_cm);
}

void test_near_range_correction_compensates_for_offset_pref_delta()
{
  // The 130->100 anchor was measured with the default geometry offset (400 mm,
  // applied library-side before this correction sees the value). When the user
  // changes the offset pref by delta, the same physical subject arrives
  // delta cm higher/lower, so the correction must be evaluated in the
  // default-offset frame and the delta re-added afterwards. Without this, any
  // offset change silently invalidates the measured anchor.
  const int delta_up = 10;   // user offset 500 mm
  const int delta_down = -5; // user offset 350 mm

  // The anchor subject: default-offset reading 130 -> ~100. With the pref
  // moved, the same subject reads 130+delta and must correct to ~100+delta.
  TEST_ASSERT_INT_WITHIN(1, 100 + delta_up, applyLidarCalibrationCm(130 + delta_up, delta_up));
  TEST_ASSERT_INT_WITHIN(1, 100 + delta_down, applyLidarCalibrationCm(130 + delta_down, delta_down));

  // The cutoff moves with the delta too: a subject at the cutoff passes
  // through unchanged regardless of the configured offset.
  TEST_ASSERT_EQUAL_INT(150 + delta_up, applyLidarCalibrationCm(150 + delta_up, delta_up));

  // Zero delta stays byte-for-byte compatible with the single-argument form.
  TEST_ASSERT_EQUAL_INT(applyLidarCalibrationCm(130), applyLidarCalibrationCm(130, 0));
}

void test_lidar_fusion_takes_max_confidence_not_average()
{
  // A strong primary must not be demoted by a weak but agreeing secondary:
  // fusion keeps the higher confidence (plus the agreement bonus), it does not
  // average the two down toward the weak candidate.
  DTSMeasurement primaryOnly = makeLidarMeasurement(3000, 200, DataQuality::EXCELLENT);
  LidarCandidate strong = chooseBestLidarCandidate(primaryOnly, 0, false, 0);
  TEST_ASSERT_TRUE(strong.valid);

  DTSMeasurement dual = makeLidarDualPeakMeasurement(
      3000, 200, DataQuality::EXCELLENT,
      3200, 12, DataQuality::POOR);
  LidarCandidate fused = chooseBestLidarCandidate(dual, 0, false, 0);
  TEST_ASSERT_TRUE(fused.valid);

  // Averaging would drag confidence below the strong primary; max() keeps it at
  // or above (agreement only adds a bonus).
  TEST_ASSERT_GREATER_OR_EQUAL_INT(strong.confidence, fused.confidence);
}

void test_lidar_lens_prior_is_range_weighted()
{
  DTSMeasurement nearMeasurement = makeLidarMeasurement(2000, 200, DataQuality::GOOD);

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
  // 450cm subject under low ambient IR; sunlightBase is set below.
  DTSMeasurement lowAmbientMeasurement = makeLidarMeasurement(4500, 40, DataQuality::GOOD);
  lowAmbientMeasurement.sunlightBase = 25;

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
  // 700cm at the far-range minimum intensity, under heavy ambient IR.
  DTSMeasurement measurement = makeLidarMeasurement(
      7000, LIDAR_FUSION_MIN_INTENSITY_FAR, DataQuality::GOOD);
  measurement.sunlightBase = 1500;

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

  // Reciprocal-distance interpolation between the 1.2m (200) and 1.5m (300)
  // marks: sensor 250 -> 1/(1/1.2 + 0.5*(1/1.5 - 1/1.2)) = 1.333m.
  LensDistanceEstimate interpolated = estimateLensDistance(lens, 250);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_FALSE(interpolated.is_infinity);
  TEST_ASSERT_EQUAL_INT(133, interpolated.distance_cm);

  LensDistanceEstimate infinity = estimateLensDistance(lens, 706);
  TEST_ASSERT_TRUE(infinity.valid);
  TEST_ASSERT_TRUE(infinity.is_infinity);
  TEST_ASSERT_EQUAL_INT(LENS_INFINITY_RAW, infinity.distance_cm);
}

void test_lens_logic_ignores_unused_distance_markers()
{
  Lens lens = {
      15056,
      "150/5.6",
      150.0f,
      {100, 200, 300, 400, 500, 0, 0},
      {2.0f, 2.5f, 3.0f, 5.0f, 10.0f, 0.0f, 0.0f},
      {5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f, 45.0f, 0.0f, 0.0f},
      {0, 0, 0, 0},
      true};

  TEST_ASSERT_EQUAL_INT(5, getLensDistancePointCount(lens));
  TEST_ASSERT_EQUAL_INT(2, findLensSnapIndex(lens, 301));

  // Reciprocal-distance interpolation between the 2.5m (200) and 3.0m (300)
  // marks: sensor 250 -> 1/(1/2.5 + 0.5*(1/3 - 1/2.5)) = 2.7272m -> 273 cm
  // rounded to nearest (truncation used to pin this at 272).
  LensDistanceEstimate interpolated = estimateLensDistance(lens, 250);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_FALSE(interpolated.is_infinity);
  TEST_ASSERT_EQUAL_INT(273, interpolated.distance_cm);

  LensDistanceEstimate infinity = estimateLensDistance(lens, 506);
  TEST_ASSERT_TRUE(infinity.valid);
  TEST_ASSERT_TRUE(infinity.is_infinity);
  TEST_ASSERT_EQUAL_INT(LENS_INFINITY_RAW, infinity.distance_cm);
}

void test_far_snap_deadzone_is_distance_relative_at_sparse_marks()
{
  // Sparse far span: 5m -> 10m over only 18 ADC counts. The fixed +-3 count
  // far deadzone used to display the mark distance across a third of that
  // span (reading 415 interpolates to 8.57m but snapped to "10.0m", a 14%
  // error that also fed the LiDAR plausibility prior). Snapping must only
  // engage when the interpolated distance is actually near the mark.
  Lens lens = {
      998,
      "SPARSE",
      100.0f,
      {100, 200, 300, 400, 418, 0, 0},
      {2.0f, 2.5f, 3.0f, 5.0f, 10.0f, 0.0f, 0.0f},
      {0.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f},
      {0, 0, 0, 0},
      true};

  // 3 counts from the 10m mark but 14% away in distance: no snap.
  TEST_ASSERT_EQUAL_INT(-1, findLensSnapIndex(lens, 415));

  // 1 count away interpolates to 9.47m (5.3% error): snap engages.
  TEST_ASSERT_EQUAL_INT(4, findLensSnapIndex(lens, 417));

  // Exact mark always snaps.
  TEST_ASSERT_EQUAL_INT(4, findLensSnapIndex(lens, 418));

  // Dense far span (100 counts from 5m to 10m on the shipped-style table):
  // 3 counts is only ~3% in distance, so the deadzone behaves as before.
  Lens dense = makeTestLens();
  TEST_ASSERT_EQUAL_INT(6, findLensSnapIndex(dense, 697));
}

void test_shipped_calibrated_lens_readings_are_ascending()
{
  // Convention: sensor_reading[] must ascend with focus distance so
  // estimateLensDistance() (which assumes an ascending ADC axis) interpolates
  // between marks instead of collapsing to the near clamp / infinity. A
  // descending table was the v10.5.x default-65mm focus bug.
  for (size_t i = 0; i < NUM_LENSES; i++)
  {
    const Lens &lens = lenses[i];
    if (!lens.calibrated)
    {
      continue;
    }
    int point_count = getLensDistancePointCount(lens);
    for (int p = 1; p < point_count; p++)
    {
      TEST_ASSERT_TRUE_MESSAGE(lens.sensor_reading[p] > lens.sensor_reading[p - 1], lens.name);
    }
  }
}

void test_default_lens_estimate_interpolates_between_marks()
{
  // Regression for the descending-table bug: the shipped default lens must read
  // back a monotonic focus distance across its range, not snap to 1m / Inf.
  const Lens &lens = lenses[DEFAULT_SELECTED_LENS];
  TEST_ASSERT_TRUE(lens.calibrated);
  int point_count = getLensDistancePointCount(lens);
  TEST_ASSERT_GREATER_THAN_INT(2, point_count);

  // Each calibrated mark reads back its own distance.
  for (int p = 0; p < point_count; p++)
  {
    LensDistanceEstimate at_mark = estimateLensDistance(lens, lens.sensor_reading[p]);
    TEST_ASSERT_TRUE(at_mark.valid);
    TEST_ASSERT_FALSE(at_mark.is_infinity);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(lens.distance[p] * CM_PER_METER), at_mark.distance_cm);
  }

  // A reading halfway between the two nearest-focus marks interpolates strictly
  // between them (the descending table returned distance[0] exactly here).
  int mid_reading = (lens.sensor_reading[0] + lens.sensor_reading[1]) / 2;
  LensDistanceEstimate mid = estimateLensDistance(lens, mid_reading);
  TEST_ASSERT_TRUE(mid.valid);
  TEST_ASSERT_FALSE(mid.is_infinity);
  TEST_ASSERT_GREATER_THAN_INT(static_cast<int>(lens.distance[0] * CM_PER_METER), mid.distance_cm);
  TEST_ASSERT_LESS_THAN_INT(static_cast<int>(lens.distance[1] * CM_PER_METER), mid.distance_cm);
}

void test_150mm_profile_uses_custom_distance_scale()
{
  const Lens *lens150 = findLensById(15056);
  TEST_ASSERT_NOT_NULL(lens150);
  TEST_ASSERT_EQUAL_INT(5, getLensDistancePointCount(*lens150));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, lens150->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.5f, lens150->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, lens150->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens150->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens150->distance[4]);
}

void test_250mm_profiles_use_custom_distance_scales()
{
  const Lens *lens250f5 = findLensById(25005);
  const Lens *lens250f8 = findLensById(25008);
  TEST_ASSERT_NOT_NULL(lens250f5);
  TEST_ASSERT_NOT_NULL(lens250f8);

  TEST_ASSERT_EQUAL_INT(10, getLensDistancePointCount(*lens250f5));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.5f, lens250f5->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.0f, lens250f5->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens250f5->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, lens250f5->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 8.0f, lens250f5->distance[4]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens250f5->distance[5]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 15.0f, lens250f5->distance[6]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, lens250f5->distance[7]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.0f, lens250f5->distance[8]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.0f, lens250f5->distance[9]);

  TEST_ASSERT_EQUAL_INT(9, getLensDistancePointCount(*lens250f8));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.5f, lens250f8->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.0f, lens250f8->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens250f8->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, lens250f8->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens250f8->distance[4]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 15.0f, lens250f8->distance[5]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, lens250f8->distance[6]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.0f, lens250f8->distance[7]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.0f, lens250f8->distance[8]);
}

void test_calibration_median_spread_tolerates_gentle_drift()
{
  // 8 samples drifting gently upward: total spread 14 exceeds the old
  // min-to-max limit of 10, but max deviation from median (504) is only 8,
  // which the median-based check should accept.
  const int driftSamples[8] = {500, 502, 504, 504, 506, 506, 508, 510};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(driftSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_TRUE(stable);
  TEST_ASSERT_INT_WITHIN(3, 505, averagedReading);
}

void test_calibration_rejects_truly_unstable_readings()
{
  // Max deviation from median exceeds the spread limit.
  const int wildSamples[8] = {300, 310, 320, 300, 310, 320, 300, 310};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(wildSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_FALSE(stable);
}

void test_lightmeter_dark_bright_fraction_and_seconds()
{
  char formattedShutter[16] = {0};
  formatShutterSpeed(0.0f, 8.0f, 400, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("Dark!", formattedShutter);
  formatShutterSpeed(50000.0f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("Bright!", formattedShutter);
  // K=12.5: speed = 64*12.5/(320*400) = 0.00625 s. Nearest standard speed in
  // log space: 0.36 stop from 1/125 vs 0.64 stop from 1/250 → display 1/125.
  // (The old bucket bounds floored this to 1/250, a 0..-1 stop display bias.)
  formatShutterSpeed(320.0f, 8.0f, 400, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1/125 sec.", formattedShutter);
  // speed = 64*12.5/(400*400) = 0.005 s, below the 1/250|1/125 geometric
  // boundary at 0.005657 s → still 1/250.
  formatShutterSpeed(400.0f, 8.0f, 400, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1/250 sec.", formattedShutter);
  // speed = 4*12.5/(1.25*100) = 0.4 s → nearest standard is 1/2.
  formatShutterSpeed(1.25f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1/2 sec.", formattedShutter);
  // speed = 4*12.5/(666.7*100) = 0.00075 s: within half a stop of 1/1000, so
  // show 1/1000 rather than Bright!.
  formatShutterSpeed(666.7f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1/1000 sec.", formattedShutter);
  // speed = 4*12.5/(833.3*100) = 0.0006 s: more than half a stop past 1/1000.
  formatShutterSpeed(833.3f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("Bright!", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.5*50) = 2.0s → 2 sec
  formatShutterSpeed(0.5f, 2.0f, 50, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("2 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.4*100) = 1.25s → rounds to 1.5 sec
  formatShutterSpeed(0.4f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1.5 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.5*100) = 1.0s → 1 sec
  formatShutterSpeed(0.5f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.001*100) = 500s → 8m20s
  formatShutterSpeed(0.001f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("8m20s", formattedShutter);
  // absurdly dark → capped at 25m0s
  formatShutterSpeed(0.00001f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("25m0s", formattedShutter);
}

void test_frameline_scaling_returns_base_when_format_mm_invalid()
{
  // Zero or negative format dimensions short-circuit to the base box (still clamped).
  FramelineDimensions r = scaleFramelineToFormat(100, 60, 0.0f, 70.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(100, r.width);
  TEST_ASSERT_EQUAL_INT(60, r.height);

  r = scaleFramelineToFormat(100, 60, 60.0f, -1.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(100, r.width);
  TEST_ASSERT_EQUAL_INT(60, r.height);
}

void test_frameline_scaling_height_constrained_for_taller_format()
{
  // 6x7 base (60mm x 70mm, ratio 0.857) into a base pixel box that is wider
  // than its own ratio (100x60 -> 1.667). formatRatio (0.857) < baseRatio
  // (1.667), so height stays at baseHeight and width scales down.
  // Expected width = round(60 * 0.857) = 51.
  FramelineDimensions r = scaleFramelineToFormat(100, 60, 60.0f, 70.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(51, r.width);
  TEST_ASSERT_EQUAL_INT(60, r.height);
}

void test_frameline_scaling_width_constrained_for_wider_pixel_ratio()
{
  // A current format wider than the pixel box but NOT wider than the base
  // format (so overflow is not allowed). 6x4.5 (60x45, ratio 1.333) vs base
  // 6x7 (60x70, ratio 0.857) — formatRatio > baseFormatRatio, but
  // formatHeightMm (45) < baseFormatHeightMm (70), so allowOverflow is false.
  // formatRatio (1.333) >= base pixel ratio (1.667)? No, 1.333 < 1.667 →
  // height-constrained: width = round(60 * 1.333) = 80, height = 60.
  FramelineDimensions r = scaleFramelineToFormat(100, 60, 60.0f, 45.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(80, r.width);
  TEST_ASSERT_EQUAL_INT(60, r.height);
}

void test_frameline_scaling_allows_overflow_when_format_is_wider_and_taller()
{
  // 6x9 (60x90, ratio 0.667 — wait, let's pick a truly wider format).
  // Panoramic 6x12 (60x120, ratio 0.5) vs base 6x7 (60x70, ratio 0.857):
  // formatRatio (0.5) < baseFormatRatio (0.857), so overflow NOT allowed.
  // To exercise overflow we need a format both wider in ratio AND at least
  // as tall in mm. 6x9 vs 6x4.5 with 6x4.5 as base would do — but base is
  // fixed to DEFAULT_SELECTED_FORMAT in the wrapper, so we use whatever
  // satisfies the rule here.
  // Use synthetic dimensions: format 90x60 (ratio 1.5) vs base 60x70
  // (ratio 0.857). formatRatio > baseRatio AND formatHeightMm (60) is not
  // >= baseFormatHeightMm (70) — so overflow still NOT allowed. Bump
  // formatHeightMm to 70 and it does qualify: 90x70 (ratio 1.286) vs
  // base 60x70 (ratio 0.857) — overflow allowed.
  // Pixel box: 100x60. Expected: height=60, width=round(60*1.286)=77.
  FramelineDimensions r = scaleFramelineToFormat(100, 60, 90.0f, 70.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(77, r.width);
  TEST_ASSERT_EQUAL_INT(60, r.height);
  // Overflow allows width to exceed baseWidth.
  FramelineDimensions r2 = scaleFramelineToFormat(50, 60, 90.0f, 70.0f, 60.0f, 70.0f);
  TEST_ASSERT_EQUAL_INT(77, r2.width); // not clamped to 50
  TEST_ASSERT_EQUAL_INT(60, r2.height);
}

void test_frameline_scaling_clamps_dimensions_to_minimum_one()
{
  // Extreme ratio that would round to zero is clamped to 1.
  FramelineDimensions r = scaleFramelineToFormat(1, 100, 1.0f, 10000.0f, 60.0f, 70.0f);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(1, r.width);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(1, r.height);
}

void test_6x9_and_9x3_share_corrected_sensor_table()
{
  // Regression guard for v10.4.6. 9x3 shared its physical advance pattern
  // with 6x9 on 120 film but was using a slightly out-of-tolerance sensor
  // array (1-2 ticks more per frame). The fix aligned 9x3 onto 6x9's
  // corrected spacing; a later refactor (commit b5f10ad) deduplicated the
  // two via a shared macro. This test pins the corrected values directly
  // so that a future edit can't silently restore the drift.
  const FilmFormat *fmt_6x9 = findFormatByName("6x9");
  const FilmFormat *fmt_9x3 = findFormatByName("9x3");
  TEST_ASSERT_NOT_NULL(fmt_6x9);
  TEST_ASSERT_NOT_NULL(fmt_9x3);

  const int expected_sensor[] = {0, 142, 184, 223, 260, 295, 329, 360, 390, 550};
  const int expected_count = sizeof(expected_sensor) / sizeof(expected_sensor[0]);

  for (int i = 0; i < expected_count; i++)
  {
    TEST_ASSERT_EQUAL_INT(expected_sensor[i], fmt_6x9->sensor[i]);
    TEST_ASSERT_EQUAL_INT(expected_sensor[i], fmt_9x3->sensor[i]);
  }
  // Sanity: the two formats must remain in lock-step on physical advance.
  for (int i = 0; i < expected_count; i++)
  {
    TEST_ASSERT_EQUAL_INT(fmt_6x9->sensor[i], fmt_9x3->sensor[i]);
  }
}

void test_lightmeter_scale_split_preserves_effective_exposure_constant()
{
  // Regression guard for v10.4.6 metering fix. The original bug came from
  // setting K to the ISO-standard 12.5 without re-introducing the 1.77x
  // BH1750 mounting compensation that the previous K=20 had absorbed; the
  // result was ~1.5 stops of overexposure. The fix splits the two factors:
  // K is now the standard and LIGHTMETER_LUX_CAL_SCALE applies the mounting
  // factor at the sensor read site. Removing either factor regresses the
  // exposure by a stop and a half. Pin the product so a future tweak of
  // either constant has to be deliberate.
  const float effective = K * LIGHTMETER_LUX_CAL_SCALE;
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.125f, effective);

  // formatShutterSpeed uses K directly. Pinning a known (lux, aperture, iso)
  // triple to its expected shutter band catches a stealth change to K alone
  // (e.g. reverting it to 20) that the product test above would also flag,
  // but a downstream reader will see the effect on actual exposure here.
  char shutter[16] = {0};
  // (8^2 * 12.5) / (100 * 100) = 800/10000 = 0.08 s -> falls in the 1/15 band.
  // If K reverted to the old 20, the same scene would land in the 1/8 band,
  // failing the assert and flagging the 1.5-stop drift.
  formatShutterSpeed(100.0f, 8.0f, 100, shutter, sizeof(shutter));
  TEST_ASSERT_EQUAL_STRING("1/15 sec.", shutter);
}

void test_lens_moving_average_primes_with_first_sample()
{
  // The old zero-filled window made the first smoothed reading sample/13 and
  // ramped up over 13 polls; combined with the spike filter this pinned a
  // bogus near-zero lens reading (and a wrong 100 cm LiDAR prior) for the
  // first ~15 polls after boot. Priming fills the window with the first real
  // sample instead.
  LensMovingAverageState state = {};
  TEST_ASSERT_EQUAL_INT(300, updateLensMovingAverage(state, 300));

  // Window is fully primed: a step to 313 moves the average by 1 (13/13).
  TEST_ASSERT_EQUAL_INT(301, updateLensMovingAverage(state, 313));

  // Reset re-arms priming.
  resetLensMovingAverageState(state);
  TEST_ASSERT_EQUAL_INT(500, updateLensMovingAverage(state, 500));

  // Composed with the spike filter: the first filtered boot reading is the
  // real ADC value, not a warm-up artefact.
  LensMovingAverageState bootAvg = {};
  LensSpikeFilterState bootSpike = {};
  int firstFiltered = updateLensSpikeFilter(bootSpike, updateLensMovingAverage(bootAvg, 300));
  TEST_ASSERT_EQUAL_INT(300, firstFiltered);
}

void test_lens_spike_filter_initialises_and_accepts_small_movement()
{
  LensSpikeFilterState state = {};

  // First call seeds the stable reading.
  TEST_ASSERT_EQUAL_INT(1000, updateLensSpikeFilter(state, 1000));
  TEST_ASSERT_TRUE(state.initialized);

  // A reading within +/-LENS_SPIKE_DELTA_THRESHOLD (8) is accepted immediately
  // as the new stable value.
  TEST_ASSERT_EQUAL_INT(1007, updateLensSpikeFilter(state, 1007));
  TEST_ASSERT_EQUAL_INT(1007, state.stableReading);
  TEST_ASSERT_EQUAL_UINT8(0, state.pendingCount);
}

void test_lens_spike_filter_rejects_lone_spike_then_promotes_consistent_pending()
{
  LensSpikeFilterState state = {};
  updateLensSpikeFilter(state, 1000); // seed

  // A single jump well outside the threshold does not move stable.
  TEST_ASSERT_EQUAL_INT(1000, updateLensSpikeFilter(state, 1100));
  TEST_ASSERT_EQUAL_INT(1000, state.stableReading);
  TEST_ASSERT_EQUAL_UINT8(1, state.pendingCount);

  // A consistent second sample (within threshold of pending) promotes it.
  // LENS_SPIKE_CONFIRMATION_COUNT defaults to 2.
  TEST_ASSERT_EQUAL_INT(1102, updateLensSpikeFilter(state, 1102));
  TEST_ASSERT_EQUAL_INT(1102, state.stableReading);
  TEST_ASSERT_EQUAL_UINT8(0, state.pendingCount);
}

void test_lens_spike_filter_drifting_pending_resets_count()
{
  LensSpikeFilterState state = {};
  updateLensSpikeFilter(state, 1000); // seed

  // First spike: stable unchanged, pendingCount=1.
  updateLensSpikeFilter(state, 1100);
  TEST_ASSERT_EQUAL_UINT8(1, state.pendingCount);

  // Second sample wanders well off the pending value (delta > threshold):
  // pending is restarted, count goes back to 1, stable still unchanged.
  TEST_ASSERT_EQUAL_INT(1000, updateLensSpikeFilter(state, 1200));
  TEST_ASSERT_EQUAL_INT(1000, state.stableReading);
  TEST_ASSERT_EQUAL_UINT8(1, state.pendingCount);
  TEST_ASSERT_EQUAL_INT(1200, state.pendingReading);
}

void test_cm_to_readable_renders_cm_below_one_metre_and_metres_above()
{
  char out[16] = {0};

  cmToReadable(0, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("0cm", out);

  cmToReadable(75, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("75cm", out);

  cmToReadable(99, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("99cm", out);

  // 100cm is the m/cm boundary — first value rendered in metres.
  cmToReadable(100, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("1.00m", out);

  cmToReadable(150, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("1.50m", out);

  cmToReadable(1234, 1, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("12.3m", out);

  // Zero buffer size must not write or read past the buffer.
  char guard[4] = {'X', 'X', 'X', 'X'};
  cmToReadable(42, 2, guard, 0);
  TEST_ASSERT_EQUAL_CHAR('X', guard[0]);
}

void test_lidar_secondary_quality_inherits_primary_when_invalid()
{
  // chooseBestLidarCandidate remaps a secondary peak's INVALID quality onto
  // the primary's quality before scoring. Verify that a peak with explicit
  // EXCELLENT and a peak whose quality remaps to EXCELLENT produce the same
  // fused candidate.
  DTSMeasurement explicitExcellent = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::EXCELLENT,
      3120, 170, DataQuality::EXCELLENT);
  DTSMeasurement inheritsFromPrimary = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::EXCELLENT,
      3120, 170, DataQuality::INVALID);

  LidarCandidate explicitCandidate = chooseBestLidarCandidate(explicitExcellent, 0, false, 0);
  LidarCandidate inheritedCandidate = chooseBestLidarCandidate(inheritsFromPrimary, 0, false, 0);

  TEST_ASSERT_TRUE(explicitCandidate.valid);
  TEST_ASSERT_TRUE(inheritedCandidate.valid);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.distance_cm, inheritedCandidate.distance_cm);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.confidence, inheritedCandidate.confidence);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.quality_level, inheritedCandidate.quality_level);
}

void test_ui_signature_hash_primitives_are_deterministic_and_distinct()
{
  // Determinism: same inputs produce same output.
  TEST_ASSERT_EQUAL_UINT32(hashInt(HASH_OFFSET_BASIS, 42), hashInt(HASH_OFFSET_BASIS, 42));
  TEST_ASSERT_EQUAL_UINT32(hashBool(HASH_OFFSET_BASIS, true), hashBool(HASH_OFFSET_BASIS, true));
  TEST_ASSERT_EQUAL_UINT32(hashFloat(HASH_OFFSET_BASIS, 1.5f), hashFloat(HASH_OFFSET_BASIS, 1.5f));
  TEST_ASSERT_EQUAL_UINT32(hashCString(HASH_OFFSET_BASIS, "abc"), hashCString(HASH_OFFSET_BASIS, "abc"));

  // Distinctness: any change in input changes the hash.
  TEST_ASSERT_NOT_EQUAL(hashInt(HASH_OFFSET_BASIS, 0), hashInt(HASH_OFFSET_BASIS, 1));
  TEST_ASSERT_NOT_EQUAL(hashBool(HASH_OFFSET_BASIS, false), hashBool(HASH_OFFSET_BASIS, true));
  TEST_ASSERT_NOT_EQUAL(hashFloat(HASH_OFFSET_BASIS, 1.0f), hashFloat(HASH_OFFSET_BASIS, 1.0000001f));
  TEST_ASSERT_NOT_EQUAL(hashCString(HASH_OFFSET_BASIS, "ab"), hashCString(HASH_OFFSET_BASIS, "ba"));

  // Length is part of the hash, so "ab"+"c" and "a"+"bc" do not collide.
  uint32_t ab_then_c = hashCString(hashCString(HASH_OFFSET_BASIS, "ab"), "c");
  uint32_t a_then_bc = hashCString(hashCString(HASH_OFFSET_BASIS, "a"), "bc");
  TEST_ASSERT_NOT_EQUAL(ab_then_c, a_then_bc);
}

void test_ui_signature_hash_cstring_treats_null_as_empty()
{
  TEST_ASSERT_EQUAL_UINT32(hashCString(HASH_OFFSET_BASIS, nullptr),
                           hashCString(HASH_OFFSET_BASIS, ""));
}

namespace
{
MainUiSnapshot baselineMainSnapshot()
{
  MainUiSnapshot s = {};
  s.ui_mode = 0;
  s.selected_lens = 2;
  s.selected_format = 1;
  s.iso = 400;
  s.aperture = 8.0f;
  s.show_ev_readout = false;
  s.ev_readout = 11.0f;
  s.shutter_speed = "1/125 sec.";
  s.distance_cm = "123";
  s.lens_distance_cm = "456";
  s.distance = 123;
  s.lens_distance_raw = 456;
  s.lidar_quality_level = 3;
  s.lidar_high_sunlight = false;
  s.parallaxEnabled = true;
  s.reticle_offset_x = 0;
  s.reticle_offset_y = 0;
  s.show_horizon_line = true;
  return s;
}

struct MainUiFieldMutation
{
  const char *name;
  void (*apply)(MainUiSnapshot &);
};

// One entry per MainUiSnapshot field. A field missing from the hash leaves
// the main shooting screen showing stale values (the shipped v10.6.x menu
// bug class), so every field must flip the signature.
const MainUiFieldMutation MAIN_UI_FIELD_MUTATIONS[] = {
    {"ui_mode", [](MainUiSnapshot &s) { s.ui_mode = 5; }},
    {"selected_lens", [](MainUiSnapshot &s) { s.selected_lens = 9; }},
    {"selected_format", [](MainUiSnapshot &s) { s.selected_format = 9; }},
    {"iso", [](MainUiSnapshot &s) { s.iso = 800; }},
    {"aperture", [](MainUiSnapshot &s) { s.aperture = 11.0f; }},
    {"show_ev_readout", [](MainUiSnapshot &s) { s.show_ev_readout = true; }},
    {"ev_readout", [](MainUiSnapshot &s) { s.ev_readout = 12.5f; }},
    {"shutter_speed", [](MainUiSnapshot &s) { s.shutter_speed = "1/250 sec."; }},
    {"distance_cm", [](MainUiSnapshot &s) { s.distance_cm = "999"; }},
    {"lens_distance_cm", [](MainUiSnapshot &s) { s.lens_distance_cm = "999"; }},
    {"distance", [](MainUiSnapshot &s) { s.distance = 999; }},
    {"lens_distance_raw", [](MainUiSnapshot &s) { s.lens_distance_raw = 999; }},
    {"lidar_quality_level", [](MainUiSnapshot &s) { s.lidar_quality_level = 1; }},
    {"lidar_high_sunlight", [](MainUiSnapshot &s) { s.lidar_high_sunlight = true; }},
    {"parallaxEnabled", [](MainUiSnapshot &s) { s.parallaxEnabled = false; }},
    {"reticle_offset_x", [](MainUiSnapshot &s) { s.reticle_offset_x = 4; }},
    {"reticle_offset_y", [](MainUiSnapshot &s) { s.reticle_offset_y = -4; }},
    {"show_horizon_line", [](MainUiSnapshot &s) { s.show_horizon_line = false; }},
};

ExternalUiSnapshot baselineExternalSnapshot()
{
  return {/* selected_format */ 1,
          /* selected_lens   */ 2,
          /* bat_per         */ 75,
          /* film_counter    */ 5,
          /* frame_progress  */ 0.25f,
          /* sleepMode       */ false};
}

MenuUiSnapshot baselineMenuSnapshot()
{
  return {/* ui_mode                          */ 1,
          /* config_step                      */ 2,
          /* calib_step                       */ 0,
          /* selected_lens                    */ 3,
          /* selected_format                  */ 1,
          /* calib_lens                       */ 0,
          /* current_calib_distance           */ 0,
          /* calib_capture_status             */ 0,
          /* calib_capture_status_ms          */ 0ul,
          /* lens_sensor_reading              */ 200,
          /* lens_focus_offset                */ 0,
          /* iso                              */ 400,
          /* aperture                         */ 8.0f,
          /* exposure_comp_thirds             */ 0,
          /* meter_smoothing_mode             */ 1,
          /* show_ev_readout                  */ false,
          /* parallaxEnabled                  */ true,
          /* sleep_timeout_mode               */ 1,
          /* lidar_idle_timeout_mode          */ 1,
          /* lidar_distance_offset_mm         */ 400,
          /* level_trim_landscape_deci_deg    */ 0,
          /* level_trim_portrait_pos_deci_deg */ 0,
          /* level_trim_portrait_neg_deci_deg */ 0,
          /* reticle_offset_x                 */ 0,
          /* reticle_offset_y                 */ 0,
          /* reticle_adjust_step              */ 0,
          /* brightness_auto                  */ true,
          /* brightness_manual_pct            */ 50,
          /* brightness_auto_top_pct          */ 100,
          /* show_horizon_line                */ true,
          /* frame_one_offset                 */ 0,
          /* frame_spacing_offset             */ 0,
          /* film_counter                     */ 5,
          /* last_lidar_error_code            */ 0,
          /* lidar_recovery_count             */ 0,
          /* lidarEnabled                     */ true,
          /* adsReady                         */ true,
          /* mpuReady                         */ true,
          /* mainDisplayReady                 */ true,
          /* externalDisplayReady             */ true,
          /* batteryGaugeReady                */ true,
          /* lightMeterReady                  */ true,
          /* statusPixelReady                 */ true,
          /* encoderReady                     */ true,
          /* lidarSensorReady                 */ true,
          /* prefsSchemaValid                 */ true,
          /* prefsLoadedLegacy                */ false,
          /* prefsSchemaVersionLoaded         */ 2};
}

struct MenuUiFieldMutation
{
  const char *name;
  void (*apply)(MenuUiSnapshot &);
};

// One entry per MenuUiSnapshot field, same guard as the Main table.
const MenuUiFieldMutation MENU_UI_FIELD_MUTATIONS[] = {
    {"ui_mode", [](MenuUiSnapshot &s) { s.ui_mode = 4; }},
    {"config_step", [](MenuUiSnapshot &s) { s.config_step = 7; }},
    {"calib_step", [](MenuUiSnapshot &s) { s.calib_step = 1; }},
    {"selected_lens", [](MenuUiSnapshot &s) { s.selected_lens = 9; }},
    {"selected_format", [](MenuUiSnapshot &s) { s.selected_format = 9; }},
    {"calib_lens", [](MenuUiSnapshot &s) { s.calib_lens = 2; }},
    {"current_calib_distance", [](MenuUiSnapshot &s) { s.current_calib_distance = 3; }},
    {"calib_capture_status", [](MenuUiSnapshot &s) { s.calib_capture_status = 1; }},
    {"calib_capture_status_ms", [](MenuUiSnapshot &s) { s.calib_capture_status_ms = 12345ul; }},
    {"lens_sensor_reading", [](MenuUiSnapshot &s) { s.lens_sensor_reading = 999; }},
    {"lens_focus_offset", [](MenuUiSnapshot &s) { s.lens_focus_offset = -5; }},
    {"iso", [](MenuUiSnapshot &s) { s.iso = 800; }},
    {"aperture", [](MenuUiSnapshot &s) { s.aperture = 11.0f; }},
    {"exposure_comp_thirds", [](MenuUiSnapshot &s) { s.exposure_comp_thirds = 3; }},
    {"meter_smoothing_mode", [](MenuUiSnapshot &s) { s.meter_smoothing_mode = 2; }},
    {"show_ev_readout", [](MenuUiSnapshot &s) { s.show_ev_readout = true; }},
    {"parallaxEnabled", [](MenuUiSnapshot &s) { s.parallaxEnabled = false; }},
    {"sleep_timeout_mode", [](MenuUiSnapshot &s) { s.sleep_timeout_mode = 3; }},
    {"lidar_idle_timeout_mode", [](MenuUiSnapshot &s) { s.lidar_idle_timeout_mode = 2; }},
    {"lidar_distance_offset_mm", [](MenuUiSnapshot &s) { s.lidar_distance_offset_mm = 500; }},
    {"level_trim_landscape_deci_deg", [](MenuUiSnapshot &s) { s.level_trim_landscape_deci_deg = 15; }},
    {"level_trim_portrait_pos_deci_deg", [](MenuUiSnapshot &s) { s.level_trim_portrait_pos_deci_deg = 15; }},
    {"level_trim_portrait_neg_deci_deg", [](MenuUiSnapshot &s) { s.level_trim_portrait_neg_deci_deg = -15; }},
    {"reticle_offset_x", [](MenuUiSnapshot &s) { s.reticle_offset_x = 4; }},
    {"reticle_offset_y", [](MenuUiSnapshot &s) { s.reticle_offset_y = -4; }},
    {"reticle_adjust_step", [](MenuUiSnapshot &s) { s.reticle_adjust_step = 1; }},
    {"brightness_auto", [](MenuUiSnapshot &s) { s.brightness_auto = false; }},
    {"brightness_manual_pct", [](MenuUiSnapshot &s) { s.brightness_manual_pct = 80; }},
    {"brightness_auto_top_pct", [](MenuUiSnapshot &s) { s.brightness_auto_top_pct = 60; }},
    {"show_horizon_line", [](MenuUiSnapshot &s) { s.show_horizon_line = false; }},
    {"frame_one_offset", [](MenuUiSnapshot &s) { s.frame_one_offset = 5; }},
    {"frame_spacing_offset", [](MenuUiSnapshot &s) { s.frame_spacing_offset = -5; }},
    {"film_counter", [](MenuUiSnapshot &s) { s.film_counter = 6; }},
    {"last_lidar_error_code", [](MenuUiSnapshot &s) { s.last_lidar_error_code = 6; }},
    {"lidar_recovery_count", [](MenuUiSnapshot &s) { s.lidar_recovery_count = 2; }},
    {"lidarEnabled", [](MenuUiSnapshot &s) { s.lidarEnabled = false; }},
    {"adsReady", [](MenuUiSnapshot &s) { s.adsReady = false; }},
    {"mpuReady", [](MenuUiSnapshot &s) { s.mpuReady = false; }},
    {"mainDisplayReady", [](MenuUiSnapshot &s) { s.mainDisplayReady = false; }},
    {"externalDisplayReady", [](MenuUiSnapshot &s) { s.externalDisplayReady = false; }},
    {"batteryGaugeReady", [](MenuUiSnapshot &s) { s.batteryGaugeReady = false; }},
    {"lightMeterReady", [](MenuUiSnapshot &s) { s.lightMeterReady = false; }},
    {"statusPixelReady", [](MenuUiSnapshot &s) { s.statusPixelReady = false; }},
    {"encoderReady", [](MenuUiSnapshot &s) { s.encoderReady = false; }},
    {"lidarSensorReady", [](MenuUiSnapshot &s) { s.lidarSensorReady = false; }},
    {"prefsSchemaValid", [](MenuUiSnapshot &s) { s.prefsSchemaValid = false; }},
    {"prefsLoadedLegacy", [](MenuUiSnapshot &s) { s.prefsLoadedLegacy = true; }},
    {"prefsSchemaVersionLoaded", [](MenuUiSnapshot &s) { s.prefsSchemaVersionLoaded = 3; }},
};
} // namespace

void test_ui_signature_main_changes_when_any_field_changes()
{
  const MainUiSnapshot base = baselineMainSnapshot();
  const uint32_t baseline = buildMainUiSignature(base);

  TEST_ASSERT_EQUAL_UINT32(baseline, buildMainUiSignature(baselineMainSnapshot()));

  for (const MainUiFieldMutation &mutation : MAIN_UI_FIELD_MUTATIONS)
  {
    MainUiSnapshot mutated = base;
    mutation.apply(mutated);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(baseline, buildMainUiSignature(mutated), mutation.name);
  }
}

void test_ui_signature_menu_changes_when_any_field_changes()
{
  const MenuUiSnapshot base = baselineMenuSnapshot();
  const uint32_t baseline = buildMenuUiSignature(base);

  TEST_ASSERT_EQUAL_UINT32(baseline, buildMenuUiSignature(baselineMenuSnapshot()));

  for (const MenuUiFieldMutation &mutation : MENU_UI_FIELD_MUTATIONS)
  {
    MenuUiSnapshot mutated = base;
    mutation.apply(mutated);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(baseline, buildMenuUiSignature(mutated), mutation.name);
  }
}

void test_ui_signature_external_changes_when_any_field_changes()
{
  const ExternalUiSnapshot base = baselineExternalSnapshot();
  const uint32_t baseline = buildExternalUiSignature(base);

  TEST_ASSERT_EQUAL_UINT32(baseline, buildExternalUiSignature(baselineExternalSnapshot()));

  ExternalUiSnapshot mutated = base;
  mutated.selected_format = 9;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.selected_lens = 9;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.bat_per = 50;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.film_counter = 6;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.frame_progress = 0.5f;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.sleepMode = true;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));
}

void test_ui_signature_menu_changes_when_focus_or_distance_offset_changes()
{
  // Regression: MenuUiSnapshot previously omitted lens_focus_offset and
  // lidar_distance_offset_mm, so Setup > Lens > Focus offset and Setup > LiDAR
  // > Distance offset changed and persisted their value but the redraw-skip
  // cache never saw a signature change, leaving the screen showing the old
  // number until an unrelated field happened to change.
  const MenuUiSnapshot base = baselineMenuSnapshot();
  const uint32_t baseline = buildMenuUiSignature(base);

  TEST_ASSERT_EQUAL_UINT32(baseline, buildMenuUiSignature(baselineMenuSnapshot()));

  MenuUiSnapshot mutated = base;
  mutated.lens_focus_offset = 5;
  TEST_ASSERT_NOT_EQUAL(baseline, buildMenuUiSignature(mutated));

  mutated = base;
  mutated.lidar_distance_offset_mm = 450;
  TEST_ASSERT_NOT_EQUAL(baseline, buildMenuUiSignature(mutated));
}

void test_boot_text_starts_offscreen_right()
{
  // Frame 0: the version text begins fully off the right edge of the display.
  TEST_ASSERT_TRUE(bootTextXForFrame(0, 16, 128, 120) >= 128);
}

void test_boot_text_lands_centered_on_final_frame()
{
  const int totalFrames = 16;
  const int displayWidth = 128;
  const int textWidth = 120;
  int expectedCentered = (displayWidth - textWidth) / 2; // 4
  TEST_ASSERT_EQUAL_INT(expectedCentered,
                        bootTextXForFrame(totalFrames - 1, totalFrames, displayWidth, textWidth));
}

void test_boot_text_is_monotonically_non_increasing()
{
  const int totalFrames = 16;
  int prev = bootTextXForFrame(0, totalFrames, 128, 120);
  for (int f = 1; f < totalFrames; f++)
  {
    int x = bootTextXForFrame(f, totalFrames, 128, 120);
    TEST_ASSERT_TRUE(x <= prev); // film never jitters back to the right
    prev = x;
  }
}

void test_boot_text_single_frame_guard()
{
  // Degenerate totalFrames must not divide by zero; returns the centered x.
  TEST_ASSERT_EQUAL_INT((128 - 120) / 2, bootTextXForFrame(0, 1, 128, 120));
}

void test_boot_sprocket_offset_within_spacing_and_advances()
{
  const int step = 3;
  const int spacing = 12;
  for (int f = 0; f < 40; f++)
  {
    int off = bootSprocketOffsetForFrame(f, step, spacing);
    TEST_ASSERT_TRUE(off >= 0 && off < spacing);
    TEST_ASSERT_EQUAL_INT((f * step) % spacing, off);
  }
}

void test_boot_sprocket_offset_spacing_guard()
{
  // Non-positive spacing must not modulo-by-zero.
  TEST_ASSERT_EQUAL_INT(0, bootSprocketOffsetForFrame(5, 3, 0));
}

namespace
{
// Call-recording fake for the recovery-sequence guard. Only the methods the
// recovery path may touch exist here; the assertions pin the exact sequence.
struct FakeLidarSensor
{
  int clear_error_calls = 0;
  int reset_state_calls = 0;
  int enable_sensor_calls = 0;
  int set_frame_rate_calls = 0;
  int set_distance_scale_calls = 0;
  int set_distance_offset_calls = 0;
  int call_counter = 0;
  int reset_order = 0;
  int enable_order = 0;
  int last_profile_order = 0;
  DTSError reset_result = DTSError::NONE;
  DTSError enable_result = DTSError::NONE;

  void clearError() { clear_error_calls++; call_counter++; }
  DTSError resetState()
  {
    reset_state_calls++;
    reset_order = ++call_counter;
    return reset_result;
  }
  DTSError enableSensor()
  {
    enable_sensor_calls++;
    enable_order = ++call_counter;
    return enable_result;
  }
  void setFrameRate(int) { set_frame_rate_calls++; call_counter++; }
  void setDistanceScale(float)
  {
    set_distance_scale_calls++;
    last_profile_order = ++call_counter;
  }
  void setDistanceOffset(int16_t)
  {
    set_distance_offset_calls++;
    last_profile_order = ++call_counter;
  }
};
} // namespace

void test_lidar_recovery_attempt_never_touches_frame_rate()
{
  // The v10.4.7 incident: re-sending setFrameRate right after enableSensor()
  // in the recovery path destabilises some DTS6012M units into a
  // self-perpetuating recovery loop. The whole recovery attempt — sequence
  // plus calibration profile — must never touch the frame rate, and the
  // profile must be re-applied after enable (resetState clears it).
  FakeLidarSensor fake;
  bool recovered = performLidarRecoveryAttempt(fake, [&]() {
    applyLidarCalibrationProfileTo(fake, 1.0f, static_cast<int16_t>(400));
  });

  TEST_ASSERT_TRUE(recovered);
  TEST_ASSERT_EQUAL_INT(0, fake.set_frame_rate_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.clear_error_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.reset_state_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.enable_sensor_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.set_distance_scale_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.set_distance_offset_calls);
  TEST_ASSERT_TRUE(fake.reset_order < fake.enable_order);
  TEST_ASSERT_TRUE(fake.enable_order < fake.last_profile_order);
}

void test_lidar_recovery_attempt_reapplies_profile_even_on_failure()
{
  // A partial recovery where enableSensor() fails transiently must still
  // re-apply scale + offset, or every later reading is short by the
  // configured offset until a fully successful recovery.
  FakeLidarSensor fake;
  fake.enable_result = DTSError::TIMEOUT;
  bool recovered = performLidarRecoveryAttempt(fake, [&]() {
    applyLidarCalibrationProfileTo(fake, 1.0f, static_cast<int16_t>(400));
  });

  TEST_ASSERT_FALSE(recovered);
  TEST_ASSERT_EQUAL_INT(0, fake.set_frame_rate_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.set_distance_scale_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.set_distance_offset_calls);
}

namespace
{
LoadedPrefsState corruptedPrefsState()
{
  LoadedPrefsState s = {};
  s.iso_index = 999;
  s.selected_lens = -3;
  s.selected_format = 999;
  s.aperture_index = -1;
  s.film_counter = -5;
  s.encoder_value = -100;
  s.prev_encoder_value = -100;
  s.exposure_comp_thirds = 999;
  s.meter_smoothing_mode = 99;
  s.sleep_timeout_mode = -9;
  s.lidar_idle_timeout_mode = 99;
  s.level_trim_landscape_deci_deg = 9999;
  s.level_trim_portrait_pos_deci_deg = -9999;
  s.level_trim_portrait_neg_deci_deg = LEVEL_TRIM_MIN_DECI_DEG + (LEVEL_TRIM_STEP_DECI_DEG / 2) + 1;
  s.reticle_offset_x = 999;
  s.reticle_offset_y = -999;
  s.brightness_manual_pct = 999;
  s.brightness_auto_top_pct = -1;
  s.frame_one_offset = 99;
  s.frame_spacing_offset = -99;
  s.lens_focus_offset = 999;
  return s;
}
} // namespace

void test_clamp_loaded_prefs_recovers_from_corrupted_nvs_values()
{
  // This clamp is the firmware's only defense against corrupted or legacy
  // NVS values feeding array indices (ISOS[], lenses[], film_formats[],
  // METER_SMOOTHING_ALPHA[]). Every field must come back in range.
  LoadedPrefsState s = corruptedPrefsState();
  clampLoadedPrefsState(s);

  TEST_ASSERT_EQUAL_INT(DEFAULT_ISO_INDEX, s.iso_index);
  TEST_ASSERT_EQUAL_INT(ISOS[DEFAULT_ISO_INDEX], s.iso);
  TEST_ASSERT_TRUE(s.selected_lens >= 0 && s.selected_lens < static_cast<int>(NUM_LENSES));
  TEST_ASSERT_TRUE(s.selected_format >= 0 && s.selected_format < static_cast<int>(NUM_FILM_FORMATS));
  TEST_ASSERT_TRUE(s.aperture_index >= 0 && s.aperture_index < LENS_APERTURE_COUNT);
  TEST_ASSERT_TRUE(lenses[s.selected_lens].apertures[s.aperture_index] != 0.0f ||
                   s.aperture_index == 0);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, lenses[s.selected_lens].apertures[s.aperture_index], s.aperture);
  TEST_ASSERT_EQUAL_INT(0, s.film_counter);
  TEST_ASSERT_EQUAL_INT(0, s.encoder_value);
  TEST_ASSERT_EQUAL_INT(0, s.prev_encoder_value);
  TEST_ASSERT_EQUAL_INT(LIGHTMETER_EV_COMP_MAX_THIRDS, s.exposure_comp_thirds);
  TEST_ASSERT_EQUAL_INT(LIGHTMETER_SMOOTHING_MODE_MAX, s.meter_smoothing_mode);
  TEST_ASSERT_EQUAL_INT(SLEEP_TIMEOUT_MODE_MIN, s.sleep_timeout_mode);
  TEST_ASSERT_EQUAL_INT(SLEEP_TIMEOUT_MODE_MAX, s.lidar_idle_timeout_mode);
  TEST_ASSERT_EQUAL_INT(LEVEL_TRIM_MAX_DECI_DEG, s.level_trim_landscape_deci_deg);
  TEST_ASSERT_EQUAL_INT(LEVEL_TRIM_MIN_DECI_DEG, s.level_trim_portrait_pos_deci_deg);
  TEST_ASSERT_EQUAL_INT(RETICLE_OFFSET_MAX, s.reticle_offset_x);
  TEST_ASSERT_EQUAL_INT(RETICLE_OFFSET_MIN, s.reticle_offset_y);
  TEST_ASSERT_EQUAL_INT(BRIGHTNESS_PCT_MAX, s.brightness_manual_pct);
  TEST_ASSERT_EQUAL_INT(BRIGHTNESS_AUTO_TOP_MIN_PCT, s.brightness_auto_top_pct);
  TEST_ASSERT_EQUAL_INT(FRAME_TUNING_MAX, s.frame_one_offset);
  TEST_ASSERT_EQUAL_INT(FRAME_TUNING_MIN, s.frame_spacing_offset);
  TEST_ASSERT_EQUAL_INT(LENS_FOCUS_OFFSET_MAX, s.lens_focus_offset);

  // Level trim snaps to the step grid: min + step/2 + 1 rounds up one step.
  TEST_ASSERT_EQUAL_INT(LEVEL_TRIM_MIN_DECI_DEG + LEVEL_TRIM_STEP_DECI_DEG,
                        s.level_trim_portrait_neg_deci_deg);
}

void test_clamp_loaded_prefs_leaves_valid_state_untouched()
{
  LoadedPrefsState s = {};
  s.iso_index = 5;
  s.selected_lens = 0;
  s.selected_format = 1;
  s.aperture_index = -1; // will resolve to the lens's first valid aperture
  clampLoadedPrefsState(s);
  const LoadedPrefsState before = s;

  clampLoadedPrefsState(s); // idempotent: a second pass changes nothing
  TEST_ASSERT_EQUAL_INT(before.iso_index, s.iso_index);
  TEST_ASSERT_EQUAL_INT(before.aperture_index, s.aperture_index);
  TEST_ASSERT_EQUAL_INT(before.selected_lens, s.selected_lens);
  TEST_ASSERT_EQUAL_INT(before.selected_format, s.selected_format);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, before.aperture, s.aperture);
}

void test_prefs_load_mode_covers_downgrade_and_legacy_branches()
{
  const size_t expected_blob = expectedLegacyLensBlobSize(4);

  // Matching schema loads normally.
  TEST_ASSERT_EQUAL_INT((int)PrefsLoadMode::LOAD_SCHEMA,
                        (int)selectPrefsLoadMode(2, 2, 0, expected_blob));
  // Downgrade (stored schema newer than firmware, e.g. web-updater rollback):
  // values still load best-effort rather than being wiped to defaults.
  TEST_ASSERT_EQUAL_INT((int)PrefsLoadMode::LOAD_SCHEMA,
                        (int)selectPrefsLoadMode(3, 2, 0, expected_blob));
  // Old firmware data with an intact legacy blob migrates.
  TEST_ASSERT_EQUAL_INT((int)PrefsLoadMode::MIGRATE_LEGACY,
                        (int)selectPrefsLoadMode(0, 2, expected_blob, expected_blob));
  // No legacy blob, or a size-mismatched one, falls back to defaults.
  TEST_ASSERT_EQUAL_INT((int)PrefsLoadMode::LOAD_DEFAULTS,
                        (int)selectPrefsLoadMode(0, 2, 0, expected_blob));
  TEST_ASSERT_EQUAL_INT((int)PrefsLoadMode::LOAD_DEFAULTS,
                        (int)selectPrefsLoadMode(0, 2, expected_blob - 1, expected_blob));
}

void test_prefs_health_label_distinguishes_downgrade_from_defaults()
{
  // After a firmware downgrade the values DID load (LOAD_SCHEMA), but the
  // Health screen used to fall through to "Defaults", sending support triage
  // down the wrong path.
  TEST_ASSERT_EQUAL_INT((int)PrefsHealthLabel::SCHEMA_OK,
                        (int)selectPrefsHealthLabel(true, false, 2, 2));
  TEST_ASSERT_EQUAL_INT((int)PrefsHealthLabel::LEGACY_MIGRATED,
                        (int)selectPrefsHealthLabel(false, true, 2, 2));
  TEST_ASSERT_EQUAL_INT((int)PrefsHealthLabel::NEWER_SCHEMA_LOADED,
                        (int)selectPrefsHealthLabel(false, false, 3, 2));
  TEST_ASSERT_EQUAL_INT((int)PrefsHealthLabel::DEFAULTS,
                        (int)selectPrefsHealthLabel(false, false, 0, 2));
}

void test_exposure_compensation_direction_and_magnitude()
{
  // Sign convention: positive EC means deliberate overexposure, so effective
  // lux must DROP (longer computed shutter). An inverted sign here is a
  // silent full-roll exposure error, which is why the direction is pinned.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, applyExposureCompensationToLux(1000.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, applyExposureCompensationToLux(1000.0f, -1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 793.70f, applyExposureCompensationToLux(1000.0f, 1.0f / 3.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, applyExposureCompensationToLux(0.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, applyExposureCompensationToLux(-5.0f, -2.0f));
}

void test_calculate_ev100_reference_points_and_dark_guard()
{
  // EV100 = log2(lux * 100 / K) with K = 12.5: lux 0.125 -> EV 0, lux 32 -> EV 8.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, calculateEV100(0.125f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, calculateEV100(32.0f));
  TEST_ASSERT_FLOAT_IS_NAN(calculateEV100(0.0f));
  TEST_ASSERT_FLOAT_IS_NAN(calculateEV100(-10.0f));
}

void test_meter_smoothing_mode_clamps_out_of_range()
{
  // A corrupted prefs value indexes METER_SMOOTHING_ALPHA/LABELS; the clamp
  // is the only bounds defense, so out-of-range modes must map to the edges.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, getMeterSmoothingAlpha(0), getMeterSmoothingAlpha(-1));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, getMeterSmoothingAlpha(LIGHTMETER_SMOOTHING_MODE_MAX),
                           getMeterSmoothingAlpha(99));
  TEST_ASSERT_EQUAL_STRING("Off", getMeterSmoothingLabel(-5));
  TEST_ASSERT_EQUAL_STRING("High", getMeterSmoothingLabel(99));
  TEST_ASSERT_EQUAL_STRING("Medium", getMeterSmoothingLabel(2));
}

void test_format_shutter_speed_iso_zero_caps_at_max()
{
  // iso=0 makes the computed speed infinite; the max-speed cap must absorb it
  // and render the 25-minute ceiling instead of garbage or a crash.
  char buffer[16] = {0};
  formatShutterSpeed(1000.0f, 8.0f, 0, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("25m0s", buffer);
}

void test_lidar_backoff_survives_continued_timeout_and_error_events()
{
  // During a sustained outage every poll feeds TIMEOUT (or ERROR past the
  // threshold), and each event used to rewrite next_recovery_attempt_ms to
  // now — so the exponential backoff never gated anything and a permanently
  // failing sensor would be reset at the raw poll cadence with no settle
  // time, the exact hazard the v10.4.7 note warns about.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1700);
  noteLidarRecoveryAttemptResult(state, false, 1700);
  const unsigned long deadline = state.next_recovery_attempt_ms;
  TEST_ASSERT_GREATER_THAN_UINT32(1700, deadline);

  // Continued timeouts inside the backoff window must not re-arm early.
  LidarRecoveryDecision duringBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1800);
  TEST_ASSERT_FALSE(duringBackoff.attempt_recovery);
  TEST_ASSERT_EQUAL_UINT32(deadline, state.next_recovery_attempt_ms);

  // Errors past the threshold must not re-arm early either.
  state.consecutive_errors = LIDAR_RECOVERY_ERROR_THRESHOLD;
  LidarRecoveryDecision errorDuringBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, 1850);
  TEST_ASSERT_FALSE(errorDuringBackoff.attempt_recovery);

  // Once the deadline passes, the next event releases the attempt.
  LidarRecoveryDecision afterBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, deadline);
  TEST_ASSERT_TRUE(afterBackoff.attempt_recovery);
}

void test_lidar_rejected_frames_prevent_timeout_recovery()
{
  // A camera aimed at open sky (or >18m, or hard sun) streams healthy frames
  // whose candidates are all gated out. Those frames prove the link works, so
  // interleaved no-frame polls must never escalate to TIMEOUT recovery — the
  // sensor is fine, only the scene is unmeasurable. This used to reset a
  // working sensor about every 1.5s and climb the Health screen Recoveries
  // counter for as long as the drought lasted.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  bool sawClearDisplay = false;
  for (unsigned long now = 100; now <= 10000; now += 100)
  {
    // Frame arrives, candidate rejected.
    updateLidarRecoveryState(state, LidarRecoveryEvent::NO_VALID_MEASUREMENT, now);
    // A poll lands between frames.
    LidarRecoveryDecision decision = updateLidarRecoveryState(
        state, lidarRecoveryEventForUpdateError(DTSError::NO_NEW_DATA), now + 10);
    TEST_ASSERT_FALSE(decision.attempt_recovery);
    TEST_ASSERT_FALSE(state.recovering);
    sawClearDisplay = sawClearDisplay || decision.clear_display;
  }

  // Display staleness still keys off accepted measurements: with nothing
  // accepted for 10s the placeholder decision must have fired.
  TEST_ASSERT_TRUE(sawClearDisplay);
}

void test_lidar_rejected_frame_stands_down_active_recovery()
{
  // If timeouts already escalated to recovery and frames then resume (still
  // gated), the link is healthy again: recovery must stand down instead of
  // resetting a streaming sensor.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, LIDAR_RECOVERY_TIMEOUT_MS + 1);
  TEST_ASSERT_TRUE(state.recovering);

  updateLidarRecoveryState(state, LidarRecoveryEvent::NO_VALID_MEASUREMENT,
                           LIDAR_RECOVERY_TIMEOUT_MS + 2);
  TEST_ASSERT_FALSE(state.recovering);
  TEST_ASSERT_EQUAL_INT(0, state.consecutive_errors);
}

void test_lidar_true_silence_still_escalates_to_recovery()
{
  // Rejected frames must not weaken the genuine-failure path: total silence
  // past LIDAR_RECOVERY_TIMEOUT_MS still recovers.
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  updateLidarRecoveryState(state, LidarRecoveryEvent::NO_VALID_MEASUREMENT, 100);
  LidarRecoveryDecision decision = updateLidarRecoveryState(
      state, LidarRecoveryEvent::TIMEOUT, 100 + LIDAR_RECOVERY_TIMEOUT_MS + 1);
  TEST_ASSERT_TRUE(decision.attempt_recovery);
  TEST_ASSERT_TRUE(state.recovering);
}

void test_cycle_seed_points_roundtrip_to_the_same_frame_across_tuning_range()
{
  // Manually cycling the frame counter seeds the encoder with an adjusted
  // sensor point; estimateFilmCounter must map that exact point back to the
  // same frame for every format and every corner of the tuning range.
  // cyclefuncs.cpp used to duplicate the offset formula without the monotonic
  // clamp, so a negative spacing offset could seed frame N and display N-1.
  const int tuning_values[] = {FRAME_TUNING_MIN, 0, FRAME_TUNING_MAX};

  for (size_t format_index = 0; format_index < NUM_FILM_FORMATS; format_index++)
  {
    const FilmFormat &fmt = film_formats[format_index];
    const int point_count = getFilmFormatPointCount(fmt);

    for (int f1 : tuning_values)
    {
      for (int fs : tuning_values)
      {
        for (int i = 0; i < point_count; i++)
        {
          const int seeded = adjustedSensorPointForIndex(fmt, i, f1, fs);
          const FilmCounterEstimate est = estimateFilmCounter(fmt, seeded, f1, fs);
          TEST_ASSERT_TRUE_MESSAGE(est.valid, fmt.name);
          TEST_ASSERT_EQUAL_INT_MESSAGE(fmt.frame[i], est.frame, fmt.name);
        }
      }
    }
  }
}

void test_adjusted_sensor_points_stay_monotonic_with_negative_spacing()
{
  // The monotonic clamp is the difference between the two former copies of
  // the formula: consecutive seeded points must always strictly increase,
  // even at the most negative spacing offset.
  for (size_t format_index = 0; format_index < NUM_FILM_FORMATS; format_index++)
  {
    const FilmFormat &fmt = film_formats[format_index];
    const int point_count = getFilmFormatPointCount(fmt);
    int prev = adjustedSensorPointForIndex(fmt, 0, FRAME_TUNING_MIN, FRAME_TUNING_MIN);
    for (int i = 1; i < point_count; i++)
    {
      const int current = adjustedSensorPointForIndex(fmt, i, FRAME_TUNING_MIN, FRAME_TUNING_MIN);
      TEST_ASSERT_TRUE_MESSAGE(current > prev, fmt.name);
      prev = current;
    }
  }
}

void test_all_nvs_prefs_keys_fit_the_15_char_limit()
{
  // ESP32 NVS silently fails on keys longer than 15 characters; v10.3.5
  // shipped that bug. Every key must live in prefs_keys.h so this guard
  // sees it. "selected_format" sits exactly at the limit today.
  for (size_t i = 0; i < PREFS_STATIC_KEY_COUNT; i++)
  {
    TEST_ASSERT_NOT_NULL(PREFS_ALL_STATIC_KEYS[i]);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(PREFS_NVS_MAX_KEY_LEN, strlen(PREFS_ALL_STATIC_KEYS[i]));
  }

  // Dynamic per-lens keys must fit with the highest real lens index.
  char lensKey[32] = {0};
  snprintf(lensKey, sizeof(lensKey), PREFS_KEY_PATTERN_LENS_READINGS,
           static_cast<unsigned int>(NUM_LENSES - 1));
  TEST_ASSERT_LESS_OR_EQUAL_UINT(PREFS_NVS_MAX_KEY_LEN, strlen(lensKey));
  snprintf(lensKey, sizeof(lensKey), PREFS_KEY_PATTERN_LENS_CALIBRATED,
           static_cast<unsigned int>(NUM_LENSES - 1));
  TEST_ASSERT_LESS_OR_EQUAL_UINT(PREFS_NVS_MAX_KEY_LEN, strlen(lensKey));
}

void test_all_nvs_prefs_keys_are_unique()
{
  // A duplicated key means two settings silently share one NVS slot.
  for (size_t i = 0; i < PREFS_STATIC_KEY_COUNT; i++)
  {
    for (size_t j = i + 1; j < PREFS_STATIC_KEY_COUNT; j++)
    {
      TEST_ASSERT_TRUE_MESSAGE(strcmp(PREFS_ALL_STATIC_KEYS[i], PREFS_ALL_STATIC_KEYS[j]) != 0,
                               PREFS_ALL_STATIC_KEYS[i]);
    }
  }
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_frame_counter_exact_and_interpolation);
  RUN_TEST(test_frame_counter_snap_and_roll_end);
  RUN_TEST(test_frame_counter_frame_offset_and_spacing);
  RUN_TEST(test_format_3x6_supports_21_frames);
  RUN_TEST(test_config_header_autoscales_only_when_9x15_overflows);
  RUN_TEST(test_encoder_filter_forward_hysteresis_and_debounce);
  RUN_TEST(test_encoder_filter_reverse_requires_rewind_mode);
  RUN_TEST(test_lidar_timeout_recovery_and_backoff);
  RUN_TEST(test_lidar_recovery_state_machine_survives_many_consecutive_failures);
  RUN_TEST(test_lidar_recovery_event_mapping_treats_no_new_data_as_benign);
  RUN_TEST(test_lidar_no_new_data_polls_do_not_trip_spurious_recovery);
  RUN_TEST(test_lidar_recovery_reset_on_enable_prevents_instant_wake_recovery);
  RUN_TEST(test_lidar_recovery_attempt_never_touches_frame_rate);
  RUN_TEST(test_lidar_recovery_attempt_reapplies_profile_even_on_failure);
  RUN_TEST(test_prefs_load_mode_covers_downgrade_and_legacy_branches);
  RUN_TEST(test_prefs_health_label_distinguishes_downgrade_from_defaults);
  RUN_TEST(test_clamp_loaded_prefs_recovers_from_corrupted_nvs_values);
  RUN_TEST(test_clamp_loaded_prefs_leaves_valid_state_untouched);
  RUN_TEST(test_exposure_compensation_direction_and_magnitude);
  RUN_TEST(test_calculate_ev100_reference_points_and_dark_guard);
  RUN_TEST(test_meter_smoothing_mode_clamps_out_of_range);
  RUN_TEST(test_format_shutter_speed_iso_zero_caps_at_max);
  RUN_TEST(test_lidar_backoff_survives_continued_timeout_and_error_events);
  RUN_TEST(test_lidar_rejected_frames_prevent_timeout_recovery);
  RUN_TEST(test_lidar_rejected_frame_stands_down_active_recovery);
  RUN_TEST(test_lidar_true_silence_still_escalates_to_recovery);
  RUN_TEST(test_calibration_logic_rejects_invalid_input_shapes);
  RUN_TEST(test_calibration_validation_stable_and_monotonic);
  RUN_TEST(test_ascending_calibration_requires_increasing_readings);
  RUN_TEST(test_calibration_median_spread_tolerates_gentle_drift);
  RUN_TEST(test_calibration_rejects_truly_unstable_readings);
  RUN_TEST(test_prefs_migration_mode_and_blob_apply);
  RUN_TEST(test_lidar_candidate_selection_and_blend);
  RUN_TEST(test_lidar_plausibility_gate_rejects_overshoot);
  RUN_TEST(test_lidar_plausibility_gate_allows_undershoot_and_far_focus);
  RUN_TEST(test_lidar_plausibility_gate_boundary_at_overshoot_delta);
  RUN_TEST(test_lidar_plausibility_gate_trusts_confident_readings);
  RUN_TEST(test_lidar_plausibility_trust_boundaries);
  RUN_TEST(test_lidar_plausibility_overshoot_scales_with_prior);
  RUN_TEST(test_lidar_snr_permille_math);
  RUN_TEST(test_lidar_signal_loss_placeholder_distinguishes_far_dropout);
  RUN_TEST(test_lidar_signal_loss_placeholder_sticks_across_prev_distance_reset);
  RUN_TEST(test_lidar_telemetry_age_formatting_covers_each_band);
  RUN_TEST(test_lidar_plausibility_hold_releases_on_stable_far_readings);
  RUN_TEST(test_lidar_plausibility_hold_caps_noisy_beam_miss);
  RUN_TEST(test_lidar_plausibility_hold_reset_restarts_counts);
  RUN_TEST(test_lidar_stable_boost_under_min_frames_does_nothing);
  RUN_TEST(test_lidar_stable_boost_kicks_in_at_min_frames);
  RUN_TEST(test_lidar_stable_boost_clamps_at_max);
  RUN_TEST(test_lidar_sunlight_warn_hysteresis);
  RUN_TEST(test_lidar_rejects_invalid_quality_and_zero_intensity);
  RUN_TEST(test_lidar_distance_display_formatting_covers_each_band);
  RUN_TEST(test_lidar_low_confidence_tracks_beyond_previous_distance);
  RUN_TEST(test_lidar_dynamic_intensity_threshold_accepts_mid_range);
  RUN_TEST(test_lidar_far_fallback_prevents_dropouts);
  RUN_TEST(test_near_range_correction_is_bounded_below_by_floor);
  RUN_TEST(test_near_range_correction_compensates_for_offset_pref_delta);
  RUN_TEST(test_lidar_mm_to_cm_conversion_rounds_to_nearest);
  RUN_TEST(test_lidar_candidate_fusion_when_returns_agree);
  RUN_TEST(test_lidar_fusion_takes_max_confidence_not_average);
  RUN_TEST(test_lidar_lens_prior_is_range_weighted);
  RUN_TEST(test_lidar_sunlight_penalizes_confidence_without_dropping_valid_measurement);
  RUN_TEST(test_lidar_sunlight_hard_rejects_very_low_snr_measurement);
  RUN_TEST(test_lens_snap_and_distance_estimation);
  RUN_TEST(test_lens_logic_ignores_unused_distance_markers);
  RUN_TEST(test_far_snap_deadzone_is_distance_relative_at_sparse_marks);
  RUN_TEST(test_shipped_calibrated_lens_readings_are_ascending);
  RUN_TEST(test_default_lens_estimate_interpolates_between_marks);
  RUN_TEST(test_150mm_profile_uses_custom_distance_scale);
  RUN_TEST(test_250mm_profiles_use_custom_distance_scales);
  RUN_TEST(test_lightmeter_dark_bright_fraction_and_seconds);
  RUN_TEST(test_frameline_scaling_returns_base_when_format_mm_invalid);
  RUN_TEST(test_frameline_scaling_height_constrained_for_taller_format);
  RUN_TEST(test_frameline_scaling_width_constrained_for_wider_pixel_ratio);
  RUN_TEST(test_frameline_scaling_allows_overflow_when_format_is_wider_and_taller);
  RUN_TEST(test_frameline_scaling_clamps_dimensions_to_minimum_one);
  RUN_TEST(test_6x9_and_9x3_share_corrected_sensor_table);
  RUN_TEST(test_lightmeter_scale_split_preserves_effective_exposure_constant);
  RUN_TEST(test_lens_moving_average_primes_with_first_sample);
  RUN_TEST(test_lens_spike_filter_initialises_and_accepts_small_movement);
  RUN_TEST(test_lens_spike_filter_rejects_lone_spike_then_promotes_consistent_pending);
  RUN_TEST(test_lens_spike_filter_drifting_pending_resets_count);
  RUN_TEST(test_cm_to_readable_renders_cm_below_one_metre_and_metres_above);
  RUN_TEST(test_lidar_secondary_quality_inherits_primary_when_invalid);
  RUN_TEST(test_ui_signature_hash_primitives_are_deterministic_and_distinct);
  RUN_TEST(test_ui_signature_hash_cstring_treats_null_as_empty);
  RUN_TEST(test_ui_signature_external_changes_when_any_field_changes);
  RUN_TEST(test_ui_signature_menu_changes_when_focus_or_distance_offset_changes);
  RUN_TEST(test_boot_text_starts_offscreen_right);
  RUN_TEST(test_boot_text_lands_centered_on_final_frame);
  RUN_TEST(test_boot_text_is_monotonically_non_increasing);
  RUN_TEST(test_boot_text_single_frame_guard);
  RUN_TEST(test_boot_sprocket_offset_within_spacing_and_advances);
  RUN_TEST(test_boot_sprocket_offset_spacing_guard);
  RUN_TEST(test_all_nvs_prefs_keys_fit_the_15_char_limit);
  RUN_TEST(test_all_nvs_prefs_keys_are_unique);
  RUN_TEST(test_cycle_seed_points_roundtrip_to_the_same_frame_across_tuning_range);
  RUN_TEST(test_adjusted_sensor_points_stay_monotonic_with_negative_spacing);
  RUN_TEST(test_ui_signature_main_changes_when_any_field_changes);
  RUN_TEST(test_ui_signature_menu_changes_when_any_field_changes);
  return UNITY_END();
}
