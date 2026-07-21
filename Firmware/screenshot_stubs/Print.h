#ifndef SCREENSHOT_STUB_PRINT_H
#define SCREENSHOT_STUB_PRINT_H

// Minimal Arduino Print base class for the native screenshot generator.
// Adafruit_GFX and U8G2_FOR_ADAFRUIT_GFX both derive from Print and provide
// their own write(uint8_t); the print()/println() overloads here funnel every
// value through that single glyph-drawing sink, exactly as the real core does.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class __FlashStringHelper;
class String;

class Print
{
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size)
  {
    size_t n = 0;
    while (size--)
    {
      if (write(*buffer++))
      {
        n++;
      }
      else
      {
        break;
      }
    }
    return n;
  }

  size_t write(const char *str)
  {
    if (!str)
    {
      return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(str), strlen(str));
  }

  size_t print(const char *value) { return value ? write(value) : 0; }
  size_t print(char value) { return write(static_cast<uint8_t>(value)); }
  size_t print(const __FlashStringHelper *value)
  {
    return print(reinterpret_cast<const char *>(value));
  }
  size_t print(const String &value);

  size_t print(int value) { return printNumber("%d", value); }
  size_t print(unsigned int value) { return printNumber("%u", value); }
  size_t print(long value) { return printNumber("%ld", value); }
  size_t print(unsigned long value) { return printNumber("%lu", value); }

  size_t print(double value, int digits = 2)
  {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
    return write(buffer);
  }

private:
  template <typename T>
  size_t printNumber(const char *format, T value)
  {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), format, value);
    return write(buffer);
  }
};

#endif // SCREENSHOT_STUB_PRINT_H
