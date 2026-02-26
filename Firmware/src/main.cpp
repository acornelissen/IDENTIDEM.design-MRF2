// IDENTIDEM.design Medium Format Rangefinder firmware v10.1.3
// Hardware: DTS6012M LiDAR, STEMMA I2C QT Rotary Encoder (4991), SH1107 main + SSD1306 external OLEDs

#include <Arduino.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_bt.h"

#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "loop_runtime.h"
#include "mrfconstants.h"

namespace
{
void disableWirelessRadios()
{
  esp_wifi_stop();            // Stop WiFi to save power.
  esp_bt_controller_disable(); // Stop Bluetooth to save power.
}

void configureButton(Bounce2::Button &button, int pin)
{
  button.attach(pin, INPUT_PULLUP);
  button.interval(BUTTON_DEBOUNCE_MS);
  button.setPressedState(LOW);
}

void initializeInputHardware()
{
  theads.begin();
  mpu.begin();

  theads.setDataRate(RATE_ADS1015_128SPS);
  theads.setGain(GAIN_TWOTHIRDS);

  configureButton(lbutton, BUTTON_LEFT_PIN);
  configureButton(rbutton, BUTTON_RIGHT_PIN);
}

void initializeMainDisplay()
{
  delay(DISPLAY_INIT_DELAY_MS); // Slight delay or the displays won't work.

  display.begin(SCREEN_ADDRESS, true);
  display.oled_command(DISPLAY_COMMAND_FLIP);
  display.setRotation(DISPLAY_ROTATION);
  u8g2.begin(display);
  display.clearDisplay();
  display.display();

  delay(DISPLAY_EXT_INIT_DELAY_MS); // Slight delay or the displays won't work.
  display_ext.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_EXT);
}

void showBootScreenOnExternalDisplay()
{
  display_ext.clearDisplay();
  display_ext.setTextSize(DISPLAY_BOOT_TEXT_SIZE);
  display_ext.setTextColor(SSD1306_WHITE);

  char bootText[24];
  snprintf(bootText, sizeof(bootText), "MRF %s", FWVERSION);

  int16_t bootTextX1 = 0;
  int16_t bootTextY1 = 0;
  uint16_t bootTextWidth = 0;
  uint16_t bootTextHeight = 0;
  display_ext.getTextBounds(bootText, 0, 0, &bootTextX1, &bootTextY1, &bootTextWidth, &bootTextHeight);

  int16_t bootCursorX = ((display_ext.width() - static_cast<int16_t>(bootTextWidth)) / 2) - bootTextX1;
  int16_t bootCursorY = ((display_ext.height() - static_cast<int16_t>(bootTextHeight)) / 2) - bootTextY1;
  display_ext.setCursor(bootCursorX, bootCursorY);
  display_ext.print(bootText);
  display_ext.display();

  delay(DISPLAY_BOOT_SCREEN_MS);

  u8g2_ext.begin(display_ext);
  display_ext.clearDisplay();
  display_ext.display();
}

void initializeLidarSensor()
{
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
}

void resetLensMovingAverageState()
{
  for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++)
  {
    samples[i] = 0;
  }
  curReadIndex = 0;
  sampleTotal = 0;
  sampleAvg = 0;
}

void initializePowerAndInputPeripherals()
{
  maxlipo.begin();
  lightMeter.begin();

  if (sspixel.begin(SEESAW_ADDR))
  {
    delay(SEESAW_INIT_DELAY_MS);
    sspixel.setBrightness(SEESAW_NEOPIXEL_BRIGHTNESS);
    sspixel.show();
  }

  if (encoder.begin(SEESAW_ADDR))
  {
    delay(SEESAW_INIT_DELAY_MS);
    encoder.setEncoderPosition(encoder_value);
    encoder.enableEncoderInterrupt();
  }
}
} // namespace

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);

  disableWirelessRadios();
  loadPrefs();
  initializeInputHardware();

  // Initialize here because globals are set before millis() is reliable.
  lastActivityTime = millis();

  initializeMainDisplay();
  showBootScreenOnExternalDisplay();
  initializeLidarSensor();
  resetLensMovingAverageState();
  initializePowerAndInputPeripherals();

  // Treat end-of-setup as the idle timer baseline.
  lastActivityTime = millis();
  sleepMode = false;
}

void loop()
{
  runLoopRuntimeIteration(millis());
}
