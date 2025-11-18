// IDENTIDEM.design Medium Format Rangefinder firmware v7.0
// Hardware: DTS6012M LiDAR, STEMMA I2C QT Rotary Encoder (4991), SH1107 main + SSD1306 external OLEDs

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
#include <Preferences.h>
#include <math.h>

#include "driver/rtc_io.h"
#include "esp_wifi.h"
#include "esp_bt.h"

// Constants and variables
#include "mrfconstants.h"
#include "lenses.h"
#include "formats.h"
#include "globals.h"

// Init hardware
#include "hardware.h"

// Functions
#include "helpers.h"
#include "cyclefuncs.h"
#include "setfuncs.h"
#include "interface.h"
#include "inputs.h"

// Setup and loop functions
// ---------------------
void setup()
{

  Serial.begin(115200); // Initializing serial port

  esp_wifi_stop(); // Stop WiFi to save power
  // esp_bt_controller_disable(); // This function might not exist, esp_bt_disable() is more common.
  // Or ensure esp_bt_controller_deinit() is paired with esp_bt_controller_init if that's the API used.
  esp_bt_controller_disable(); // Stop Bluetooth to save power

  loadPrefs();

  // Initialise inputs
  theads.begin();
  mpu.begin();

  theads.setDataRate(RATE_ADS1015_128SPS);
  theads.setGain(GAIN_TWOTHIRDS);
  lbutton.attach(10, INPUT_PULLUP);
  lbutton.interval(5);
  lbutton.setPressedState(LOW);
  rbutton.attach(9, INPUT_PULLUP);
  rbutton.interval(5);
  rbutton.setPressedState(LOW);

  // Initialize lastActivityTime here, as globals are initialized before millis() is reliable.
  lastActivityTime = millis();

  delay(1000);               // Slight delay or the displays won't work
  display.begin(0x3D, true); // Address 0x3D default
  display.oled_command(0xC8);
  display.setRotation(3);
  u8g2.begin(display);
  display.clearDisplay();
  display.display();

  delay(500); // Slight delay or the displays won't work

  display_ext.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_EXT);

  // Boot up sceen
  display_ext.clearDisplay();
  display_ext.setTextSize(2); // Draw 2X-scale text
  display_ext.setTextColor(SSD1306_WHITE);
  display_ext.setCursor(20, 10);
  display_ext.print(F("MRF "));
  display_ext.println(FWVERSION);
  display_ext.display();

  delay(1000);

  u8g2_ext.begin(display_ext);
  display_ext.clearDisplay();
  display_ext.display();

  // Start the LiDAR sensor
  lidarSerial.begin(921600, SERIAL_8N1, RXD2, TXD2);
  delay(20);
  lidar.begin();

  // Clear the moving average arrays
  for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++)
  {
    samples[0][i] = 0;
    samples[1][i] = 0;
  }

  // Start the battery gauge and lightmeter
  maxlipo.begin();
  lightMeter.begin();

  // Seesaw NEOpixel
  if (sspixel.begin(SEESAW_ADDR))
  {
    delay(10);
    sspixel.setBrightness(80);
    sspixel.show();
  }

  // Start the encoder
  if (encoder.begin(SEESAW_ADDR))
  {
    delay(10);
    encoder.setEncoderPosition(encoder_value);
    encoder.enableEncoderInterrupt();
  }
}

void loop()
{

  if (millis() - lastActivityTime > SLEEPTIMEOUT)
  {
    sleepMode = true;
  }

  checkButtons();
  setFilmCounter();

  if (sleepMode == true)
  {
    toggleLidar(false);
    drawSleepUI();
  }
  else
  {
    toggleLidar(true);
    setDistance();
    setVoltage();
    setLightMeter();

    if (ui_mode == "main")
    {
      setLensDistance();
      drawMainUI();
    }
    else if (ui_mode == "config")
    {
      drawConfigUI();
    }
    else if (ui_mode == "calib")
    {
      lens_sensor_reading = getLensSensorReading() - LENS_CALIB_OFFSET;
      drawCalibUI();
    }
    drawExternalUI();
  }
}
// ---------------------
