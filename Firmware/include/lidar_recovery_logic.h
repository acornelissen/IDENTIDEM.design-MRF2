#ifndef LIDAR_RECOVERY_LOGIC_H
#define LIDAR_RECOVERY_LOGIC_H

#include <Arduino.h>

enum class LidarRecoveryEvent
{
  VALID_MEASUREMENT = 0,
  NO_VALID_MEASUREMENT = 1,
  TIMEOUT = 2,
  ERROR = 3
};

struct LidarRecoveryState
{
  bool initialized;
  bool recovering;
  int consecutive_errors;
  unsigned long last_valid_measurement_ms;
  unsigned long next_recovery_attempt_ms;
};

struct LidarRecoveryDecision
{
  bool clear_display;
  bool attempt_recovery;
};

void resetLidarRecoveryState(LidarRecoveryState &state, unsigned long now_ms);
LidarRecoveryDecision updateLidarRecoveryState(
    LidarRecoveryState &state, LidarRecoveryEvent event, unsigned long now_ms);
void noteLidarRecoveryAttemptResult(LidarRecoveryState &state, bool success, unsigned long now_ms);

#endif // LIDAR_RECOVERY_LOGIC_H
