#include "interface.h"

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <math.h>
#include <string.h>

#include "activity.h"
#include "cyclefuncs.h"
#include "formats.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "lenses.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"

// Define colors for U8G2 if not available (GFX uses BLACK/WHITE)
#ifndef BLACK
#define BLACK 0
#endif
#ifndef WHITE
#define WHITE 1
#endif
#ifndef INVERSE
#define INVERSE 2
#endif

namespace
{
struct ParallaxShift
{
  float x;
  float y;
};

struct MainFramelineLayout
{
  int baseX;
  int baseY;
  int width;
  int height;
  int shiftedX;
  int shiftedY;
  int innerX;
  int innerY;
  int innerWidth;
  int innerHeight;
  int reticleCenterX;
  int reticleCenterY;
};

constexpr int LIDAR_QUALITY_BLOCK_COUNT = 4;

struct AccelReading
{
  float x;
  float y;
  float z;
};

struct LevelAngles
{
  float pitch;
  float roll;
};

bool portraitMode = false; // Landscape/portrait hysteresis state.

void getCompactShutterDisplay(const char *fullShutter, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  const char *suffix = " sec.";
  const size_t suffixLen = 5;
  const char *raw = fullShutter ? fullShutter : "";
  size_t rawLen = strlen(raw);

  if (rawLen > suffixLen && strcmp(raw + (rawLen - suffixLen), suffix) == 0)
  {
    size_t copyLen = min(rawLen - suffixLen, bufferSize - 1);
    memcpy(buffer, raw, copyLen);
    buffer[copyLen] = '\0';
    return;
  }

  strncpy(buffer, raw, bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

void getFormatWidthHeightMm(const FilmFormat &format, float &widthMm, float &heightMm)
{
  widthMm = format.frame_mm_width;
  heightMm = format.frame_mm_height;
}

float getParallaxDistanceMm()
{
  // Prefer lens focus distance so framelines track focus changes, but only if calibrated.
  if (lenses[selected_lens].calibrated && lens_distance_raw == LENS_INFINITY_RAW)
  {
    // Treat infinity as no parallax shift and avoid falling back to LiDAR.
    return 0.0f;
  }
  if (lenses[selected_lens].calibrated && lens_distance_raw > 0 && lens_distance_raw != LENS_INFINITY_RAW)
  {
    float lens_mm = static_cast<float>(lens_distance_raw) * CM_TO_MM;
    return max(lens_mm, PARALLAX_MIN_DISTANCE_MM);
  }

  if (distance > 0)
  {
    float lidar_mm = static_cast<float>(distance) * CM_TO_MM;
    if (lidar_mm > 0)
    {
      return max(lidar_mm, PARALLAX_MIN_DISTANCE_MM);
    }
  }

  return 0.0f;
}

ParallaxShift computeParallaxShiftPx(int frameWidthPx, int frameHeightPx)
{
  ParallaxShift shift = {0.0f, 0.0f};

  if (!parallaxEnabled)
  {
    return shift;
  }

  if ((PARALLAX_OFFSET_X_MM == 0.0f && PARALLAX_OFFSET_Y_MM == 0.0f) || lenses[selected_lens].focal_mm <= 0)
  {
    return shift;
  }

  float distance_mm = getParallaxDistanceMm();
  if (distance_mm <= 0.0f)
  {
    return shift;
  }

  float formatWidthMm = 0.0f;
  float formatHeightMm = 0.0f;
  getFormatWidthHeightMm(film_formats[selected_format], formatWidthMm, formatHeightMm);
  if (formatWidthMm <= 0.0f || formatHeightMm <= 0.0f)
  {
    return shift;
  }

  float shiftX_mm = lenses[selected_lens].focal_mm * PARALLAX_OFFSET_X_MM / distance_mm;
  float shiftY_mm = lenses[selected_lens].focal_mm * PARALLAX_OFFSET_Y_MM / distance_mm;

  float pxPerMmX = static_cast<float>(frameWidthPx) / formatWidthMm;
  float pxPerMmY = static_cast<float>(frameHeightPx) / formatHeightMm;

  shift.x = shiftX_mm * pxPerMmX;
  shift.y = shiftY_mm * pxPerMmY;

  if (shift.x > PARALLAX_MAX_SHIFT_PX)
  {
    shift.x = PARALLAX_MAX_SHIFT_PX;
  }
  else if (shift.x < -PARALLAX_MAX_SHIFT_PX)
  {
    shift.x = -PARALLAX_MAX_SHIFT_PX;
  }

  if (shift.y > PARALLAX_MAX_SHIFT_PX)
  {
    shift.y = PARALLAX_MAX_SHIFT_PX;
  }
  else if (shift.y < -PARALLAX_MAX_SHIFT_PX)
  {
    shift.y = -PARALLAX_MAX_SHIFT_PX;
  }

  return shift;
}

void setMenuItemColors(bool selected)
{
  if (selected)
  {
    u8g2.setBackgroundColor(WHITE);
    u8g2.setForegroundColor(BLACK);
  }
  else
  {
    u8g2.setBackgroundColor(BLACK);
    u8g2.setForegroundColor(WHITE);
  }
}

void printSignedInt(int value)
{
  if (value > 0)
  {
    u8g2.print(F("+"));
  }
  u8g2.print(value);
}

void printSignedDeciDegrees(int deciDegrees)
{
  if (deciDegrees > 0)
  {
    u8g2.print(F("+"));
  }
  u8g2.print(static_cast<float>(deciDegrees) / 10.0f, 1);
}

void preparePrimaryDisplayTextMode()
{
  display.clearDisplay();
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);
}

int getConfigMenuRowY(int step)
{
  const int menuItemStart = CONFIG_ITEM_Y_START + CONFIG_HEADER_PADDING_Y;
  return menuItemStart + (CONFIG_ITEM_Y_STEP * step);
}

void beginConfigMenuScreen(const __FlashStringHelper *title)
{
  preparePrimaryDisplayTextMode();
  u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(CONFIG_TITLE_X, CONFIG_TITLE_Y);
  u8g2.print(title);
  u8g2.setFont(u8g2_font_4x6_mf);
}

void selectConfigMenuRow(int step, bool selected)
{
  u8g2.setCursor(CONFIG_ITEM_X, getConfigMenuRowY(step));
  setMenuItemColors(selected);
}

void resetConfigTextColors()
{
  u8g2.setBackgroundColor(BLACK);
  u8g2.setForegroundColor(WHITE);
}

void drawLidarQualityIndicator()
{
  int blocks = lidar_quality_level;
  if (blocks < 0)
  {
    blocks = 0;
  }
  else if (blocks > LIDAR_QUALITY_BLOCK_COUNT)
  {
    blocks = LIDAR_QUALITY_BLOCK_COUNT;
  }

  for (int i = 0; i < LIDAR_QUALITY_BLOCK_COUNT; i++)
  {
    int x = MAIN_LIDAR_QUALITY_X;
    int y = MAIN_LIDAR_QUALITY_Y + (i * (MAIN_LIDAR_QUALITY_SIZE + MAIN_LIDAR_QUALITY_GAP));
    int color = (i >= (LIDAR_QUALITY_BLOCK_COUNT - blocks)) ? BLACK : WHITE;
    display.fillRect(x, y, MAIN_LIDAR_QUALITY_SIZE, MAIN_LIDAR_QUALITY_SIZE, color);
  }
}

void drawMainHeader()
{
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  u8g2.setFont(u8g2_font_4x6_mf);

  display.fillRect(0, 0, SCREEN_WIDTH, MAIN_HEADER_HEIGHT, WHITE);
  display.drawLine(SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT, BLACK);

  u8g2.setCursor(MAIN_ISO_X, MAIN_ISO_Y);
  u8g2.print(F("ISO"));
  u8g2.print(iso);

  u8g2.setCursor(MAIN_APERTURE_X, MAIN_APERTURE_Y);
  u8g2.print(F("f"));
  if (aperture == static_cast<int>(aperture))
  {
    u8g2.print(static_cast<int>(aperture));
  }
  else
  {
    u8g2.print(aperture, APERTURE_DECIMAL_PLACES);
  }

  u8g2.setCursor(MAIN_SHUTTER_X, MAIN_SHUTTER_Y);
  if (show_ev_readout)
  {
    char compactShutter[16] = {0};
    getCompactShutterDisplay(shutter_speed, compactShutter, sizeof(compactShutter));
    u8g2.print(compactShutter);
    u8g2.print(F(" EV"));
    if (ev_readout == ev_readout)
    {
      u8g2.print(ev_readout, 1);
    }
    else
    {
      u8g2.print(F("--.-"));
    }
  }
  else
  {
    u8g2.print(shutter_speed);
  }

  u8g2.setCursor(MAIN_DISTANCE_X, MAIN_DISTANCE_Y);
  u8g2.print(F("Dist:"));
  u8g2.print(distance_cm);
  drawLidarQualityIndicator();

  u8g2.setCursor(MAIN_LENS_X, MAIN_LENS_Y);
  u8g2.print(F("Lens:"));
  u8g2.print(lens_distance_cm);
}

void getScaledFramelineDimensions(int baseWidth,
                                  int baseHeight,
                                  int &scaledWidth,
                                  int &scaledHeight)
{
  float formatWidthMm = 0.0f;
  float formatHeightMm = 0.0f;
  getFormatWidthHeightMm(film_formats[selected_format], formatWidthMm, formatHeightMm);

  int baseFormatIndex = constrain(DEFAULT_SELECTED_FORMAT, 0, static_cast<int>(NUM_FILM_FORMATS) - 1);

  float baseFormatWidthMm = 0.0f;
  float baseFormatHeightMm = 0.0f;
  getFormatWidthHeightMm(film_formats[baseFormatIndex], baseFormatWidthMm, baseFormatHeightMm);

  scaledWidth = baseWidth;
  scaledHeight = baseHeight;
  bool allowOverflow = false;

  if (formatWidthMm > 0.0f && formatHeightMm > 0.0f)
  {
    float formatRatio = formatWidthMm / formatHeightMm;
    float baseRatio = static_cast<float>(baseWidth) / static_cast<float>(baseHeight);

    if (baseFormatWidthMm > 0.0f && baseFormatHeightMm > 0.0f)
    {
      float baseFormatRatio = baseFormatWidthMm / baseFormatHeightMm;
      allowOverflow = formatRatio > baseFormatRatio && formatHeightMm >= baseFormatHeightMm;
    }

    if (allowOverflow)
    {
      scaledHeight = baseHeight;
      scaledWidth = static_cast<int>(roundf(baseHeight * formatRatio));
    }
    else if (formatRatio >= baseRatio)
    {
      scaledWidth = baseWidth;
      scaledHeight = static_cast<int>(roundf(baseWidth / formatRatio));
    }
    else
    {
      scaledHeight = baseHeight;
      scaledWidth = static_cast<int>(roundf(baseHeight * formatRatio));
    }
  }

  if (allowOverflow)
  {
    scaledWidth = max(1, scaledWidth);
    scaledHeight = max(1, min(baseHeight, scaledHeight));
  }
  else
  {
    scaledWidth = max(1, min(baseWidth, scaledWidth));
    scaledHeight = max(1, min(baseHeight, scaledHeight));
  }
}

MainFramelineLayout buildMainFramelineLayout()
{
  MainFramelineLayout layout = {};

  layout.baseX = lenses[selected_lens].framelines[0];
  layout.baseY = lenses[selected_lens].framelines[1];
  layout.width = lenses[selected_lens].framelines[2];
  layout.height = lenses[selected_lens].framelines[3];

  int scaledWidth = layout.width;
  int scaledHeight = layout.height;
  getScaledFramelineDimensions(layout.width, layout.height, scaledWidth, scaledHeight);

  ParallaxShift parallax = computeParallaxShiftPx(scaledWidth, scaledHeight);
  layout.shiftedX = layout.baseX + static_cast<int>(roundf(parallax.x));
  layout.shiftedY = layout.baseY + static_cast<int>(roundf(parallax.y));

  float frameCenterX = layout.shiftedX + (layout.width / 2.0f);
  float frameCenterY = layout.shiftedY + (layout.height / 2.0f);
  layout.innerWidth = scaledWidth;
  layout.innerHeight = scaledHeight;
  layout.innerX = static_cast<int>(roundf(frameCenterX - (scaledWidth / 2.0f)));
  layout.innerY = static_cast<int>(roundf(frameCenterY - (scaledHeight / 2.0f)));

  layout.reticleCenterX = (layout.baseX + (layout.width / 2)) + RETICLE_OFFSET_X;
  layout.reticleCenterY = (layout.baseY + (layout.height / 2) - MAIN_RETICLE_CENTER_Y_OFFSET) + RETICLE_OFFSET_Y;

  return layout;
}

void drawMainFrameline(const MainFramelineLayout &layout)
{
  display.fillRect(layout.shiftedX, layout.shiftedY, layout.width, layout.height, WHITE);
  display.fillRect(layout.innerX, layout.innerY, layout.innerWidth, layout.innerHeight, BLACK);
  display.drawRect(layout.innerX, layout.innerY, layout.innerWidth, layout.innerHeight, WHITE);
}

int getSmoothedFocusRingThickness(int focusRadius)
{
  static bool focusThicknessInit = false;
  static float focusThicknessSmoothed = 0.0f;

  float targetThickness = static_cast<float>(FOCUS_RING_THICKNESS_MIN);
  if (FOCUS_RADIUS_MAX > FOCUS_RADIUS_MIN)
  {
    float ratio = static_cast<float>(focusRadius - FOCUS_RADIUS_MIN) /
                  static_cast<float>(FOCUS_RADIUS_MAX - FOCUS_RADIUS_MIN);
    targetThickness = static_cast<float>(FOCUS_RING_THICKNESS_MIN) +
                      (ratio * (FOCUS_RING_THICKNESS_MAX - FOCUS_RING_THICKNESS_MIN));
  }

  targetThickness = max(static_cast<float>(FOCUS_RING_THICKNESS_MIN),
                        min(static_cast<float>(FOCUS_RING_THICKNESS_MAX), targetThickness));

  if (!focusThicknessInit)
  {
    focusThicknessSmoothed = targetThickness;
    focusThicknessInit = true;
  }

  focusThicknessSmoothed += (targetThickness - focusThicknessSmoothed) * FOCUS_RING_THICKNESS_SMOOTHING;

  int focusThickness = static_cast<int>(roundf(focusThicknessSmoothed));
  return max(FOCUS_RING_THICKNESS_MIN, min(FOCUS_RING_THICKNESS_MAX, focusThickness));
}

void drawReticleAndFocusRing(const MainFramelineLayout &layout)
{
  display.fillCircle(layout.reticleCenterX, layout.reticleCenterY, MAIN_RETICLE_CENTER_RADIUS, INVERSE);

  int focusRadius = getFocusRadius();
  int focusThickness = getSmoothedFocusRingThickness(focusRadius);
  int outerRadius = focusRadius;
  int innerRadius = focusRadius - focusThickness;

  if (outerRadius >= 1)
  {
    display.fillCircle(layout.reticleCenterX, layout.reticleCenterY, outerRadius, INVERSE);
  }
  if (innerRadius >= 1)
  {
    display.fillCircle(layout.reticleCenterX, layout.reticleCenterY, innerRadius, INVERSE);
  }
}

AccelReading readAccelerometer()
{
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  return {a.acceleration.x, a.acceleration.y, a.acceleration.z};
}

void updatePortraitMode(float rollRaw)
{
  float absRollRaw = fabsf(rollRaw);
  if (!portraitMode)
  {
    if (absRollRaw >= LEVEL_PORTRAIT_ENTER_RAD)
    {
      portraitMode = true;
    }
  }
  else if (absRollRaw <= LEVEL_PORTRAIT_EXIT_RAD)
  {
    portraitMode = false;
  }
}

LevelAngles computeLevelAngles(const AccelReading &accel)
{
  // Convert accelerometer readings into angles.
  // Keep the existing calibration basis but auto-rebase roll when in portrait.
  float pitchRaw = atan2(accel.x, sqrt(accel.x * accel.x + accel.z * accel.z));
  float rollRaw = atan2(accel.y, sqrt(accel.x * accel.x + accel.z * accel.z));
  float rollPortraitRaw = atan2(accel.y, accel.z);

  updatePortraitMode(rollRaw);

  float pitchScale = portraitMode ? LEVEL_PITCH_SCALE_PORTRAIT : LEVEL_PITCH_SCALE;
  float rollScale = portraitMode ? LEVEL_ROLL_SCALE_PORTRAIT : LEVEL_ROLL_SCALE;

  float pitchAngle = pitchRaw;
  if (fabsf(pitchAngle) < LEVEL_DEADZONE)
  {
    pitchAngle = 0.0f;
  }
  pitchAngle *= pitchScale;

  const float degToRad = PI / 180.0f;
  float rollAngle = 0.0f;
  if (portraitMode)
  {
    float portraitSign = (rollPortraitRaw >= 0.0f) ? 1.0f : -1.0f;
    float portraitCenter = portraitSign * HALF_PI;
    float rollDeviation = rollPortraitRaw - portraitCenter;

    // Normalize deviation so both sides around portrait center are represented.
    rollDeviation = atan2f(sinf(rollDeviation), cosf(rollDeviation));

    if (fabsf(rollDeviation) < LEVEL_DEADZONE)
    {
      rollDeviation = 0.0f;
    }

    // In portrait, keep the indicator centered at +/-90deg and mirror deviation by side.
    // Use per-side trim so left/right portrait can be calibrated independently.
    float portraitDeviationScale = LEVEL_PORTRAIT_ROLL_DEVIATION_SIGN * portraitSign;
    int portraitTrimDeciDegrees = (portraitSign > 0.0f)
                                      ? level_trim_portrait_pos_deci_deg
                                      : level_trim_portrait_neg_deci_deg;
    float portraitTrim = (static_cast<float>(portraitTrimDeciDegrees) / 10.0f) * degToRad;

    rollAngle = portraitCenter +
                (portraitDeviationScale * rollDeviation * rollScale) +
                portraitTrim;
  }
  else
  {
    float rollLandscape = rollRaw;
    if (fabsf(rollLandscape) < LEVEL_DEADZONE)
    {
      rollLandscape = 0.0f;
    }

    float landscapeTrim = (static_cast<float>(level_trim_landscape_deci_deg) / 10.0f) * degToRad;
    rollAngle = (rollLandscape * rollScale) + landscapeTrim;
  }

  return {pitchAngle, rollAngle};
}

void renderLevelLine(int centerX, int centerY, const LevelAngles &angles)
{
  float lineLength = SCREEN_WIDTH - LEVEL_LINE_MARGIN_PX;
  float halfLength = lineLength * 0.5f;

  // Calculate the horizon line direction and shift pitch along the line normal.
  float dirX = cosf(angles.roll);
  float dirY = sinf(angles.roll);
  float normalX = -dirY;
  float normalY = dirX;
  float offsetX = normalX * angles.pitch;
  float offsetY = normalY * angles.pitch;

  float startX = centerX + offsetX - (halfLength * dirX);
  float startY = centerY + offsetY - (halfLength * dirY);
  float endX = centerX + offsetX + (halfLength * dirX);
  float endY = centerY + offsetY + (halfLength * dirY);

  display.drawLine(startX, startY, endX, endY, INVERSE);

  int verticalLineStartY = centerY - (LEVEL_VERTICAL_LINE_LENGTH / 2);
  int verticalLineEndY = centerY + (LEVEL_VERTICAL_LINE_LENGTH / 2);
  display.drawLine(centerX, verticalLineStartY, centerX, verticalLineEndY, INVERSE);
}

void drawLevelIndicator(int centerX, int centerY)
{
  if (!mpuReady)
  {
    return;
  }

  AccelReading accel = readAccelerometer();
  LevelAngles angles = computeLevelAngles(accel);
  renderLevelLine(centerX, centerY, angles);
}

void prepareExternalDisplayTextMode()
{
  display_ext.clearDisplay();
  u8g2_ext.setFontMode(1);
  u8g2_ext.setFontDirection(0);
  u8g2_ext.setForegroundColor(BLACK);
  u8g2_ext.setBackgroundColor(WHITE);
  u8g2_ext.setFont(u8g2_font_6x10_mf);
}

void drawExternalHeader()
{
  display_ext.fillRect(0, 0, SCREEN_WIDTH, EXT_HEADER_HEIGHT, WHITE);

  u8g2_ext.setCursor(EXT_HEADER_FORMAT_X, EXT_HEADER_FORMAT_Y);
  u8g2_ext.print(film_formats[selected_format].name);

  display_ext.drawLine(EXT_HEADER_DIVIDER_X, 0, EXT_HEADER_DIVIDER_X, EXT_HEADER_HEIGHT, BLACK);

  u8g2_ext.setCursor(EXT_HEADER_LENS_X, EXT_HEADER_LENS_Y);
  u8g2_ext.print(lenses[selected_lens].name);
}

void drawExternalBatteryReadout()
{
  if (bat_per == BATTERY_PERCENT_MAX)
  {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_FULL_X, 0, EXT_BATTERY_DIVIDER_FULL_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_FULL_X, EXT_HEADER_FORMAT_Y);
  }
  else if (bat_per < BATTERY_PERCENT_LOW_THRESHOLD)
  {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_LOW_X, 0, EXT_BATTERY_DIVIDER_LOW_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_LOW_X, EXT_HEADER_FORMAT_Y);
  }
  else
  {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_MID_X, 0, EXT_BATTERY_DIVIDER_MID_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_MID_X, EXT_HEADER_FORMAT_Y);
  }

  u8g2_ext.print(bat_per);
  u8g2_ext.print(F("%"));
}

bool drawExternalProgressBarAndLed()
{
  if (frame_progress <= 0.0f)
  {
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_BLUE_R, NEOPIXEL_BLUE_G, NEOPIXEL_BLUE_B));
    return false;
  }

  float progressPercentage = frame_progress * PERCENT_SCALE;
  int progressWidth = static_cast<int>(EXT_PROGRESS_BAR_WIDTH * (progressPercentage / PERCENT_SCALE));

  display_ext.drawRect(EXT_PROGRESS_BAR_X, EXT_PROGRESS_BAR_Y, EXT_PROGRESS_BAR_WIDTH, EXT_PROGRESS_BAR_HEIGHT, WHITE);
  display_ext.fillRect(EXT_PROGRESS_BAR_X, EXT_PROGRESS_BAR_Y, progressWidth, EXT_PROGRESS_BAR_HEIGHT, WHITE);

  if (frame_progress != prev_frame_progress)
  {
    if (progressPercentage > 0 && progressPercentage < PERCENT_SCALE)
    {
      int greenValue = static_cast<int>(frame_progress * NEOPIXEL_COLOR_MAX);
      int redValue = static_cast<int>((1 - frame_progress) * NEOPIXEL_COLOR_MAX);
      sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(redValue, greenValue, NEOPIXEL_OFF_B));
    }
    prev_frame_progress = frame_progress;
  }

  return true;
}

void drawExternalCounterText(bool progressVisible)
{
  u8g2_ext.setForegroundColor(WHITE);
  u8g2_ext.setBackgroundColor(BLACK);
  u8g2_ext.setFont(u8g2_font_10x20_mf);

  if (film_counter == 0 && frame_progress == 0)
  {
    u8g2_ext.setCursor(EXT_COUNTER_MESSAGE_X, EXT_COUNTER_TEXT_Y);
    u8g2_ext.print(F(" Load film."));
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_VIOLET_R, NEOPIXEL_VIOLET_G, NEOPIXEL_VIOLET_B));
  }
  else if (film_counter == FILM_COUNTER_END)
  {
    u8g2_ext.setCursor(EXT_COUNTER_MESSAGE_X, EXT_COUNTER_TEXT_Y);
    u8g2_ext.print(F(" Roll end."));
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_VIOLET_R, NEOPIXEL_VIOLET_G, NEOPIXEL_VIOLET_B));
  }
  else
  {
    u8g2_ext.setCursor(progressVisible ? EXT_COUNTER_VALUE_X_WITH_PROGRESS : EXT_COUNTER_VALUE_X_NO_PROGRESS,
                       EXT_COUNTER_TEXT_Y);
    u8g2_ext.print(film_counter);
  }
}
} // namespace

// Functions to draw UI
// ---------------------
void drawMainUI()
{
  display.clearDisplay();
  drawMainHeader();

  MainFramelineLayout framelineLayout = buildMainFramelineLayout();
  drawMainFrameline(framelineLayout);
  drawReticleAndFocusRing(framelineLayout);
  drawLevelIndicator(framelineLayout.reticleCenterX, framelineLayout.reticleCenterY);

  display.display();
}

void drawConfigUI()
{
  beginConfigMenuScreen(F("Setup"));

  selectConfigMenuRow(CONFIG_ROOT_STEP_FILM_MENU, config_step == CONFIG_ROOT_STEP_FILM_MENU);
  u8g2.print(F(" Film > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_LENS_MENU, config_step == CONFIG_ROOT_STEP_LENS_MENU);
  u8g2.print(F(" Lens Settings > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_METER_MENU, config_step == CONFIG_ROOT_STEP_METER_MENU);
  u8g2.print(F(" Light Meter > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_UI_MENU, config_step == CONFIG_ROOT_STEP_UI_MENU);
  u8g2.print(F(" UI Settings > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_RESET, config_step == CONFIG_ROOT_STEP_RESET);
  u8g2.print(F(" Reset frame counter >> "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_HEALTH, config_step == CONFIG_ROOT_STEP_HEALTH);
  u8g2.print(F(" System Health > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_EXIT, config_step == CONFIG_ROOT_STEP_EXIT);
  u8g2.print(F(" Exit >> "));

  u8g2.setCursor(CONFIG_ITEM_X, CONFIG_FOOTER_Y);
  resetConfigTextColors();
  u8g2.print(F(" IDENTIDEM.design MRF "));
  u8g2.print(FWVERSION);

  display.display();
}

void drawFilmConfigUI()
{
  beginConfigMenuScreen(F("Film"));

  int maxFrameForFormat = getFilmFormatMaxFrame(film_formats[selected_format]);
  int displayedFrame = constrain(film_counter, 0, maxFrameForFormat);

  selectConfigMenuRow(CONFIG_FILM_STEP_FORMAT, config_step == CONFIG_FILM_STEP_FORMAT);
  u8g2.print(F(" Format: "));
  u8g2.print(film_formats[selected_format].name);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FILM_STEP_CURRENT_FRAME, config_step == CONFIG_FILM_STEP_CURRENT_FRAME);
  u8g2.print(F(" Current frame: "));
  u8g2.print(displayedFrame);
  u8g2.print(F(" (0.."));
  u8g2.print(maxFrameForFormat);
  u8g2.print(F(") "));

  selectConfigMenuRow(CONFIG_FILM_STEP_FRAME_ONE_OFFSET, config_step == CONFIG_FILM_STEP_FRAME_ONE_OFFSET);
  u8g2.print(F(" Frame 1 offset: "));
  printSignedInt(frame_one_offset);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FILM_STEP_FRAME_SPACING, config_step == CONFIG_FILM_STEP_FRAME_SPACING);
  u8g2.print(F(" Frame spacing: "));
  printSignedInt(frame_spacing_offset);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FILM_STEP_BACK, config_step == CONFIG_FILM_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawLensConfigUI()
{
  beginConfigMenuScreen(F("Lens"));

  selectConfigMenuRow(CONFIG_LENS_STEP_LENS, config_step == CONFIG_LENS_STEP_LENS);
  u8g2.print(F(" Lens:"));
  u8g2.print(lenses[selected_lens].name);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_LENS_STEP_PARALLAX, config_step == CONFIG_LENS_STEP_PARALLAX);
  u8g2.print(F(" Parallax correction: "));
  u8g2.print(parallaxEnabled ? F("On") : F("Off"));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_LENS_STEP_CALIB, config_step == CONFIG_LENS_STEP_CALIB);
  u8g2.print(F(" Lens Calibration > "));

  selectConfigMenuRow(CONFIG_LENS_STEP_BACK, config_step == CONFIG_LENS_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawMeterConfigUI()
{
  beginConfigMenuScreen(F("Meter"));

  selectConfigMenuRow(CONFIG_METER_STEP_ISO, config_step == CONFIG_METER_STEP_ISO);
  u8g2.print(F(" ISO:"));
  u8g2.print(iso);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_EV_COMP, config_step == CONFIG_METER_STEP_EV_COMP);
  u8g2.print(F(" EV Comp:"));
  float evComp = static_cast<float>(exposure_comp_thirds) / 3.0f;
  if (evComp >= 0.0f)
  {
    u8g2.print(F("+"));
  }
  u8g2.print(evComp, 1);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_SMOOTHING, config_step == CONFIG_METER_STEP_SMOOTHING);
  u8g2.print(F(" Smoothing:"));
  u8g2.print(getMeterSmoothingLabel(meter_smoothing_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_EV_READOUT, config_step == CONFIG_METER_STEP_EV_READOUT);
  u8g2.print(F(" EV Readout:"));
  u8g2.print(show_ev_readout ? F("On") : F("Off"));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_BACK, config_step == CONFIG_METER_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawUiConfigUI()
{
  beginConfigMenuScreen(F("UI Settings"));

  selectConfigMenuRow(CONFIG_UI_STEP_HORIZON_LANDSCAPE, config_step == CONFIG_UI_STEP_HORIZON_LANDSCAPE);
  u8g2.print(F(" Horizon L: "));
  printSignedDeciDegrees(level_trim_landscape_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_UI_STEP_HORIZON_PORTRAIT_POS, config_step == CONFIG_UI_STEP_HORIZON_PORTRAIT_POS);
  u8g2.print(F(" Horizon P+: "));
  printSignedDeciDegrees(level_trim_portrait_pos_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_UI_STEP_HORIZON_PORTRAIT_NEG, config_step == CONFIG_UI_STEP_HORIZON_PORTRAIT_NEG);
  u8g2.print(F(" Horizon P-: "));
  printSignedDeciDegrees(level_trim_portrait_neg_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_UI_STEP_SLEEP_TIMEOUT, config_step == CONFIG_UI_STEP_SLEEP_TIMEOUT);
  u8g2.print(F(" Sleep timeout: "));
  u8g2.print(getSleepTimeoutModeLabel(sleep_timeout_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_UI_STEP_LIDAR_IDLE_TIMEOUT, config_step == CONFIG_UI_STEP_LIDAR_IDLE_TIMEOUT);
  u8g2.print(F(" LiDAR idle: "));
  u8g2.print(getSleepTimeoutModeLabel(lidar_idle_timeout_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_UI_STEP_BACK, config_step == CONFIG_UI_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawCalibUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(CALIB_TITLE_X, CALIB_TITLE_Y);
  u8g2.print(F("Calibrate"));

  u8g2.setFont(u8g2_font_4x6_mf);

  u8g2.setCursor(CALIB_ITEM_X, CALIB_LENS_Y);
  setMenuItemColors(calib_step == 0);
  u8g2.print(F(" Lens:"));
  u8g2.print(lenses[calib_lens].name);
  u8g2.print(F(" "));

  const Lens &calibrationLens = lenses[calib_lens];
  int calibrationPointCount = getLensDistancePointCount(calibrationLens);
  if (calibrationPointCount <= 0)
  {
    calibrationPointCount = 1;
  }

  int calibrationIndex = max(0, min(current_calib_distance, calibrationPointCount - 1));
  float calibrationDistance = calibrationLens.distance[calibrationIndex];

  u8g2.setCursor(CALIB_ITEM_X, CALIB_DISTANCE_Y);
  setMenuItemColors(calib_step == 1);
  u8g2.print(F(" "));
  u8g2.print(calibrationDistance, DISTANCE_DECIMAL_PLACES);
  u8g2.print(F("m: "));
  u8g2.print(lens_sensor_reading);
  u8g2.print(F(" "));

  resetConfigTextColors();

  if (calib_step == 0)
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1);
    u8g2.print(F(" (L) to Cycle"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2);
    u8g2.print(F(" (R) to Select"));
  }
  else
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1);
    u8g2.print(F(" (L) to Select"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2);
    u8g2.print(F(" (R) to Cancel"));

    if (calib_capture_status == CALIB_CAPTURE_STATUS_UNSTABLE)
    {
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1);
      u8g2.print(F(" Unstable reading"));
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2);
      u8g2.print(F(" Hold still + retry"));
    }
    else if (calib_capture_status == CALIB_CAPTURE_STATUS_NON_MONOTONIC)
    {
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1);
      u8g2.print(F(" Out of sequence"));
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2);
      u8g2.print(F(" Recheck distance"));
    }
  }

  display.display();
}

void drawResetConfirmUI()
{
  beginConfigMenuScreen(F("Reset Count?"));

  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y1);
  u8g2.print(F(" (L) Cancel"));
  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y2);
  u8g2.print(F(" (R) Reset"));

  display.display();
}

void drawHealthUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(HEALTH_TITLE_X, HEALTH_TITLE_Y);
  u8g2.print(F("System Health"));

  u8g2.setFont(u8g2_font_4x6_mf);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 0));
  u8g2.print(F("FW: "));
  u8g2.print(FWVERSION);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 1));
  u8g2.print(F("Prefs: "));
  if (prefsSchemaValid)
  {
    u8g2.print(F("v"));
    u8g2.print(prefsSchemaVersionLoaded);
    u8g2.print(F(" OK"));
  }
  else if (prefsLoadedLegacy)
  {
    u8g2.print(F("Legacy migrated"));
  }
  else
  {
    u8g2.print(F("Defaults"));
  }

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 2));
  u8g2.print(F("LiDAR: "));
  u8g2.print(lidarSensorReady ? F("OK ") : F("InitErr "));
  u8g2.print(lidarEnabled ? F("On") : F("Off"));
  u8g2.print(F(" err:"));
  u8g2.print(last_lidar_error_code);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 3));
  u8g2.print(F("Recoveries: "));
  u8g2.print(lidar_recovery_count);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 4));
  u8g2.print(F("HW D"));
  u8g2.print(mainDisplayReady ? 1 : 0);
  u8g2.print(F(" X"));
  u8g2.print(externalDisplayReady ? 1 : 0);
  u8g2.print(F(" A"));
  u8g2.print(adsReady ? 1 : 0);
  u8g2.print(F(" M"));
  u8g2.print(mpuReady ? 1 : 0);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 5));
  u8g2.print(F("HW L"));
  u8g2.print(lightMeterReady ? 1 : 0);
  u8g2.print(F(" B"));
  u8g2.print(batteryGaugeReady ? 1 : 0);
  u8g2.print(F(" E"));
  u8g2.print(encoderReady ? 1 : 0);
  u8g2.print(F(" P"));
  u8g2.print(statusPixelReady ? 1 : 0);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_FOOTER_Y);
  u8g2.print(F(" (L/R) Back"));

  display.display();
}

void drawExternalUI()
{
  if (!externalDisplayReady)
  {
    return;
  }

  prepareExternalDisplayTextMode();
  drawExternalHeader();
  drawExternalBatteryReadout();

  bool progressVisible = drawExternalProgressBarAndLed();
  drawExternalCounterText(progressVisible);

  if (statusPixelReady)
  {
    sspixel.show();
  }
  display_ext.display();
}

void drawSleepUI()
{
  if (mainDisplayReady)
  {
    display.clearDisplay();
    display.display();
  }

  if (externalDisplayReady)
  {
    display_ext.clearDisplay();
    // Face outline
    display_ext.drawCircle(EXT_SLEEP_FACE_CX, EXT_SLEEP_FACE_CY, EXT_SLEEP_FACE_RADIUS, WHITE);
    // Closed eyes
    display_ext.drawLine(EXT_SLEEP_FACE_CX - 8, EXT_SLEEP_FACE_CY - 3,
                         EXT_SLEEP_FACE_CX - 4, EXT_SLEEP_FACE_CY - 3, WHITE);
    display_ext.drawLine(EXT_SLEEP_FACE_CX + 4, EXT_SLEEP_FACE_CY - 3,
                         EXT_SLEEP_FACE_CX + 8, EXT_SLEEP_FACE_CY - 3, WHITE);
    // Mouth
    display_ext.drawLine(EXT_SLEEP_FACE_CX - 4, EXT_SLEEP_FACE_CY + 5,
                         EXT_SLEEP_FACE_CX + 4, EXT_SLEEP_FACE_CY + 5, WHITE);
    // "Zzz" tag
    u8g2_ext.setFontMode(1);
    u8g2_ext.setFontDirection(0);
    u8g2_ext.setForegroundColor(WHITE);
    u8g2_ext.setBackgroundColor(BLACK);
    u8g2_ext.setFont(u8g2_font_6x10_mf);
    u8g2_ext.setCursor(EXT_SLEEP_ZZZ_X, EXT_SLEEP_ZZZ_Y);
    u8g2_ext.print(F("Zzz"));
    display_ext.display();
  }
}
// ---------------------
