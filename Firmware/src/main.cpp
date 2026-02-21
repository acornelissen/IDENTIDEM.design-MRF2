// IDENTIDEM.design Medium Format Rangefinder firmware v9.7.5
// Hardware: DTS6012M LiDAR, STEMMA I2C QT Rotary Encoder (4991), SH1107 main + SSD1306 external OLEDs

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_bt.h"

// Constants and variables
#include "mrfconstants.h"
#include "globals.h"

// Init hardware
#include "hardware.h"

// Functions
#include "helpers.h"
#include "setfuncs.h"
#include "interface.h"
#include "inputs.h"
#include "activity.h"

namespace
{
struct LoopScheduler
{
  bool initialized = false;
  unsigned long lastInputMs = 0;
  unsigned long lastFilmCounterMs = 0;
  unsigned long lastSleepCheckMs = 0;
  unsigned long lastLidarMs = 0;
  unsigned long lastLensMs = 0;
  unsigned long lastMeterMs = 0;
  unsigned long lastBatteryMs = 0;
  unsigned long lastUiMs = 0;
  unsigned long lastPrefsFlushMs = 0;
};

LoopScheduler scheduler;

bool shouldRunTask(unsigned long nowMs, unsigned long &lastRunMs, unsigned long intervalMs)
{
  if ((nowMs - lastRunMs) < intervalMs)
  {
    return false;
  }

  lastRunMs = nowMs;
  return true;
}
} // namespace

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

  // Clear the moving average values.
  for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++)
  {
    samples[i] = 0;
  }
  curReadIndex = 0;
  sampleTotal = 0;
  sampleAvg = 0;

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

  // Treat end-of-setup as the idle timer baseline.
  lastActivityTime = millis();
  sleepMode = false;
}

void loop()
{
  const unsigned long now = millis();
  if (!scheduler.initialized)
  {
    scheduler.initialized = true;
    scheduler.lastInputMs = 0;
    scheduler.lastFilmCounterMs = 0;
    scheduler.lastSleepCheckMs = 0;
    scheduler.lastLidarMs = 0;
    scheduler.lastLensMs = 0;
    scheduler.lastMeterMs = 0;
    scheduler.lastBatteryMs = 0;
    scheduler.lastUiMs = 0;
    scheduler.lastPrefsFlushMs = 0;
  }

  if (shouldRunTask(now, scheduler.lastInputMs, LOOP_INPUT_INTERVAL_MS))
  {
    checkButtons();
  }
  if (shouldRunTask(now, scheduler.lastFilmCounterMs, LOOP_FILM_COUNTER_INTERVAL_MS))
  {
    setFilmCounter();
  }
  if (shouldRunTask(now, scheduler.lastSleepCheckMs, LOOP_SLEEP_CHECK_INTERVAL_MS))
  {
    updateSleepMode(now);
  }

  if (sleepMode)
  {
    toggleLidar(false);
    if (shouldRunTask(now, scheduler.lastUiMs, LOOP_UI_INTERVAL_MS))
    {
      drawSleepUI();
    }
  }
  else
  {
    toggleLidar(true);

    if (shouldRunTask(now, scheduler.lastLidarMs, LOOP_LIDAR_INTERVAL_MS))
    {
      setDistance();
    }
    if (shouldRunTask(now, scheduler.lastLensMs, LOOP_LENS_INTERVAL_MS))
    {
      lens_sensor_reading = getLensSensorReading();
      setLensDistance();
    }
    if (shouldRunTask(now, scheduler.lastMeterMs, LOOP_LIGHTMETER_INTERVAL_MS))
    {
      setLightMeter();
    }
    if (shouldRunTask(now, scheduler.lastBatteryMs, LOOP_BATTERY_INTERVAL_MS))
    {
      setVoltage();
    }

    if (shouldRunTask(now, scheduler.lastUiMs, LOOP_UI_INTERVAL_MS))
    {
      if (ui_mode == UiMode::Main)
      {
        drawMainUI();
      }
      else if (ui_mode == UiMode::Config)
      {
        drawConfigUI();
      }
      else if (ui_mode == UiMode::ConfigFilm)
      {
        drawFilmConfigUI();
      }
      else if (ui_mode == UiMode::ConfigLens)
      {
        drawLensConfigUI();
      }
      else if (ui_mode == UiMode::ConfigMeter)
      {
        drawMeterConfigUI();
      }
      else if (ui_mode == UiMode::Calib)
      {
        drawCalibUI();
      }
      else if (ui_mode == UiMode::ResetConfirm)
      {
        drawResetConfirmUI();
      }
      else if (ui_mode == UiMode::Health)
      {
        drawHealthUI();
      }
      drawExternalUI();
    }
  }

  if (shouldRunTask(now, scheduler.lastPrefsFlushMs, LOOP_PREFS_FLUSH_INTERVAL_MS))
  {
    flushPrefsIfDirty();
  }
}
// ---------------------
