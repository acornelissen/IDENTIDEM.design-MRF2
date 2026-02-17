#include "lightmeter_logic.h"

#include <math.h>

#include "mrfconstants.h"

namespace
{
struct SpeedRange
{
  float lower;
  float upper;
  const char *label;
};

const float SPEED_TOO_FAST_THRESHOLD = 0.001f;
const SpeedRange SPEED_RANGES[] = {
    {0.001f, 0.002f, "1/1000"},
    {0.002f, 0.004f, "1/500"},
    {0.004f, 0.008f, "1/250"},
    {0.008f, 0.016f, "1/125"},
    {0.016f, 0.033f, "1/60"},
    {0.033f, 0.066f, "1/30"},
    {0.066f, 0.125f, "1/15"},
    {0.125f, 0.250f, "1/8"},
    {0.250f, 0.500f, "1/4"},
    {0.500f, 1.000f, "1/2"}};

const int SPEED_RANGE_COUNT = sizeof(SPEED_RANGES) / sizeof(SPEED_RANGES[0]);

const char *getShutterFractionLabel(float speed)
{
  for (int i = 0; i < SPEED_RANGE_COUNT; i++)
  {
    if (SPEED_RANGES[i].lower <= speed && speed < SPEED_RANGES[i].upper)
    {
      return SPEED_RANGES[i].label;
    }
  }
  return nullptr;
}
} // namespace

String formatShutterSpeed(float lux, float aperture, int iso)
{
  if (lux <= 0)
  {
    return "Dark!";
  }

  float speed = roundf(((aperture * aperture) * K) / (lux * static_cast<float>(iso)) * LIGHTMETER_SPEED_ROUND_SCALE) /
                LIGHTMETER_SPEED_ROUND_SCALE;

  if (speed < SPEED_TOO_FAST_THRESHOLD)
  {
    return "Bright!";
  }

  if (speed >= SPEED_SECONDS_THRESHOLD)
  {
    char speed_raw[SPEED_TEXT_BUFFER_LEN];
    dtostrf(speed, SPEED_TEXT_WIDTH, SPEED_TEXT_DECIMALS_LONG, speed_raw);
    return String(speed_raw) + " sec.";
  }

  const char *fraction_label = getShutterFractionLabel(speed);
  if (fraction_label != nullptr)
  {
    return String(fraction_label) + " sec.";
  }

  char speed_raw[SPEED_TEXT_BUFFER_LEN];
  dtostrf(speed, SPEED_TEXT_WIDTH, SPEED_TEXT_DECIMALS_SHORT, speed_raw);
  return String(speed_raw) + " sec.";
}
