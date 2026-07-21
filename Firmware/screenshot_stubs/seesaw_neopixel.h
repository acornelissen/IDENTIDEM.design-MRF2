#ifndef SCREENSHOT_STUB_SEESAW_NEOPIXEL_H
#define SCREENSHOT_STUB_SEESAW_NEOPIXEL_H

#include <cstdint>

// The status NeoPixel is disabled in the fixtures (hardware.statusPixel =
// false), so every call here is inert.
class seesaw_NeoPixel
{
public:
  seesaw_NeoPixel(uint16_t = 0, uint8_t = 0, uint16_t = 0) {}
  bool begin(uint8_t = 0x60, int8_t = -1) { return true; }
  void setBrightness(uint8_t) {}
  void setPixelColor(uint16_t, uint32_t) {}
  uint32_t Color(uint8_t r, uint8_t g, uint8_t b)
  {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
  }
  void show() {}
};

#endif // SCREENSHOT_STUB_SEESAW_NEOPIXEL_H
