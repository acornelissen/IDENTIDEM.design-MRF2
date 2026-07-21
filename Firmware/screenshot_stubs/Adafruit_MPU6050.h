#ifndef SCREENSHOT_STUB_ADAFRUIT_MPU6050_H
#define SCREENSHOT_STUB_ADAFRUIT_MPU6050_H

#include "Adafruit_Sensor.h"

// Injectable accelerometer pose. The fixtures set these globals to place the
// level line: level device for landscape, rolled ~90deg for portrait.
inline float g_stub_accel_x = 0.0f;
inline float g_stub_accel_y = 0.0f;
inline float g_stub_accel_z = 9.81f;

class Adafruit_MPU6050
{
public:
  bool begin(uint8_t = 0x68) { return true; }
  void setAccelerometerRange(int) {}
  void setGyroRange(int) {}
  void setFilterBandwidth(int) {}

  bool getEvent(sensors_event_t *accel, sensors_event_t *gyro, sensors_event_t *temp)
  {
    if (accel)
    {
      accel->acceleration.x = g_stub_accel_x;
      accel->acceleration.y = g_stub_accel_y;
      accel->acceleration.z = g_stub_accel_z;
    }
    (void)gyro;
    (void)temp;
    return true;
  }
};

// Range/bandwidth enum values the real header exposes; unused numerically here.
enum { MPU6050_RANGE_8_G = 0, MPU6050_RANGE_500_DEG = 0, MPU6050_BAND_21_HZ = 0 };

#endif // SCREENSHOT_STUB_ADAFRUIT_MPU6050_H
