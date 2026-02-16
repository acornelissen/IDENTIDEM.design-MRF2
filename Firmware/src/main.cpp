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

  Serial.begin(SERIAL_BAUD_RATE); // Initializing serial port

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
  lbutton.attach(BUTTON_LEFT_PIN, INPUT_PULLUP);
  lbutton.interval(BUTTON_DEBOUNCE_MS);
  lbutton.setPressedState(LOW);
  rbutton.attach(BUTTON_RIGHT_PIN, INPUT_PULLUP);
  rbutton.interval(BUTTON_DEBOUNCE_MS);
  rbutton.setPressedState(LOW);

  // Initialize lastActivityTime here, as globals are initialized before millis() is reliable.
  lastActivityTime = millis();

  delay(DISPLAY_INIT_DELAY_MS);               // Slight delay or the displays won't work
  display.begin(SCREEN_ADDRESS, true); // Default display address
  display.oled_command(DISPLAY_COMMAND_FLIP);
  display.setRotation(DISPLAY_ROTATION);
  u8g2.begin(display);
  display.clearDisplay();
  display.display();

  delay(DISPLAY_EXT_INIT_DELAY_MS); // Slight delay or the displays won't work

  display_ext.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_EXT);

  // Boot up sceen
  display_ext.clearDisplay();
  display_ext.setTextSize(DISPLAY_BOOT_TEXT_SIZE); // Draw 2X-scale text
  display_ext.setTextColor(SSD1306_WHITE);
  display_ext.setCursor(DISPLAY_BOOT_CURSOR_X, DISPLAY_BOOT_CURSOR_Y);
  display_ext.print(F("MRF "));
  display_ext.println(FWVERSION);
  display_ext.display();

  delay(DISPLAY_BOOT_SCREEN_MS);

  u8g2_ext.begin(display_ext);
  display_ext.clearDisplay();
  display_ext.display();

  // Start the LiDAR sensor using the v2 initialization interface.
  DTSResult lidarInit = lidar.begin(LIDAR_BAUD_RATE, RXD2, TXD2);
  if (lidarInit != DTSError::NONE)
  {
    lidarEnabled = false;
    Serial.print(F("LiDAR init error: "));
    Serial.println(static_cast<int>(static_cast<DTSError>(lidarInit)));
  }
  else
  {
    // Stage 1 correction: global linear scale/offset in library space (mm).
    lidar.setDistanceScale(LIDAR_LIBRARY_DISTANCE_SCALE);
    lidar.setDistanceOffset(LIDAR_LIBRARY_DISTANCE_OFFSET_MM);
  }
  delay(LIDAR_SERIAL_STARTUP_DELAY_MS);

  // Clear the moving average arrays
  for (int channel = 0; channel < SENSOR_CHANNEL_COUNT; channel++)
  {
    for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++)
    {
      samples[channel][i] = 0;
    }
  }

  // Start the battery gauge and lightmeter
  maxlipo.begin();
  lightMeter.begin();

  // Seesaw NEOpixel
  if (sspixel.begin(SEESAW_ADDR))
  {
    delay(SEESAW_INIT_DELAY_MS);
    sspixel.setBrightness(SEESAW_NEOPIXEL_BRIGHTNESS);
    sspixel.show();
  }

  // Start the encoder
  if (encoder.begin(SEESAW_ADDR))
  {
    delay(SEESAW_INIT_DELAY_MS);
    encoder.setEncoderPosition(encoder_value);
    encoder.enableEncoderInterrupt();
  }
}

void loop()
{
  checkButtons();
  setFilmCounter();

  if (millis() - lastActivityTime > SLEEPTIMEOUT)
  {
    sleepMode = true;
  }

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
      drawMainUI();
    }
    else if (ui_mode == "config")
    {
      drawConfigUI();
    }
    else if (ui_mode == "calib")
    {
      drawCalibUI();
    }
    else if (ui_mode == "reset_confirm")
    {
      drawResetConfirmUI();
    }
    drawExternalUI();
  }

  lens_sensor_reading = getLensSensorReading();
  setLensDistance();
}
// ---------------------
