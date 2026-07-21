#ifndef SCREENSHOT_STUB_ADAFRUIT_SSD1306_H
#define SCREENSHOT_STUB_ADAFRUIT_SSD1306_H

#include <cstdint>

#include "Wire.h"
#include "screenshot_canvas.h"

// Canvas-backed stand-in for the 128x32 SSD1306 external OLED.
class Adafruit_SSD1306 : public CanvasDisplay
{
public:
  Adafruit_SSD1306(uint8_t w, uint8_t h, TwoWire * = nullptr, int8_t = -1,
                   uint32_t = 0, uint32_t = 0)
      : CanvasDisplay(static_cast<int16_t>(w), static_cast<int16_t>(h))
  {
  }

  bool begin(uint8_t = 0, uint8_t = 0, bool = true, bool = true) { return true; }
  void setContrast(uint8_t) {}
  void dim(bool) {}
  void ssd1306_command(uint8_t) {}
};

#endif // SCREENSHOT_STUB_ADAFRUIT_SSD1306_H
