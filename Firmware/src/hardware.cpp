#include "hardware.h"
#include "mrfconstants.h" // For pin/address constants and screen dimensions

#include <Arduino.h> // For HardwareSerial
#include <Wire.h>    // For I2C devices & display

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h> // Correct header for seesaw_NeoPixel
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_MAX1704X.h>
#include <BH1750.h>
#include <DTS6012M_UART.h>
#include <Adafruit_SH110X.h> // For SH1107
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Bounce2.h>

// Hardware init
// ---------------------
// Inputs
Adafruit_seesaw encoder;
Adafruit_ADS1015 theads;
Adafruit_MPU6050 mpu;
seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX, NEO_GRB + NEO_KHZ800);
Bounce2::Button lbutton = Bounce2::Button();
Bounce2::Button rbutton = Bounce2::Button();

// Battery gauge
Adafruit_MAX17048 maxlipo;
// Lightmeter
BH1750 lightMeter;
// LiDAR setup
HardwareSerial lidarSerial(2); // Using serial port 2
DTS6012M_UART lidar(lidarSerial);
// Display setup
Adafruit_SH1107 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 1000000);
Adafruit_SSD1306 display_ext(SCREEN_WIDTH, SCREEN_HEIGHT_EXT, &Wire, OLED_RESET);
U8G2_FOR_ADAFRUIT_GFX u8g2;
U8G2_FOR_ADAFRUIT_GFX u8g2_ext;
// ---------------------