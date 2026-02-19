#include "lidar_recovery_logic.h"

#include <Arduino.h>

#include "mrfconstants.h"

namespace
{
unsigned long computeRecoveryBackoffMs(int consecutiveErrors)
{
  unsigned long delayMs = LIDAR_RECOVERY_RETRY_BASE_MS;
  int attempts = min(consecutiveErrors, 6);
  for (int i = 1; i < attempts; i++)
  {
    delayMs = min(delayMs * 2UL, LIDAR_RECOVERY_RETRY_MAX_MS);
  }
  return delayMs;
}
} // namespace

void resetLidarRecoveryState(LidarRecoveryState &state, unsigned long now_ms)
{
  state.initialized = true;
  state.recovering = false;
  state.consecutive_errors = 0;
  state.last_valid_measurement_ms = now_ms;
  state.next_recovery_attempt_ms = now_ms;
}

LidarRecoveryDecision updateLidarRecoveryState(
    LidarRecoveryState &state, LidarRecoveryEvent event, unsigned long now_ms)
{
  if (!state.initialized)
  {
    resetLidarRecoveryState(state, now_ms);
  }

  if (event == LidarRecoveryEvent::VALID_MEASUREMENT)
  {
    state.recovering = false;
    state.consecutive_errors = 0;
    state.last_valid_measurement_ms = now_ms;
    state.next_recovery_attempt_ms = now_ms;
    return {false, false};
  }

  if (event == LidarRecoveryEvent::ERROR)
  {
    state.consecutive_errors++;
    if (state.consecutive_errors >= LIDAR_RECOVERY_ERROR_THRESHOLD)
    {
      state.recovering = true;
      state.next_recovery_attempt_ms = now_ms;
    }
  }
  else if (event == LidarRecoveryEvent::TIMEOUT)
  {
    if ((now_ms - state.last_valid_measurement_ms) > LIDAR_RECOVERY_TIMEOUT_MS)
    {
      state.recovering = true;
      state.next_recovery_attempt_ms = now_ms;
    }
  }

  bool clearDisplay = (now_ms - state.last_valid_measurement_ms) > LIDAR_NO_DATA_TIMEOUT_MS;
  bool attemptRecovery = state.recovering && now_ms >= state.next_recovery_attempt_ms;
  return {clearDisplay, attemptRecovery};
}

void noteLidarRecoveryAttemptResult(LidarRecoveryState &state, bool success, unsigned long now_ms)
{
  if (!state.initialized)
  {
    resetLidarRecoveryState(state, now_ms);
  }

  if (success)
  {
    state.recovering = false;
    state.consecutive_errors = 0;
    state.last_valid_measurement_ms = now_ms;
    state.next_recovery_attempt_ms = now_ms;
    return;
  }

  state.recovering = true;
  state.consecutive_errors = min(state.consecutive_errors + 1, 10);
  state.next_recovery_attempt_ms = now_ms + computeRecoveryBackoffMs(state.consecutive_errors);
}
