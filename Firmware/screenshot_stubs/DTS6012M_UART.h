#ifndef SCREENSHOT_STUB_DTS6012M_UART_H
#define SCREENSHOT_STUB_DTS6012M_UART_H

#include <cstdint>

// Mirrors the enums/structs the pure lidar_logic module consumes, plus an inert
// DTS6012M_UART class so hardware.h's `extern DTS6012M_UART lidar;` can be
// defined in the generator. Kept in sync with test_stubs/DTS6012M_UART.h.
enum class DTSError : uint8_t
{
  NONE = 0x00,
  SERIAL_INIT_FAILED = 0x01,
  FRAME_HEADER_INVALID = 0x02,
  FRAME_LENGTH_INVALID = 0x03,
  CRC_CHECK_FAILED = 0x04,
  BUFFER_OVERFLOW = 0x05,
  TIMEOUT = 0x06,
  INVALID_COMMAND = 0x07,
  UNSUPPORTED_OPERATION = 0x08,
  NO_NEW_DATA = 0x09
};

enum class DataQuality : uint8_t
{
  EXCELLENT = 0,
  GOOD = 1,
  FAIR = 2,
  POOR = 3,
  INVALID = 4
};

struct DTSMeasurement
{
  uint16_t primaryDistance_mm = 0;
  uint16_t primaryIntensity = 0;
  uint16_t primaryCorrection = 0;
  uint16_t secondaryDistance_mm = 0;
  uint16_t secondaryIntensity = 0;
  uint16_t temperatureCode = 0;
  uint16_t sunlightBase = 0;
  unsigned long timestamp = 0;
  DataQuality primaryQuality = DataQuality::INVALID;
  DataQuality secondaryQuality = DataQuality::INVALID;
  DTSError lastError = DTSError::NONE;
};

constexpr uint16_t DTS_INVALID_DISTANCE = 0xFFFF;
constexpr uint16_t DTS_INVALID_INTENSITY = 0x0000;

class DTS6012M_UART
{
public:
  DTS6012M_UART(void * = nullptr) {}
  DTSError update() { return DTSError::NONE; }
  DTSMeasurement getMeasurement() { return DTSMeasurement{}; }
};

#endif // SCREENSHOT_STUB_DTS6012M_UART_H
