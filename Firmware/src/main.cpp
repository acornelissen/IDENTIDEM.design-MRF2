// IDENTIDEM.design Medium Format Rangefinder firmware v10.0.2
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
bool sleepServicesActive = false;
bool sleepWakeBaselinesInitialized = false;
bool lightMeterSleeping = false;
int sleepWakeEncoderBaseline = 0;
int sleepWakeLensBaseline = 0;

bool sendLightMeterCommand(uint8_t command)
{
  Wire.beginTransmission(LIGHTMETER_I2C_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

void powerDownLightMeterForSleep()
{
  if (lightMeterSleeping)
  {
    return;
  }
  sendLightMeterCommand(LIGHTMETER_CMD_POWER_DOWN);
  lightMeterSleeping = true;
}

void wakeLightMeterFromSleep()
{
  if (!lightMeterSleeping)
  {
    return;
  }

  sendLightMeterCommand(LIGHTMETER_CMD_POWER_ON);
  if (!lightMeter.configure(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    lightMeter.begin();
  }
  lightMeterSleeping = false;
}

void initializeSleepWakeBaselines()
{
  sleepWakeEncoderBaseline = encoder.getEncoderPosition();
  sleepWakeLensBaseline = theads.readADC_SingleEnded(LENS_ADC_PIN);
  sleepWakeBaselinesInitialized = true;
}

void pollSleepWakeEncoder()
{
  if (!sleepWakeBaselinesInitialized)
  {
    initializeSleepWakeBaselines();
  }

  int currentEncoder = encoder.getEncoderPosition();
  if (abs(currentEncoder - sleepWakeEncoderBaseline) >= SLEEP_WAKE_ENCODER_DELTA)
  {
    registerActivity();
    sleepWakeEncoderBaseline = currentEncoder;
  }
}

void pollSleepWakeLens()
{
  if (!sleepWakeBaselinesInitialized)
  {
    initializeSleepWakeBaselines();
  }

  int currentLensReading = theads.readADC_SingleEnded(LENS_ADC_PIN);
  if (abs(currentLensReading - sleepWakeLensBaseline) >= SLEEP_WAKE_LENS_DELTA)
  {
    registerActivity();
    sleepWakeLensBaseline = currentLensReading;
  }
}

void enterSleepServices()
{
  toggleLidar(false);
  powerDownLightMeterForSleep();
  drawSleepUI();

  // Keep external sleep text visible while turning the main display fully off.
  display.oled_command(0xAE);

  sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_OFF_R, NEOPIXEL_OFF_G, NEOPIXEL_OFF_B));
  sspixel.show();

  initializeSleepWakeBaselines();
}

void exitSleepServices()
{
  display.oled_command(0xAF);
  wakeLightMeterFromSleep();
  toggleLidar(true);
  sleepWakeBaselinesInitialized = false;

  // Force immediate sensor/UI refresh right after wake.
  scheduler.lastLidarMs = 0;
  scheduler.lastLensMs = 0;
  scheduler.lastMeterMs = 0;
  scheduler.lastBatteryMs = 0;
  scheduler.lastUiMs = 0;
}

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

  if (shouldRunTask(now, scheduler.lastSleepCheckMs, LOOP_SLEEP_CHECK_INTERVAL_MS))
  {
    updateSleepMode(now);
  }

  if (sleepMode)
  {
    if (!sleepServicesActive)
    {
      enterSleepServices();
      sleepServicesActive = true;
    }

    if (shouldRunTask(now, scheduler.lastInputMs, LOOP_SLEEP_INPUT_INTERVAL_MS))
    {
      checkButtons();
    }
    if (shouldRunTask(now, scheduler.lastFilmCounterMs, LOOP_SLEEP_ENCODER_POLL_INTERVAL_MS))
    {
      pollSleepWakeEncoder();
    }
    if (shouldRunTask(now, scheduler.lastLensMs, LOOP_SLEEP_LENS_POLL_INTERVAL_MS))
    {
      pollSleepWakeLens();
    }
  }
  else
  {
    if (sleepServicesActive)
    {
      exitSleepServices();
      sleepServicesActive = false;
    }

    if (shouldRunTask(now, scheduler.lastInputMs, LOOP_INPUT_INTERVAL_MS))
    {
      checkButtons();
    }
    if (shouldRunTask(now, scheduler.lastFilmCounterMs, LOOP_FILM_COUNTER_INTERVAL_MS))
    {
      setFilmCounter();
    }

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
