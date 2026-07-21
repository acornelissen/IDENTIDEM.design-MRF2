#ifndef SCREENSHOT_STUB_BOUNCE2_H
#define SCREENSHOT_STUB_BOUNCE2_H

#include <cstdint>

namespace Bounce2
{
class Button
{
public:
  void attach(int, int) {}
  void interval(uint16_t) {}
  void setPressedState(int) {}
  bool update() { return false; }
  bool pressed() { return false; }
  bool released() { return false; }
  bool read() { return false; }
  bool isPressed() { return false; }
};
} // namespace Bounce2

#endif // SCREENSHOT_STUB_BOUNCE2_H
