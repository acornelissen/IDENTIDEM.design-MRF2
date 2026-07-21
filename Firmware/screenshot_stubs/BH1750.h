#ifndef SCREENSHOT_STUB_BH1750_H
#define SCREENSHOT_STUB_BH1750_H

#include <cstdint>

class BH1750
{
public:
  BH1750(uint8_t = 0x23) {}
  bool begin(int = 0) { return true; }
  float readLightLevel() { return 0.0f; }
};

#endif // SCREENSHOT_STUB_BH1750_H
