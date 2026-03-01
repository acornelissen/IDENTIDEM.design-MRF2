#include <unity.h>

#include "globals.h"
#include "mrfconstants.h"

namespace
{
unsigned long fakeMillis = 0;
}

unsigned long millis()
{
  return fakeMillis;
}

unsigned long getSleepTimeoutModeMs(int timeout_mode)
{
  switch (timeout_mode)
  {
  case SLEEP_TIMEOUT_MODE_OFF:
    return 0;
  case SLEEP_TIMEOUT_MODE_15S:
    return 15000;
  case SLEEP_TIMEOUT_MODE_30S:
    return 30000;
  case SLEEP_TIMEOUT_MODE_1M:
    return 60000;
  case SLEEP_TIMEOUT_MODE_1M30S:
    return 90000;
  case SLEEP_TIMEOUT_MODE_2M:
    return 120000;
  default:
    return 60000;
  }
}

Preferences prefs;
UiMode ui_mode = UiMode::Main;
int sleep_timeout_mode = DEFAULT_SLEEP_TIMEOUT_MODE;
unsigned long lastActivityTime = 0;
bool sleepMode = false;

#include "../../src/activity.cpp"

void setUp() {}
void tearDown() {}

void test_register_activity_sets_timestamp_and_wakes()
{
  fakeMillis = 1234;
  sleepMode = true;
  lastActivityTime = 0;

  registerActivity();

  TEST_ASSERT_FALSE(sleepMode);
  TEST_ASSERT_EQUAL_UINT32(1234, lastActivityTime);
}

void test_sleep_state_machine_boot_grace_and_modes()
{
  ui_mode = UiMode::Main;
  sleep_timeout_mode = SLEEP_TIMEOUT_MODE_15S;
  lastActivityTime = 0;
  sleepMode = false;

  updateSleepMode(0);
  TEST_ASSERT_FALSE(sleepMode);

  updateSleepMode(SLEEP_BOOT_GRACE_MS - 1);
  TEST_ASSERT_FALSE(sleepMode);

  updateSleepMode(SLEEP_BOOT_GRACE_MS + 15001);
  TEST_ASSERT_TRUE(sleepMode);

  sleepMode = false;
  ui_mode = UiMode::Config;
  updateSleepMode(SLEEP_BOOT_GRACE_MS + 30000);
  TEST_ASSERT_FALSE(sleepMode);

  ui_mode = UiMode::Main;
  sleep_timeout_mode = SLEEP_TIMEOUT_MODE_OFF;
  sleepMode = true;
  updateSleepMode(SLEEP_BOOT_GRACE_MS + 45000);
  TEST_ASSERT_FALSE(sleepMode);

  sleep_timeout_mode = SLEEP_TIMEOUT_MODE_15S;
  sleepMode = false;
  lastActivityTime = 70000;
  updateSleepMode(60000);
  TEST_ASSERT_EQUAL_UINT32(60000, lastActivityTime);
  TEST_ASSERT_FALSE(sleepMode);
}

void test_idle_duration_handles_ordering()
{
  lastActivityTime = 1000;
  TEST_ASSERT_EQUAL_UINT32(300, getIdleDurationMs(1300));
  TEST_ASSERT_EQUAL_UINT32(0, getIdleDurationMs(900));
}

void test_sleep_mode_off_never_enters_sleep()
{
  // Complement to the existing test: MODE_OFF must also prevent a non-sleeping
  // device from ever entering sleep, regardless of idle duration.
  sleep_timeout_mode = SLEEP_TIMEOUT_MODE_OFF;
  lastActivityTime = 0;
  sleepMode = false;

  updateSleepMode(SLEEP_BOOT_GRACE_MS + 3600000UL); // 1 hour idle
  TEST_ASSERT_FALSE(sleepMode);
}

void test_all_timeout_modes_have_correct_boundaries()
{
  // Verify each configured timeout fires at exactly the right millisecond
  // boundary. Use a fixed "now" well past the grace window and set
  // lastActivityTime to dial the idle duration precisely, avoiding
  // cross-iteration state accumulation.
  ui_mode = UiMode::Main;

  const struct
  {
    int  mode;
    unsigned long timeoutMs;
  } cases[] = {
    { SLEEP_TIMEOUT_MODE_15S,   15000  },
    { SLEEP_TIMEOUT_MODE_30S,   30000  },
    { SLEEP_TIMEOUT_MODE_1M,    60000  },
    { SLEEP_TIMEOUT_MODE_1M30S, 90000  },
    { SLEEP_TIMEOUT_MODE_2M,    120000 },
  };

  for (const auto &c : cases)
  {
    sleep_timeout_mode = c.mode;
    const unsigned long now = SLEEP_BOOT_GRACE_MS + c.timeoutMs + 10;

    // Idle = timeoutMs - 1  →  should NOT sleep yet.
    lastActivityTime = now - (c.timeoutMs - 1);
    sleepMode = false;
    updateSleepMode(now);
    TEST_ASSERT_FALSE(sleepMode);

    // Idle = timeoutMs + 1  →  should sleep.
    lastActivityTime = now - (c.timeoutMs + 1);
    sleepMode = false;
    updateSleepMode(now);
    TEST_ASSERT_TRUE(sleepMode);
  }
}

void test_sleep_constants_match_intended_durations()
{
  // LOOP_SLEEP_LIGHT_SLEEP_US drives the timer wakeup interval in
  // runSleepTasks(); changing it accidentally would silently alter sensor
  // responsiveness during device sleep.
  TEST_ASSERT_EQUAL_UINT64(100000ULL, LOOP_SLEEP_LIGHT_SLEEP_US);

  // Wake-detection thresholds: tighten these and the device becomes harder
  // to wake; loosen them and spurious noise causes unwanted wakes.
  TEST_ASSERT_EQUAL_INT(1, SLEEP_WAKE_ENCODER_DELTA);
  TEST_ASSERT_EQUAL_INT(8, SLEEP_WAKE_LENS_DELTA);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_register_activity_sets_timestamp_and_wakes);
  RUN_TEST(test_sleep_state_machine_boot_grace_and_modes);
  RUN_TEST(test_idle_duration_handles_ordering);
  RUN_TEST(test_sleep_mode_off_never_enters_sleep);
  RUN_TEST(test_all_timeout_modes_have_correct_boundaries);
  RUN_TEST(test_sleep_constants_match_intended_durations);
  return UNITY_END();
}
