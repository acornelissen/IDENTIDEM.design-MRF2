#ifndef SCREENSHOT_STUB_ADAFRUIT_MAX1704X_H
#define SCREENSHOT_STUB_ADAFRUIT_MAX1704X_H

#include <cstdint>

class Adafruit_MAX17048
{
public:
  bool begin() { return true; }
  float cellPercent() { return 0.0f; }
  float cellVoltage() { return 0.0f; }
};

#endif // SCREENSHOT_STUB_ADAFRUIT_MAX1704X_H
