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

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_register_activity_sets_timestamp_and_wakes);
  RUN_TEST(test_sleep_state_machine_boot_grace_and_modes);
  RUN_TEST(test_idle_duration_handles_ordering);
  return UNITY_END();
}
