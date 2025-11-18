#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MAX1704X.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DTS6012M_UART.h>
#include <BH1750.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Bounce2.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

// Hardware init
// ---------------------
// Inputs
extern Adafruit_seesaw encoder;
extern Adafruit_ADS1015 theads;
extern Adafruit_MPU6050 mpu;
extern seesaw_NeoPixel sspixel;
extern Bounce2::Button lbutton;
extern Bounce2::Button rbutton;

// Battery gauge
extern Adafruit_MAX17048 maxlipo;
// Lightmeter
extern BH1750 lightMeter;
// LiDAR setup
extern DTS6012M_UART lidar;
extern HardwareSerial lidarSerial; // Using serial port 2
// Display setup
extern Adafruit_SH1107 display;
extern Adafruit_SSD1306 display_ext;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;
extern U8G2_FOR_ADAFRUIT_GFX u8g2_ext;
// ---------------------

#endif // HARDWARE_H