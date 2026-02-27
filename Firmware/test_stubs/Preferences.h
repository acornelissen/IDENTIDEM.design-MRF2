#ifndef TEST_STUB_PREFERENCES_H
#define TEST_STUB_PREFERENCES_H

#include <stdint.h>
#include <stddef.h>

class Preferences
{
public:
  bool begin(const char *, bool) { return true; }
  void end() {}

  int getInt(const char *, int defaultValue = 0) const { return defaultValue; }
  float getFloat(const char *, float defaultValue = 0.0f) const { return defaultValue; }
  bool getBool(const char *, bool defaultValue = false) const { return defaultValue; }
  uint32_t getUInt(const char *, uint32_t defaultValue = 0U) const { return defaultValue; }
  uint16_t getUShort(const char *, uint16_t defaultValue = 0U) const { return defaultValue; }
  size_t getBytesLength(const char *) const { return 0; }
  size_t getBytes(const char *, void *, size_t) const { return 0; }

  size_t putInt(const char *, int) { return sizeof(int); }
  size_t putFloat(const char *, float) { return sizeof(float); }
  size_t putBool(const char *, bool) { return sizeof(bool); }
  size_t putUInt(const char *, uint32_t) { return sizeof(uint32_t); }
  size_t putUShort(const char *, uint16_t) { return sizeof(uint16_t); }
  size_t putBytes(const char *, const void *, size_t len) { return len; }

  bool remove(const char *) { return true; }
};

#endif // TEST_STUB_PREFERENCES_H
