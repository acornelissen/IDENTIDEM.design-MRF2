#ifndef SCREENSHOT_STUB_ADAFRUIT_SH110X_H
#define SCREENSHOT_STUB_ADAFRUIT_SH110X_H

#include <cstdint>

#include "Wire.h"
#include "screenshot_canvas.h"

// Canvas-backed stand-in for the 128x128 SH1107 primary OLED. Drawing goes to
// the software buffer; the hardware-only methods are inert.
class Adafruit_SH1107 : public CanvasDisplay
{
public:
  Adafruit_SH1107(uint16_t w, uint16_t h, TwoWire * = nullptr, int8_t = -1,
                  uint32_t = 0, uint32_t = 0)
      : CanvasDisplay(static_cast<int16_t>(w), static_cast<int16_t>(h))
  {
  }

  bool begin(uint8_t = 0, bool = true) { return true; }
  void setContrast(uint8_t) {}
  void oled_command(uint8_t) {}
};

#endif // SCREENSHOT_STUB_ADAFRUIT_SH110X_H
