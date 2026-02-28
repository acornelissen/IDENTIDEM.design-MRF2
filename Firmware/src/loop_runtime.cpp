#include "loop_runtime.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_sleep.h"

#include "activity.h"
#include "cyclefuncs.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "inputs.h"
#include "interface.h"
#include "mrfconstants.h"
#include "setfuncs.h"

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

struct LoopRuntimeState
{
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
};

LoopRuntimeState loopState;

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

uint32_t hashCString(uint32_t hash, const char *value)
{
  const char *raw = value ? value : "";
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
  hash = hashCString(hash, shutter_speed);
  hash = hashCString(hash, distance_cm);
  hash = hashCString(hash, lens_distance_cm);
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
  hash = hashInt(hash, lidar_idle_timeout_mode);
  hash = hashInt(hash, level_trim_landscape_deci_deg);
  hash = hashInt(hash, level_trim_portrait_pos_deci_deg);
  hash = hashInt(hash, level_trim_portrait_neg_deci_deg);
  hash = hashInt(hash, frame_one_offset);
  hash = hashInt(hash, frame_spacing_offset);
  hash = hashInt(hash, film_counter);
  hash = hashInt(hash, last_lidar_error_code);
  hash = hashInt(hash, lidar_recovery_count);
  hash = hashBool(hash, lidarEnabled);
  hash = hashBool(hash, adsReady);
  hash = hashBool(hash, mpuReady);
  hash = hashBool(hash, mainDisplayReady);
  hash = hashBool(hash, externalDisplayReady);
  hash = hashBool(hash, batteryGaugeReady);
  hash = hashBool(hash, lightMeterReady);
  hash = hashBool(hash, statusPixelReady);
  hash = hashBool(hash, encoderReady);
  hash = hashBool(hash, lidarSensorReady);
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
  UiRenderCache &cache = loopState.uiRenderCache;
  if (ui_mode == UiMode::Main)
  {
    uint32_t signature = buildMainUiSignature();
    bool changed = !cache.initialized || cache.lastMode != ui_mode || signature != cache.mainSignature;
    bool refreshDue = (nowMs - cache.lastMainDrawMs) >= LOOP_UI_MAIN_REFRESH_MS;
    if (!changed && !refreshDue)
    {
      return false;
    }

    cache.mainSignature = signature;
    cache.lastMainDrawMs = nowMs;
    return true;
  }

  uint32_t signature = buildMenuUiSignature();
  bool changed = !cache.initialized || cache.lastMode != ui_mode || signature != cache.menuSignature;
  bool healthRefreshDue = (ui_mode == UiMode::Health) &&
                          ((nowMs - cache.lastHealthDrawMs) >= LOOP_UI_HEALTH_REFRESH_MS);
  if (!changed && !healthRefreshDue)
  {
    return false;
  }

  cache.menuSignature = signature;
  if (ui_mode == UiMode::Health)
  {
    cache.lastHealthDrawMs = nowMs;
  }
  return true;
}

bool shouldDrawExternalUi()
{
  UiRenderCache &cache = loopState.uiRenderCache;
  uint32_t signature = buildExternalUiSignature();
  bool changed = !cache.initialized || signature != cache.externalSignature;
  if (changed)
  {
    cache.externalSignature = signature;
  }
  return changed;
}

void updateUiRenderCacheMode()
{
  loopState.uiRenderCache.initialized = true;
  loopState.uiRenderCache.lastMode = ui_mode;
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
      loopState.lastFilmMovementMs,
      LOOP_FILM_COUNTER_INTERVAL_MS,
      LOOP_FILM_COUNTER_IDLE_INTERVAL_MS,
      LOOP_FILM_COUNTER_ACTIVE_HOLD_MS);
}

unsigned long getLensIntervalMs(unsigned long nowMs)
{
  return selectAdaptiveInterval(
      nowMs,
      loopState.lastLensMovementMs,
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
      loopState.lastMeterChangeMs,
      LOOP_LIGHTMETER_INTERVAL_MS,
      LOOP_LIGHTMETER_IDLE_INTERVAL_MS,
      LOOP_LIGHTMETER_ACTIVE_HOLD_MS);
}

bool sendLightMeterCommand(uint8_t command)
{
  if (!lightMeterReady)
  {
    return false;
  }

  Wire.beginTransmission(LIGHTMETER_I2C_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

void powerDownLightMeterForSleep()
{
  if (loopState.lightMeterSleeping)
  {
    return;
  }

  if (sendLightMeterCommand(LIGHTMETER_CMD_POWER_DOWN))
  {
    loopState.lightMeterSleeping = true;
    return;
  }

  lightMeterReady = false;
  loopState.lightMeterSleeping = false;
}

void wakeLightMeterFromSleep()
{
  if (!loopState.lightMeterSleeping)
  {
    return;
  }

  bool poweredOn = sendLightMeterCommand(LIGHTMETER_CMD_POWER_ON);
  if (!poweredOn)
  {
    lightMeterReady = false;
    loopState.lightMeterSleeping = false;
    return;
  }

  bool configured = lightMeter.configure(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (!configured)
  {
    configured = lightMeter.begin();
  }

  if (!configured)
  {
    lightMeterReady = false;
    loopState.lightMeterSleeping = false;
    return;
  }

  lightMeterReady = true;
  loopState.lightMeterSleeping = false;
}

void initializeSleepWakeBaselines()
{
  loopState.sleepWakeEncoderBaseline = encoderReady ? encoder.getEncoderPosition() : 0;
  loopState.sleepWakeLensBaseline = adsReady ? theads.readADC_SingleEnded(LENS_ADC_PIN) : 0;
  loopState.sleepWakeBaselinesInitialized = true;
}

void pollSleepWakeEncoder()
{
  if (!encoderReady)
  {
    return;
  }

  if (!loopState.sleepWakeBaselinesInitialized)
  {
    initializeSleepWakeBaselines();
  }

  int currentEncoder = encoder.getEncoderPosition();
  if (abs(currentEncoder - loopState.sleepWakeEncoderBaseline) >= SLEEP_WAKE_ENCODER_DELTA)
  {
    registerActivity();
    loopState.sleepWakeEncoderBaseline = currentEncoder;
  }
}

void pollSleepWakeLens()
{
  if (!adsReady)
  {
    return;
  }

  if (!loopState.sleepWakeBaselinesInitialized)
  {
    initializeSleepWakeBaselines();
  }

  int currentLensReading = theads.readADC_SingleEnded(LENS_ADC_PIN);
  if (abs(currentLensReading - loopState.sleepWakeLensBaseline) >= SLEEP_WAKE_LENS_DELTA)
  {
    registerActivity();
    loopState.sleepWakeLensBaseline = currentLensReading;
  }
}

void resetWakeSchedulerAndActivityBaselines(unsigned long nowMs)
{
  // Force immediate sensor/UI refresh right after wake.
  loopState.scheduler.lastLidarMs = 0;
  loopState.scheduler.lastLensMs = 0;
  loopState.scheduler.lastMeterMs = 0;
  loopState.scheduler.lastBatteryMs = 0;
  loopState.scheduler.lastUiMs = 0;
  loopState.lastFilmMovementMs = nowMs;
  loopState.lastLensMovementMs = nowMs;
  loopState.lastMeterChangeMs = nowMs;
}

void enterSleepServices()
{
  toggleLidar(false);
  loopState.lidarIdleStandbyActive = false;
  loopState.uiRenderCache.initialized = false;
  powerDownLightMeterForSleep();
  drawSleepUI();

  // Keep external sleep text visible while turning the main display fully off.
  if (mainDisplayReady)
  {
    display.oled_command(0xAE);
  }

  if (statusPixelReady)
  {
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_OFF_R, NEOPIXEL_OFF_G, NEOPIXEL_OFF_B));
    sspixel.show();
  }

  initializeSleepWakeBaselines();

  if (mpuReady)
  {
    mpu.enableSleep(true);
  }

  // Scale CPU down after all I2C work is complete.
  setCpuFrequencyMhz(CPU_FREQ_SLEEP_MHZ);

  // Configure button GPIOs as light-sleep wakeup sources.
  gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_LEFT_PIN),  GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_RIGHT_PIN), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
}

void exitSleepServices()
{
  // Restore full CPU speed before any I2C communications.
  setCpuFrequencyMhz(CPU_FREQ_ACTIVE_MHZ);

  // Remove light-sleep wakeup sources before returning to active polling.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  gpio_wakeup_disable(static_cast<gpio_num_t>(BUTTON_LEFT_PIN));
  gpio_wakeup_disable(static_cast<gpio_num_t>(BUTTON_RIGHT_PIN));

  if (mainDisplayReady)
  {
    display.oled_command(0xAF);
  }

  if (mpuReady)
  {
    mpu.enableSleep(false);
  }

  wakeLightMeterFromSleep();
  toggleLidar(true);
  loopState.lidarIdleStandbyActive = false;
  loopState.uiRenderCache.initialized = false;
  loopState.sleepWakeBaselinesInitialized = false;
  resetWakeSchedulerAndActivityBaselines(millis());
}

void clearLidarUiForStandby()
{
  snprintf(distance_cm, sizeof(distance_cm), "Zzz");
  lidar_quality_level = 0;
}

void updateLidarIdleStandby(unsigned long nowMs)
{
  if (!lidarSensorReady)
  {
    loopState.lidarIdleStandbyActive = false;
    return;
  }

  // Keep LiDAR active when not in Main UI; wake immediately on any activity.
  bool canStandby = (ui_mode == UiMode::Main);
  unsigned long idleDurationMs = getIdleDurationMs(nowMs);
  unsigned long idleStandbyTimeoutMs = getSleepTimeoutModeMs(lidar_idle_timeout_mode);

  if (idleStandbyTimeoutMs == 0)
  {
    if (loopState.lidarIdleStandbyActive)
    {
      toggleLidar(true);
      loopState.lidarIdleStandbyActive = !lidarEnabled;
      if (!loopState.lidarIdleStandbyActive)
      {
        loopState.scheduler.lastLidarMs = 0; // Force immediate re-acquisition after wake.
      }
    }
    return;
  }

  if (!loopState.lidarIdleStandbyActive)
  {
    if (canStandby && lidarEnabled && idleDurationMs >= idleStandbyTimeoutMs)
    {
      toggleLidar(false);
      loopState.lidarIdleStandbyActive = !lidarEnabled;
      if (loopState.lidarIdleStandbyActive)
      {
        clearLidarUiForStandby();
      }
    }
    return;
  }

  if (!canStandby || idleDurationMs < idleStandbyTimeoutMs)
  {
    toggleLidar(true);
    loopState.lidarIdleStandbyActive = !lidarEnabled;
    if (!loopState.lidarIdleStandbyActive)
    {
      loopState.scheduler.lastLidarMs = 0; // Force immediate re-acquisition after wake.
    }
  }
}

void drawPrimaryUiForCurrentMode()
{
  switch (ui_mode)
  {
  case UiMode::Main:
    drawMainUI();
    break;
  case UiMode::Config:
    drawConfigUI();
    break;
  case UiMode::ConfigFilm:
    drawFilmConfigUI();
    break;
  case UiMode::ConfigLens:
    drawLensConfigUI();
    break;
  case UiMode::ConfigMeter:
    drawMeterConfigUI();
    break;
  case UiMode::ConfigUi:
    drawUiConfigUI();
    break;
  case UiMode::Calib:
    drawCalibUI();
    break;
  case UiMode::ResetConfirm:
    drawResetConfirmUI();
    break;
  case UiMode::Health:
    drawHealthUI();
    break;
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

void initializeSchedulerIfNeeded(unsigned long nowMs)
{
  if (loopState.scheduler.initialized)
  {
    return;
  }

  loopState.scheduler.initialized = true;
  loopState.scheduler.lastInputMs = 0;
  loopState.scheduler.lastFilmCounterMs = 0;
  loopState.scheduler.lastSleepCheckMs = 0;
  loopState.scheduler.lastLidarMs = 0;
  loopState.scheduler.lastLensMs = 0;
  loopState.scheduler.lastMeterMs = 0;
  loopState.scheduler.lastBatteryMs = 0;
  loopState.scheduler.lastUiMs = 0;
  loopState.scheduler.lastPrefsFlushMs = 0;
  loopState.lastFilmMovementMs = nowMs;
  loopState.lastLensMovementMs = nowMs;
  loopState.lastMeterChangeMs = nowMs;
}

void runSleepTasks([[maybe_unused]] unsigned long nowMs)
{
  if (!loopState.sleepServicesActive)
  {
    enterSleepServices();
    loopState.sleepServicesActive = true;
  }

  // Sleep the CPU until a button GPIO fires or the sensor-poll timer expires.
  esp_sleep_enable_timer_wakeup(LOOP_SLEEP_LIGHT_SLEEP_US);
  esp_light_sleep_start();

  switch (esp_sleep_get_wakeup_cause())
  {
    case ESP_SLEEP_WAKEUP_GPIO:
      // Button pressed — let Bounce2 confirm and register activity.
      checkButtons();
      break;

    case ESP_SLEEP_WAKEUP_TIMER:
    default:
      // Timer elapsed (or undefined wakeup) — poll I2C sensors for activity.
      pollSleepWakeEncoder();
      pollSleepWakeLens();
      break;
  }
}

void updateFilmCounterTask(unsigned long nowMs)
{
  unsigned long filmCounterIntervalMs = getFilmCounterIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastFilmCounterMs, filmCounterIntervalMs))
  {
    return;
  }

  int previousEncoder = encoder_value;
  float previousProgress = frame_progress;
  setFilmCounter();
  if (encoder_value != previousEncoder || fabsf(frame_progress - previousProgress) > 0.0001f)
  {
    loopState.lastFilmMovementMs = nowMs;
  }
}

void updateLidarTask(unsigned long nowMs)
{
  updateLidarIdleStandby(nowMs);
  if (!loopState.lidarIdleStandbyActive &&
      shouldRunTask(nowMs, loopState.scheduler.lastLidarMs, LOOP_LIDAR_INTERVAL_MS))
  {
    setDistance();
  }
}

void updateLensTask(unsigned long nowMs)
{
  if (!adsReady)
  {
    return;
  }

  unsigned long lensIntervalMs = getLensIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastLensMs, lensIntervalMs))
  {
    return;
  }

  int previousReading = lens_sensor_reading;
  lens_sensor_reading = getLensSensorReading();
  if (abs(lens_sensor_reading - previousReading) > LENS_ACTIVITY_THRESHOLD)
  {
    loopState.lastLensMovementMs = nowMs;
  }
  setLensDistance();
}

void updateLightMeterTask(unsigned long nowMs)
{
  unsigned long lightMeterIntervalMs = getLightMeterIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastMeterMs, lightMeterIntervalMs))
  {
    return;
  }

  float previousLux = lux;
  float previousAperture = aperture;
  int previousIso = iso;
  char previousShutter[sizeof(shutter_speed)] = {0};
  strncpy(previousShutter, shutter_speed, sizeof(previousShutter) - 1);
  setLightMeter();

  bool meterChanged = fabsf(lux - previousLux) >= LIGHTMETER_ACTIVITY_DELTA_LUX ||
                      aperture != previousAperture ||
                      iso != previousIso ||
                      strcmp(shutter_speed, previousShutter) != 0;
  if (meterChanged)
  {
    loopState.lastMeterChangeMs = nowMs;
  }
}

void updateBatteryTask(unsigned long nowMs)
{
  if (shouldRunTask(nowMs, loopState.scheduler.lastBatteryMs, LOOP_BATTERY_INTERVAL_MS))
  {
    setVoltage();
  }
}

void updateUiTask(unsigned long nowMs)
{
  if (!shouldRunTask(nowMs, loopState.scheduler.lastUiMs, LOOP_UI_INTERVAL_MS))
  {
    return;
  }

  bool drewPrimaryUi = false;
  if (mainDisplayReady && shouldDrawPrimaryUi(nowMs))
  {
    drewPrimaryUi = true;
    drawPrimaryUiForCurrentMode();
  }

  if (externalDisplayReady && (drewPrimaryUi || shouldDrawExternalUi()))
  {
    drawExternalUI();
  }

  updateUiRenderCacheMode();
}

void runAwakeTasks(unsigned long nowMs)
{
  if (loopState.sleepServicesActive)
  {
    exitSleepServices();
    loopState.sleepServicesActive = false;
  }

  if (shouldRunTask(nowMs, loopState.scheduler.lastInputMs, LOOP_INPUT_INTERVAL_MS))
  {
    checkButtons();
  }

  updateFilmCounterTask(nowMs);
  updateLidarTask(nowMs);
  updateLensTask(nowMs);
  updateLightMeterTask(nowMs);
  updateBatteryTask(nowMs);
  updateUiTask(nowMs);
}

void flushPrefsTask(unsigned long nowMs)
{
  if (shouldRunTask(nowMs, loopState.scheduler.lastPrefsFlushMs, LOOP_PREFS_FLUSH_INTERVAL_MS))
  {
    flushPrefsIfDirty();
  }
}
} // namespace

void runLoopRuntimeIteration(unsigned long now_ms)
{
  initializeSchedulerIfNeeded(now_ms);
  if (shouldRunTask(now_ms, loopState.scheduler.lastSleepCheckMs, LOOP_SLEEP_CHECK_INTERVAL_MS))
  {
    updateSleepMode(now_ms);
  }

  if (sleepMode)
  {
    runSleepTasks(now_ms);
  }
  else
  {
    runAwakeTasks(now_ms);
  }

  flushPrefsTask(now_ms);
}
