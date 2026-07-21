#ifndef SCREENSHOT_STUB_ARDUINO_H
#define SCREENSHOT_STUB_ARDUINO_H

// Host-side Arduino shim for the native screenshot generator. Richer than the
// core-tests Arduino stub because it must satisfy the real interface.cpp and
// the vendored Adafruit_GFX / U8g2 rendering code: Print, F()/PROGMEM, a
// settable millis(), and the usual math helpers.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include "Print.h"

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#ifndef HALF_PI
#define HALF_PI 1.5707963267948966192313216916398
#endif
#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105
#endif
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)

typedef uint8_t byte;
typedef bool boolean;

// F() / flash-string handling. __FlashStringHelper is forward-declared in
// Print.h; F(x) just tags a plain pointer so print(const __FlashStringHelper*)
// selects the text path.
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(string_literal))

// Settable virtual clock. interface.cpp reads millis() for the portrait
// hysteresis and the LiDAR telemetry age; the fixtures pin it to a constant.
inline unsigned long g_stub_millis = 100000UL;
inline unsigned long millis() { return g_stub_millis; }
inline unsigned long micros() { return g_stub_millis * 1000UL; }
inline void delay(unsigned long) {}

template <typename T>
inline T constrain(T value, T low, T high)
{
  return value < low ? low : (value > high ? high : value);
}
template <typename T>
inline T min(T lhs, T rhs) { return std::min(lhs, rhs); }
template <typename T>
inline T max(T lhs, T rhs) { return std::max(lhs, rhs); }
template <typename T>
inline T abs(T value) { return value < 0 ? -value : value; }

inline char *dtostrf(double value, signed char width, unsigned char precision, char *output)
{
  char format[16] = {0};
  std::snprintf(format, sizeof(format), "%%%d.%df", static_cast<int>(width),
                static_cast<int>(precision));
  std::snprintf(output, 32, format, value);
  return output;
}

class String
{
public:
  String() = default;
  String(const char *value) : value_(value ? value : "") {}
  String(const std::string &value) : value_(value) {}
  String(int value) : value_(std::to_string(value)) {}
  String(unsigned int value) : value_(std::to_string(value)) {}
  String(long value) : value_(std::to_string(value)) {}
  String(unsigned long value) : value_(std::to_string(value)) {}
  const char *c_str() const { return value_.c_str(); }
  unsigned int length() const { return static_cast<unsigned int>(value_.size()); }
  char charAt(unsigned int index) const { return value_[index]; }
  char operator[](unsigned int index) const { return value_[index]; }
  String operator+(const String &rhs) const { return String(value_ + rhs.value_); }
  bool operator==(const String &rhs) const { return value_ == rhs.value_; }

private:
  std::string value_;
};

inline size_t Print::print(const String &value) { return print(value.c_str()); }

#endif // SCREENSHOT_STUB_ARDUINO_H
