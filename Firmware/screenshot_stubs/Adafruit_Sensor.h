#ifndef SCREENSHOT_STUB_ADAFRUIT_SENSOR_H
#define SCREENSHOT_STUB_ADAFRUIT_SENSOR_H

// Only the fields interface.cpp reads off a motion event. The generator poses
// the accelerometer via the Adafruit_MPU6050 stub, which fills these in.
struct sensors_vec_t
{
  float x;
  float y;
  float z;
};

struct sensors_event_t
{
  sensors_vec_t acceleration;
  sensors_vec_t gyro;
  float temperature;
};

#endif // SCREENSHOT_STUB_ADAFRUIT_SENSOR_H
