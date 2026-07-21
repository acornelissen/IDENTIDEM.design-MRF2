#ifndef SCREENSHOT_STUB_WIRE_H
#define SCREENSHOT_STUB_WIRE_H

#include <cstdint>

// Just enough of TwoWire for the display/sensor stub constructors to accept a
// &Wire argument. Nothing here does any I/O.
class TwoWire
{
public:
  void begin() {}
  void setClock(uint32_t) {}
  bool begin(int) { return true; }
};

extern TwoWire Wire;

#endif // SCREENSHOT_STUB_WIRE_H
