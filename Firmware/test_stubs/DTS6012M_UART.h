#ifndef TEST_STUB_DTS6012M_UART_H
#define TEST_STUB_DTS6012M_UART_H

#include <cstdint>

enum class DataQuality : uint8_t
{
  INVALID = 0,
  POOR = 1,
  FAIR = 2,
  GOOD = 3,
  EXCELLENT = 4
};

struct DTSMeasurement
{
  uint16_t primaryDistance_mm = 0;
  uint16_t secondaryDistance_mm = 0;
  uint16_t primaryIntensity = 0;
  uint16_t secondaryIntensity = 0;
  DataQuality primaryQuality = DataQuality::INVALID;
  DataQuality secondaryQuality = DataQuality::INVALID;
};

constexpr uint16_t DTS_INVALID_DISTANCE = 0xFFFF;

#endif // TEST_STUB_DTS6012M_UART_H
