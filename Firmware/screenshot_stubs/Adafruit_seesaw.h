#ifndef SCREENSHOT_STUB_ADAFRUIT_SEESAW_H
#define SCREENSHOT_STUB_ADAFRUIT_SEESAW_H

#include <cstdint>

class Adafruit_seesaw
{
public:
  Adafruit_seesaw(void * = nullptr) {}
  bool begin(uint8_t = 0x49, int8_t = -1, bool = true) { return true; }
  uint32_t getVersion() { return 0; }
  int32_t getEncoderPosition() { return 0; }
  void setGPIOInterrupts(uint32_t, bool) {}
  void pinMode(uint8_t, uint8_t) {}
  void enableEncoderInterrupt() {}
};

#endif // SCREENSHOT_STUB_ADAFRUIT_SEESAW_H
