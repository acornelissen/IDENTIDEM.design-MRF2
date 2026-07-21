#ifndef SCREENSHOT_STUB_CANVAS_H
#define SCREENSHOT_STUB_CANVAS_H

// Software-canvas backing for the two OLEDs. Subclasses Adafruit_GFX directly
// and implements only drawPixel, so every GFX primitive (fillRect, drawLine,
// fillCircle, drawRect...) routes through it. drawPixel reproduces the real
// GrayOLED / SH110X semantics exactly: colour 1 sets, 0 clears, 2 (INVERSE)
// XORs. That XOR is load-bearing — the main-UI focus ring is drawn as two
// nested INVERSE circles and only renders as a ring, not a filled disc, when
// INVERSE toggles pixels the way the hardware driver does.
//
// The 1-bit buffer uses the GFXcanvas1 layout (rows of ceil(w/8) bytes,
// MSB-first) so screenshot_svg_logic can consume getBuffer() directly.

#include <cstdint>
#include <cstring>
#include <vector>

#include <Adafruit_GFX.h>

class CanvasDisplay : public Adafruit_GFX
{
public:
  CanvasDisplay(int16_t w, int16_t h)
      : Adafruit_GFX(w, h), bytesPerRow_((w + 7) / 8), buffer_(static_cast<size_t>((w + 7) / 8) * h, 0)
  {
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override
  {
    if (x < 0 || y < 0 || x >= width() || y >= height())
    {
      return;
    }
    uint8_t *p = &buffer_[(x / 8) + (y * bytesPerRow_)];
    uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
    if (color == 0)
    {
      *p &= ~mask;
    }
    else if (color == 2)
    {
      *p ^= mask;
    }
    else
    {
      *p |= mask;
    }
  }

  void clearDisplay() { std::fill(buffer_.begin(), buffer_.end(), 0); }
  void display() {}

  const uint8_t *getBuffer() const { return buffer_.data(); }
  int getBytesPerRow() const { return bytesPerRow_; }

private:
  int bytesPerRow_;
  std::vector<uint8_t> buffer_;
};

#endif // SCREENSHOT_STUB_CANVAS_H
