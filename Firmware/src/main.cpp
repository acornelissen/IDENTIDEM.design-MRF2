// IDENTIDEM.design Medium Format Rangefinder firmware v10.2.1
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
#include "setfuncs.h"

namespace
{
void logPeripheralInitStatus(const __FlashStringHelper *name, bool ok)
{
  Serial.print(name);
  Serial.print(F(": "));
  Serial.println(ok ? F("OK") : F("FAILED"));
}

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
  adsReady = theads.begin();
  logPeripheralInitStatus(F("ADS1015"), adsReady);
  mpuReady = mpu.begin();
  logPeripheralInitStatus(F("MPU6050"), mpuReady);

  if (adsReady)
  {
    theads.setDataRate(RATE_ADS1015_128SPS);
    theads.setGain(GAIN_TWOTHIRDS);
  }

  configureButton(lbutton, BUTTON_LEFT_PIN);
  configureButton(rbutton, BUTTON_RIGHT_PIN);
}

void initializeMainDisplay()
{
  delay(DISPLAY_INIT_DELAY_MS); // Slight delay or the displays won't work.

  mainDisplayReady = display.begin(SCREEN_ADDRESS, true);
  logPeripheralInitStatus(F("Main OLED"), mainDisplayReady);
  if (mainDisplayReady)
  {
    display.oled_command(DISPLAY_COMMAND_FLIP);
    display.setRotation(DISPLAY_ROTATION);
    u8g2.begin(display);
    display.clearDisplay();
    display.display();
  }

  delay(DISPLAY_EXT_INIT_DELAY_MS); // Slight delay or the displays won't work.
  externalDisplayReady = display_ext.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_EXT);
  logPeripheralInitStatus(F("Ext OLED"), externalDisplayReady);
}

void showBootScreenOnExternalDisplay()
{
  if (!externalDisplayReady)
  {
    return;
  }

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
    lidarSensorReady = false;
    lidarEnabled = false;
    Serial.print(F("LiDAR init error: "));
    Serial.println(static_cast<int>(static_cast<DTSError>(lidarInit)));
  }
  else
  {
    lidarSensorReady = true;
    lidarEnabled = true;
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
  batteryGaugeReady = maxlipo.begin();
  if (!batteryGaugeReady)
  {
    // maxlipo.begin() fails when the Seesaw (also at I2C address 0x36) ACKs
    // the soft-reset write that MAX17048 deliberately NACKs as part of its
    // initialisation sequence. i2c_dev is still valid at this point.
    // The Seesaw protocol uses two-byte register addresses so it does not
    // respond to the single-byte register reads that MAX17048 uses; SOC reads
    // therefore return accurate data despite the shared address. Confirm the
    // gauge is present by checking that a direct SOC read is plausible.
    float testPct = maxlipo.cellPercent();
    batteryGaugeReady = (testPct >= 0.0f && testPct <= 100.0f);
  }
  logPeripheralInitStatus(F("MAX17048"), batteryGaugeReady);

  lightMeterReady = lightMeter.begin();
  logPeripheralInitStatus(F("BH1750"), lightMeterReady);

  statusPixelReady = sspixel.begin(SEESAW_ADDR);
  logPeripheralInitStatus(F("Seesaw Pixel"), statusPixelReady);
  if (statusPixelReady)
  {
    delay(SEESAW_INIT_DELAY_MS);
    sspixel.setBrightness(SEESAW_NEOPIXEL_BRIGHTNESS);
    sspixel.show();
  }

  encoderReady = encoder.begin(SEESAW_ADDR);
  logPeripheralInitStatus(F("Seesaw Encoder"), encoderReady);
  if (encoderReady)
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
  setVoltage(); // Populate bat_per before the first loop tick.

  // Treat end-of-setup as the idle timer baseline.
  lastActivityTime = millis();
  sleepMode = false;
}

void loop()
{
  runLoopRuntimeIteration(millis());
}
