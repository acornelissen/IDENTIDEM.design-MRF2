#ifndef SCREENSHOT_STUB_ADAFRUIT_ADS1X15_H
#define SCREENSHOT_STUB_ADAFRUIT_ADS1X15_H

#include <cstdint>

class Adafruit_ADS1015
{
public:
  bool begin(uint8_t = 0x48) { return true; }
  void setGain(int) {}
  int16_t readADC_SingleEnded(uint8_t) { return 0; }
};

enum { GAIN_ONE = 0 };

#endif // SCREENSHOT_STUB_ADAFRUIT_ADS1X15_H
