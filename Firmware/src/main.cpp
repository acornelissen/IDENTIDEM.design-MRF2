// IDENTIDEM.design Medium Format Rangefinder firmware v10.1.3
// Hardware: DTS6012M LiDAR, STEMMA I2C QT Rotary Encoder (4991), SH1107 main + SSD1306 external OLEDs

#include <Arduino.h>
#include <math.h>
#include <string.h>
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

struct UiRenderCache
{
  bool initialized = false;
  UiMode lastMode = UiMode::Main;
  uint32_t mainSignature = 0;
  uint32_t menuSignature = 0;
  uint32_t externalSignature = 0;
  unsigned long lastMainDrawMs = 0;
  unsigned long lastHealthDrawMs = 0;
};

LoopScheduler scheduler;
UiRenderCache uiRenderCache;
bool sleepServicesActive = false;
bool sleepWakeBaselinesInitialized = false;
bool lightMeterSleeping = false;
bool lidarIdleStandbyActive = false;
int sleepWakeEncoderBaseline = 0;
int sleepWakeLensBaseline = 0;
unsigned long lastFilmMovementMs = 0;
unsigned long lastLensMovementMs = 0;
unsigned long lastMeterChangeMs = 0;

constexpr uint32_t HASH_OFFSET_BASIS = 2166136261u;
constexpr uint32_t HASH_PRIME = 16777619u;

uint32_t hashUint32(uint32_t hash, uint32_t value)
{
  hash ^= value;
  hash *= HASH_PRIME;
  return hash;
}

uint32_t hashInt(uint32_t hash, int value)
{
  return hashUint32(hash, static_cast<uint32_t>(value));
}

uint32_t hashBool(uint32_t hash, bool value)
{
  return hashUint32(hash, value ? 1u : 0u);
}

uint32_t hashFloat(uint32_t hash, float value)
{
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return hashUint32(hash, bits);
}

uint32_t hashString(uint32_t hash, const String &value)
{
  const char *raw = value.c_str();
  size_t len = strlen(raw);
  hash = hashUint32(hash, static_cast<uint32_t>(len));
  for (size_t i = 0; i < len; i++)
  {
    hash = hashUint32(hash, static_cast<uint8_t>(raw[i]));
  }
  return hash;
}

uint32_t buildMainUiSignature()
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, static_cast<int>(ui_mode));
  hash = hashInt(hash, selected_lens);
  hash = hashInt(hash, selected_format);
  hash = hashInt(hash, iso);
  hash = hashFloat(hash, aperture);
  hash = hashBool(hash, show_ev_readout);
  hash = hashFloat(hash, ev_readout);
  hash = hashString(hash, shutter_speed);
  hash = hashString(hash, distance_cm);
  hash = hashString(hash, lens_distance_cm);
  hash = hashInt(hash, distance);
  hash = hashInt(hash, lens_distance_raw);
  hash = hashInt(hash, lidar_quality_level);
  hash = hashBool(hash, parallaxEnabled);
  return hash;
}

uint32_t buildMenuUiSignature()
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, static_cast<int>(ui_mode));
  hash = hashInt(hash, config_step);
  hash = hashInt(hash, calib_step);
  hash = hashInt(hash, selected_lens);
  hash = hashInt(hash, selected_format);
  hash = hashInt(hash, calib_lens);
  hash = hashInt(hash, current_calib_distance);
  hash = hashInt(hash, calib_capture_status);
  hash = hashInt(hash, lens_sensor_reading);
  hash = hashInt(hash, iso);
  hash = hashFloat(hash, aperture);
  hash = hashInt(hash, exposure_comp_thirds);
  hash = hashInt(hash, meter_smoothing_mode);
  hash = hashBool(hash, show_ev_readout);
  hash = hashBool(hash, parallaxEnabled);
  hash = hashInt(hash, sleep_timeout_mode);
  hash = hashInt(hash, level_trim_landscape_deci_deg);
  hash = hashInt(hash, level_trim_portrait_pos_deci_deg);
  hash = hashInt(hash, level_trim_portrait_neg_deci_deg);
  hash = hashInt(hash, frame_one_offset);
  hash = hashInt(hash, frame_spacing_offset);
  hash = hashInt(hash, film_counter);
  hash = hashInt(hash, last_lidar_error_code);
  hash = hashInt(hash, lidar_recovery_count);
  hash = hashBool(hash, lidarEnabled);
  hash = hashBool(hash, prefsSchemaValid);
  hash = hashBool(hash, prefsLoadedLegacy);
  hash = hashInt(hash, prefsSchemaVersionLoaded);
  return hash;
}

uint32_t buildExternalUiSignature()
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, selected_format);
  hash = hashInt(hash, selected_lens);
  hash = hashInt(hash, bat_per);
  hash = hashInt(hash, film_counter);
  hash = hashInt(hash, static_cast<int>(frame_progress * 1000.0f));
  hash = hashBool(hash, sleepMode);
  return hash;
}

bool shouldDrawPrimaryUi(unsigned long nowMs)
{
  if (ui_mode == UiMode::Main)
  {
    uint32_t signature = buildMainUiSignature();
    bool changed = !uiRenderCache.initialized ||
                   uiRenderCache.lastMode != ui_mode ||
                   signature != uiRenderCache.mainSignature;
    bool refreshDue = (nowMs - uiRenderCache.lastMainDrawMs) >= LOOP_UI_MAIN_REFRESH_MS;
    if (!changed && !refreshDue)
    {
      return false;
    }

    uiRenderCache.mainSignature = signature;
    uiRenderCache.lastMainDrawMs = nowMs;
    return true;
  }

  uint32_t signature = buildMenuUiSignature();
  bool changed = !uiRenderCache.initialized ||
                 uiRenderCache.lastMode != ui_mode ||
                 signature != uiRenderCache.menuSignature;
  bool healthRefreshDue = (ui_mode == UiMode::Health) &&
                          ((nowMs - uiRenderCache.lastHealthDrawMs) >= LOOP_UI_HEALTH_REFRESH_MS);
  if (!changed && !healthRefreshDue)
  {
    return false;
  }

  uiRenderCache.menuSignature = signature;
  if (ui_mode == UiMode::Health)
  {
    uiRenderCache.lastHealthDrawMs = nowMs;
  }
  return true;
}

bool shouldDrawExternalUi()
{
  uint32_t signature = buildExternalUiSignature();
  bool changed = !uiRenderCache.initialized || signature != uiRenderCache.externalSignature;
  if (changed)
  {
    uiRenderCache.externalSignature = signature;
  }
  return changed;
}

unsigned long selectAdaptiveInterval(unsigned long nowMs,
                                     unsigned long lastActivityMs,
                                     unsigned long fastIntervalMs,
                                     unsigned long idleIntervalMs,
                                     unsigned long holdWindowMs)
{
  if ((nowMs - lastActivityMs) < holdWindowMs)
  {
    return fastIntervalMs;
  }
  return idleIntervalMs;
}

unsigned long getFilmCounterIntervalMs(unsigned long nowMs)
{
  return selectAdaptiveInterval(
      nowMs,
      lastFilmMovementMs,
      LOOP_FILM_COUNTER_INTERVAL_MS,
      LOOP_FILM_COUNTER_IDLE_INTERVAL_MS,
      LOOP_FILM_COUNTER_ACTIVE_HOLD_MS);
}

unsigned long getLensIntervalMs(unsigned long nowMs)
{
  return selectAdaptiveInterval(
      nowMs,
      lastLensMovementMs,
      LOOP_LENS_INTERVAL_MS,
      LOOP_LENS_IDLE_INTERVAL_MS,
      LOOP_LENS_ACTIVE_HOLD_MS);
}

unsigned long getLightMeterIntervalMs(unsigned long nowMs)
{
  // Keep meter updates snappy whenever user-adjusted settings are pending.
  if (aperture != prev_aperture || iso != prev_iso)
  {
    return LOOP_LIGHTMETER_INTERVAL_MS;
  }

  return selectAdaptiveInterval(
      nowMs,
      lastMeterChangeMs,
      LOOP_LIGHTMETER_INTERVAL_MS,
      LOOP_LIGHTMETER_IDLE_INTERVAL_MS,
      LOOP_LIGHTMETER_ACTIVE_HOLD_MS);
}

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
  lidarIdleStandbyActive = false;
  uiRenderCache.initialized = false;
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
  lidarIdleStandbyActive = false;
  uiRenderCache.initialized = false;
  sleepWakeBaselinesInitialized = false;

  // Force immediate sensor/UI refresh right after wake.
  scheduler.lastLidarMs = 0;
  scheduler.lastLensMs = 0;
  scheduler.lastMeterMs = 0;
  scheduler.lastBatteryMs = 0;
  scheduler.lastUiMs = 0;

  unsigned long nowMs = millis();
  lastFilmMovementMs = nowMs;
  lastLensMovementMs = nowMs;
  lastMeterChangeMs = nowMs;
}

void clearLidarUiForStandby()
{
  distance_cm = "...";
  lidar_quality_level = 0;
}

void updateLidarIdleStandby(unsigned long nowMs)
{
  // Keep LiDAR active when not in Main UI; wake immediately on any activity.
  bool canStandby = (ui_mode == UiMode::Main);
  unsigned long idleDurationMs = getIdleDurationMs(nowMs);

  if (!lidarIdleStandbyActive)
  {
    if (canStandby && lidarEnabled && idleDurationMs >= LIDAR_IDLE_STANDBY_TIMEOUT_MS)
    {
      toggleLidar(false);
      lidarIdleStandbyActive = !lidarEnabled;
      if (lidarIdleStandbyActive)
      {
        clearLidarUiForStandby();
      }
    }
    return;
  }

  if (!canStandby || idleDurationMs < LIDAR_IDLE_STANDBY_TIMEOUT_MS)
  {
    toggleLidar(true);
    lidarIdleStandbyActive = !lidarEnabled;
    if (!lidarIdleStandbyActive)
    {
      scheduler.lastLidarMs = 0; // Force immediate re-acquisition after wake.
    }
  }
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
    lastFilmMovementMs = now;
    lastLensMovementMs = now;
    lastMeterChangeMs = now;
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
    unsigned long filmCounterIntervalMs = getFilmCounterIntervalMs(now);
    if (shouldRunTask(now, scheduler.lastFilmCounterMs, filmCounterIntervalMs))
    {
      int previousEncoder = encoder_value;
      float previousProgress = frame_progress;
      setFilmCounter();
      if (encoder_value != previousEncoder || fabsf(frame_progress - previousProgress) > 0.0001f)
      {
        lastFilmMovementMs = now;
      }
    }

    updateLidarIdleStandby(now);

    if (!lidarIdleStandbyActive && shouldRunTask(now, scheduler.lastLidarMs, LOOP_LIDAR_INTERVAL_MS))
    {
      setDistance();
    }
    unsigned long lensIntervalMs = getLensIntervalMs(now);
    if (shouldRunTask(now, scheduler.lastLensMs, lensIntervalMs))
    {
      int previousReading = lens_sensor_reading;
      lens_sensor_reading = getLensSensorReading();
      if (abs(lens_sensor_reading - previousReading) > LENS_ACTIVITY_THRESHOLD)
      {
        lastLensMovementMs = now;
      }
      setLensDistance();
    }
    unsigned long lightMeterIntervalMs = getLightMeterIntervalMs(now);
    if (shouldRunTask(now, scheduler.lastMeterMs, lightMeterIntervalMs))
    {
      float previousLux = lux;
      float previousAperture = aperture;
      int previousIso = iso;
      String previousShutter = shutter_speed;
      setLightMeter();
      bool meterChanged = fabsf(lux - previousLux) >= LIGHTMETER_ACTIVITY_DELTA_LUX ||
                          aperture != previousAperture ||
                          iso != previousIso ||
                          shutter_speed != previousShutter;
      if (meterChanged)
      {
        lastMeterChangeMs = now;
      }
    }
    if (shouldRunTask(now, scheduler.lastBatteryMs, LOOP_BATTERY_INTERVAL_MS))
    {
      setVoltage();
    }

    if (shouldRunTask(now, scheduler.lastUiMs, LOOP_UI_INTERVAL_MS))
    {
      bool drewPrimaryUi = false;
      if (shouldDrawPrimaryUi(now))
      {
        drewPrimaryUi = true;
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
        else if (ui_mode == UiMode::ConfigUi)
        {
          drawUiConfigUI();
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
      }

      if (drewPrimaryUi || shouldDrawExternalUi())
      {
        drawExternalUI();
      }

      uiRenderCache.initialized = true;
      uiRenderCache.lastMode = ui_mode;
    }
  }

  if (shouldRunTask(now, scheduler.lastPrefsFlushMs, LOOP_PREFS_FLUSH_INTERVAL_MS))
  {
    flushPrefsIfDirty();
  }
}
// ---------------------
