#ifndef TEST_STUB_ARDUINO_H
#define TEST_STUB_ARDUINO_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

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
  String(float value, unsigned char decimals = 2) : value_(formatFloat(value, decimals)) {}
  String(double value, unsigned char decimals = 2) : value_(formatFloat(value, decimals)) {}

  String(const String &) = default;
  String(String &&) = default;
  String &operator=(const String &) = default;
  String &operator=(String &&) = default;
  String &operator=(const char *value)
  {
    value_ = value ? value : "";
    return *this;
  }

  const char *c_str() const { return value_.c_str(); }

  String operator+(const String &rhs) const { return String(value_ + rhs.value_); }
  String operator+(const char *rhs) const { return String(value_ + (rhs ? rhs : "")); }

  bool operator==(const String &rhs) const { return value_ == rhs.value_; }
  bool operator==(const char *rhs) const { return value_ == (rhs ? rhs : ""); }
  bool operator!=(const String &rhs) const { return !(*this == rhs); }
  bool operator!=(const char *rhs) const { return !(*this == rhs); }

private:
  static std::string formatFloat(double value, unsigned char decimals)
  {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream << std::setprecision(static_cast<int>(decimals)) << value;
    return stream.str();
  }

  std::string value_;
};

inline String operator+(const char *lhs, const String &rhs)
{
  return String(lhs ? lhs : "") + rhs;
}

template <typename T>
inline T constrain(T value, T low, T high)
{
  if (value < low)
  {
    return low;
  }
  if (value > high)
  {
    return high;
  }
  return value;
}

template <typename T>
inline T min(T lhs, T rhs)
{
  return std::min(lhs, rhs);
}

template <typename T>
inline T max(T lhs, T rhs)
{
  return std::max(lhs, rhs);
}

inline char *dtostrf(double value, signed char width, unsigned char precision, char *output)
{
  char format[16] = {0};
  std::snprintf(
      format,
      sizeof(format),
      "%%%d.%df",
      static_cast<int>(width),
      static_cast<int>(precision));
  std::snprintf(output, 32, format, value);
  return output;
}

#endif // TEST_STUB_ARDUINO_H
