#include "interface.h"

#include <Arduino.h>
#include <Adafruit_Sensor.h> // For sensors_event_t
#include <math.h>            // For atan2, sqrt, cos, sin, abs
#include <string.h>          // For strlen, strcmp, memcpy

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h"
#include "formats.h"
#include "helpers.h" // For getFocusRadius
#include "lightmeter_logic.h"
#include "cyclefuncs.h"
#include "activity.h"

struct ParallaxShift
{
  float x;
  float y;
};

const unsigned long SLEEP_TEXT_ANIMATION_INTERVAL_MS = 5000;

static String getCompactShutterDisplay(const String &fullShutter)
{
  const char *suffix = " sec.";
  const size_t suffixLen = 5;
  const char *raw = fullShutter.c_str();
  size_t rawLen = strlen(raw);

  if (rawLen > suffixLen && strcmp(raw + (rawLen - suffixLen), suffix) == 0)
  {
    char compact[16] = {0};
    size_t copyLen = min(rawLen - suffixLen, sizeof(compact) - 1);
    memcpy(compact, raw, copyLen);
    return String(compact);
  }

  return fullShutter;
}

static void getFormatWidthHeightMm(const FilmFormat &format, float &widthMm, float &heightMm)
{
  widthMm = format.frame_mm_width;
  heightMm = format.frame_mm_height;
}

static float getParallaxDistanceMm()
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

static ParallaxShift computeParallaxShiftPx(int frameWidthPx, int frameHeightPx)
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

static void drawLidarQualityIndicator()
{
  const int maxBlocks = 4;
  int blocks = lidar_quality_level;
  if (blocks < 0)
  {
    blocks = 0;
  }
  else if (blocks > maxBlocks)
  {
    blocks = maxBlocks;
  }

  for (int i = 0; i < maxBlocks; i++)
  {
    int x = MAIN_LIDAR_QUALITY_X;
    int y = MAIN_LIDAR_QUALITY_Y + (i * (MAIN_LIDAR_QUALITY_SIZE + MAIN_LIDAR_QUALITY_GAP));
    if (i >= (maxBlocks - blocks))
    {
      display.fillRect(x, y, MAIN_LIDAR_QUALITY_SIZE, MAIN_LIDAR_QUALITY_SIZE, BLACK);
    }
    else
    {
      display.fillRect(x, y, MAIN_LIDAR_QUALITY_SIZE, MAIN_LIDAR_QUALITY_SIZE, WHITE);
    }
  }
}

// Functions to draw UI
// ---------------------
void drawMainUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(BLACK); // U8G2 uses 0/1, GFX uses BLACK/WHITE defines
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
    String compactShutter = getCompactShutterDisplay(shutter_speed);
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

  int framelineX = lenses[selected_lens].framelines[0];
  int framelineY = lenses[selected_lens].framelines[1];
  int framelineW = lenses[selected_lens].framelines[2];
  int framelineH = lenses[selected_lens].framelines[3];
  int baseFramelineX = framelineX;
  int baseFramelineY = framelineY;

  float formatWidthMm = 0.0f;
  float formatHeightMm = 0.0f;
  getFormatWidthHeightMm(film_formats[selected_format], formatWidthMm, formatHeightMm);

  int baseFormatIndex = DEFAULT_SELECTED_FORMAT;
  if (baseFormatIndex < 0 || baseFormatIndex >= static_cast<int>(NUM_FILM_FORMATS))
  {
    baseFormatIndex = selected_format;
  }

  float baseFormatWidthMm = 0.0f;
  float baseFormatHeightMm = 0.0f;
  getFormatWidthHeightMm(film_formats[baseFormatIndex], baseFormatWidthMm, baseFormatHeightMm);

  int new_width = framelineW;
  int new_height = framelineH;
  bool allowOverflow = false;
  if (formatWidthMm > 0.0f && formatHeightMm > 0.0f)
  {
    float formatRatio = formatWidthMm / formatHeightMm;
    float baseRatio = static_cast<float>(framelineW) / static_cast<float>(framelineH);

    if (baseFormatWidthMm > 0.0f && baseFormatHeightMm > 0.0f)
    {
      float baseFormatRatio = baseFormatWidthMm / baseFormatHeightMm;
      allowOverflow = formatRatio > baseFormatRatio && formatHeightMm >= baseFormatHeightMm;
    }

    if (allowOverflow)
    {
      new_height = framelineH;
      new_width = static_cast<int>(roundf(framelineH * formatRatio));
    }
    else if (formatRatio >= baseRatio)
    {
      new_width = framelineW;
      new_height = static_cast<int>(roundf(framelineW / formatRatio));
    }
    else
    {
      new_height = framelineH;
      new_width = static_cast<int>(roundf(framelineH * formatRatio));
    }
  }

  if (allowOverflow)
  {
    new_width = max(1, new_width);
    new_height = max(1, min(framelineH, new_height));
  }
  else
  {
    new_width = max(1, min(framelineW, new_width));
    new_height = max(1, min(framelineH, new_height));
  }

  ParallaxShift parallax = computeParallaxShiftPx(new_width, new_height);
  int parallaxX = static_cast<int>(roundf(parallax.x));
  int parallaxY = static_cast<int>(roundf(parallax.y));

  framelineX += parallaxX;
  framelineY += parallaxY;

  display.fillRect(
    framelineX, 
    framelineY, 
    framelineW,
    framelineH, 
    WHITE
  );

  float frameCenterX = framelineX + framelineW / 2.0f;
  float frameCenterY = framelineY + framelineH / 2.0f;
  int width_offset = static_cast<int>(roundf(frameCenterX - new_width / 2.0f));
  int heigh_offset = static_cast<int>(roundf(frameCenterY - new_height / 2.0f));
  
  display.fillRect(
    width_offset,
    heigh_offset,
    new_width,
    new_height, 
    BLACK
  );
  display.drawRect(
    width_offset,
    heigh_offset,
    new_width,
    new_height,
    WHITE
  );

  // Calculate the center of the rectangle
  int rectCenterX = (baseFramelineX + framelineW / 2) + RETICLE_OFFSET_X;
  int rectCenterY = (baseFramelineY + framelineH / 2 - MAIN_RETICLE_CENTER_Y_OFFSET) + RETICLE_OFFSET_Y;
 
  // Draw a circle at the center of the rectangle
  display.fillCircle(rectCenterX, rectCenterY, MAIN_RETICLE_CENTER_RADIUS, INVERSE);
  int focusRadius = getFocusRadius();
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
  focusThickness = max(FOCUS_RING_THICKNESS_MIN, min(FOCUS_RING_THICKNESS_MAX, focusThickness));
  int outerRadius = focusRadius;
  int innerRadius = focusRadius - focusThickness;
  if (outerRadius >= 1)
  {
    display.fillCircle(rectCenterX, rectCenterY, outerRadius, INVERSE);
  }
  if (innerRadius >= 1)
  {
    display.fillCircle(rectCenterX, rectCenterY, innerRadius, INVERSE);
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float x = a.acceleration.x;
  float y = a.acceleration.y;
  float z = a.acceleration.z;

  // Convert accelerometer readings into angles
  float pitch_scale = LEVEL_PITCH_SCALE;
  float roll_scale = LEVEL_ROLL_SCALE;
  float pitch_angle = atan2(x, sqrt(x*x + z*z)); // Renamed to avoid conflict
  float roll_angle = atan2(y, sqrt(x*x + z*z));  // Renamed to avoid conflict

  // Define the deadzone
  float deadzone = LEVEL_DEADZONE;

  // Apply the deadzone to the pitch and roll
  if (abs(pitch_angle) < deadzone) {
    pitch_angle = 0;
  }
  if (abs(roll_angle) < deadzone) {
    roll_angle = 0;
  }

  pitch_angle = pitch_angle * pitch_scale;
  roll_angle = roll_angle * roll_scale;

  // Define the length of the line
  float length = SCREEN_WIDTH - LEVEL_LINE_MARGIN_PX;

  // Calculate the start and end points of the line
  float startX = rectCenterX - length/2 * cos(roll_angle);
  float startY =  rectCenterY - length/2 * sin(roll_angle) + pitch_angle;
  float endX = rectCenterX + length/2 * cos(roll_angle);
  float endY =  rectCenterY + length/2 * sin(roll_angle) + pitch_angle;

  // Draw the line on the display
  display.drawLine(startX, startY, endX, endY, INVERSE);   

  // Define the length of the vertical line
  int vertLineLength = LEVEL_VERTICAL_LINE_LENGTH;

  // Calculate the start and end points of the vertical line
  int vertLineStartY = rectCenterY - vertLineLength / 2;
  int vertLineEndY = rectCenterY + vertLineLength / 2;

  // Draw the vertical line on the display
  display.drawLine(rectCenterX, vertLineStartY, rectCenterX, vertLineEndY, INVERSE);

  display.display();
}

void drawConfigUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

  u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(CONFIG_TITLE_X, CONFIG_TITLE_Y);
  u8g2.print(F("Setup"));

  u8g2.setFont(u8g2_font_4x6_mf);
  const int menu_item_y_start = CONFIG_ITEM_Y_START + CONFIG_HEADER_PADDING_Y;
  
  // Helper lambda to set colors based on selection
  auto setItemColors = [&](bool selected) {
    if (selected) {
      u8g2.setBackgroundColor(WHITE);
      u8g2.setForegroundColor(BLACK);
    } else {
      u8g2.setBackgroundColor(BLACK);
      u8g2.setForegroundColor(WHITE);
    }
  };

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_ISO));
  setItemColors(config_step == CONFIG_ROOT_STEP_ISO);
  u8g2.print(F(" ISO:")); u8g2.print(iso); u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_FORMAT));
  setItemColors(config_step == CONFIG_ROOT_STEP_FORMAT);
  u8g2.print(F(" Format:")); u8g2.print(film_formats[selected_format].name); u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_SLEEP_TIMEOUT));
  setItemColors(config_step == CONFIG_ROOT_STEP_SLEEP_TIMEOUT);
  u8g2.print(F(" Sleep timeout:"));
  u8g2.print(getSleepTimeoutModeLabel(sleep_timeout_mode));
  u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_LENS_MENU));
  setItemColors(config_step == CONFIG_ROOT_STEP_LENS_MENU);
  u8g2.print(F(" Lens Settings > "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_METER_MENU));
  setItemColors(config_step == CONFIG_ROOT_STEP_METER_MENU);
  u8g2.print(F(" Light Meter > "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_RESET));
  setItemColors(config_step == CONFIG_ROOT_STEP_RESET);
  u8g2.print(F(" Reset frame counter >> "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_HEALTH));
  setItemColors(config_step == CONFIG_ROOT_STEP_HEALTH);
  u8g2.print(F(" System Health > "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_ROOT_STEP_EXIT));
  setItemColors(config_step == CONFIG_ROOT_STEP_EXIT);
  u8g2.print(F(" Exit >> "));

  u8g2.setCursor(CONFIG_ITEM_X, CONFIG_FOOTER_Y);
  u8g2.setBackgroundColor(BLACK); // Reset for footer
  u8g2.setForegroundColor(WHITE);
  u8g2.print(F(" IDENTIDEM.design MRF "));
  u8g2.print(FWVERSION);

  display.display();
}

void drawLensConfigUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

  u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(CONFIG_TITLE_X, CONFIG_TITLE_Y);
  u8g2.print(F("Lens"));

  u8g2.setFont(u8g2_font_4x6_mf);
  const int menu_item_y_start = CONFIG_ITEM_Y_START + CONFIG_HEADER_PADDING_Y;

  auto setItemColors = [&](bool selected) {
    if (selected) {
      u8g2.setBackgroundColor(WHITE);
      u8g2.setForegroundColor(BLACK);
    } else {
      u8g2.setBackgroundColor(BLACK);
      u8g2.setForegroundColor(WHITE);
    }
  };

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_LENS_STEP_LENS));
  setItemColors(config_step == CONFIG_LENS_STEP_LENS);
  u8g2.print(F(" Lens:")); u8g2.print(lenses[selected_lens].name); u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_LENS_STEP_PARALLAX));
  setItemColors(config_step == CONFIG_LENS_STEP_PARALLAX);
  u8g2.print(F(" Parallax: "));
  u8g2.print(parallaxEnabled ? F("On") : F("Off"));
  u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_LENS_STEP_CALIB));
  setItemColors(config_step == CONFIG_LENS_STEP_CALIB);
  u8g2.print(F(" Lens Calibration > "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_LENS_STEP_BACK));
  setItemColors(config_step == CONFIG_LENS_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawMeterConfigUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

  u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(CONFIG_TITLE_X, CONFIG_TITLE_Y);
  u8g2.print(F("Meter"));

  u8g2.setFont(u8g2_font_4x6_mf);
  const int menu_item_y_start = CONFIG_ITEM_Y_START + CONFIG_HEADER_PADDING_Y;

  auto setItemColors = [&](bool selected) {
    if (selected) {
      u8g2.setBackgroundColor(WHITE);
      u8g2.setForegroundColor(BLACK);
    } else {
      u8g2.setBackgroundColor(BLACK);
      u8g2.setForegroundColor(WHITE);
    }
  };

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_METER_STEP_EV_COMP));
  setItemColors(config_step == CONFIG_METER_STEP_EV_COMP);
  u8g2.print(F(" EV Comp:"));
  float evComp = static_cast<float>(exposure_comp_thirds) / 3.0f;
  if (evComp >= 0.0f)
  {
    u8g2.print(F("+"));
  }
  u8g2.print(evComp, 1);
  u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_METER_STEP_SMOOTHING));
  setItemColors(config_step == CONFIG_METER_STEP_SMOOTHING);
  u8g2.print(F(" Smoothing:"));
  u8g2.print(getMeterSmoothingLabel(meter_smoothing_mode));
  u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_METER_STEP_EV_READOUT));
  setItemColors(config_step == CONFIG_METER_STEP_EV_READOUT);
  u8g2.print(F(" EV Readout:"));
  u8g2.print(show_ev_readout ? F("On") : F("Off"));
  u8g2.print(F(" "));

  u8g2.setCursor(CONFIG_ITEM_X, menu_item_y_start + (CONFIG_ITEM_Y_STEP * CONFIG_METER_STEP_BACK));
  setItemColors(config_step == CONFIG_METER_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawCalibUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(CALIB_TITLE_X, CALIB_TITLE_Y);
  u8g2.print(F("Calibrate"));

  u8g2.setFont(u8g2_font_4x6_mf);

  auto setItemColors = [&](bool selected) {
    if (selected) {
      u8g2.setBackgroundColor(WHITE);
      u8g2.setForegroundColor(BLACK);
    } else {
      u8g2.setBackgroundColor(BLACK);
      u8g2.setForegroundColor(WHITE);
    }
  };

  u8g2.setCursor(CALIB_ITEM_X, CALIB_LENS_Y);
  setItemColors(calib_step == 0);
  u8g2.print(F(" Lens:")); u8g2.print(lenses[calib_lens].name); u8g2.print(F(" "));

  u8g2.setCursor(CALIB_ITEM_X, CALIB_DISTANCE_Y);
  setItemColors(calib_step == 1);
  u8g2.print(F(" ")); u8g2.print(CALIB_DISTANCES[current_calib_distance], DISTANCE_DECIMAL_PLACES);
  u8g2.print(F("m: ")); u8g2.print(lens_sensor_reading); u8g2.print(F(" "));

  u8g2.setBackgroundColor(BLACK); // Reset for instructions
  u8g2.setForegroundColor(WHITE);

  if (calib_step == 0)
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1); u8g2.print(F(" (L) to Cycle"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2); u8g2.print(F(" (R) to Select"));
  }
  else
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1); u8g2.print(F(" (L) to Select"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2); u8g2.print(F(" (R) to Cancel"));

    if (calib_capture_status == CALIB_CAPTURE_STATUS_UNSTABLE)
    {
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1); u8g2.print(F(" Unstable reading"));
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2); u8g2.print(F(" Hold still + retry"));
    }
    else if (calib_capture_status == CALIB_CAPTURE_STATUS_NON_MONOTONIC)
    {
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1); u8g2.print(F(" Out of sequence"));
      u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2); u8g2.print(F(" Recheck distance"));
    }
  }

  display.display();
}

void drawResetConfirmUI()
{
  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

  u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(CONFIG_TITLE_X, CONFIG_TITLE_Y);
  u8g2.print(F("Reset Count?"));

  u8g2.setFont(u8g2_font_4x6_mf);
  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y1);
  u8g2.print(F(" (L) Cancel"));
  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y2);
  u8g2.print(F(" (R) Reset"));

  display.display();
}

void drawHealthUI()
{
  const unsigned long now = millis();
  const unsigned long idleMs = getIdleDurationMs(now);

  display.clearDisplay();

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);

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
  u8g2.print(lidarEnabled ? F("On") : F("Off"));
  u8g2.print(F(" err:"));
  u8g2.print(last_lidar_error_code);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 3));
  u8g2.print(F("Recoveries: "));
  u8g2.print(lidar_recovery_count);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 4));
  u8g2.print(F("Idle: "));
  u8g2.print(idleMs / 1000);
  u8g2.print(F("s"));

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 5));
  u8g2.print(F("Watchdog: "));
  u8g2.print(watchdogEnabled ? F("On") : F("Off"));

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_FOOTER_Y);
  u8g2.print(F(" (L/R) Back"));

  display.display();
}

void drawExternalUI()
{
  int progessBarWidth = EXT_PROGRESS_BAR_WIDTH;
  int progressBarHeight = EXT_PROGRESS_BAR_HEIGHT;
  int progressBarX = EXT_PROGRESS_BAR_X;
  int progressBarY = EXT_PROGRESS_BAR_Y;

  display_ext.clearDisplay();

  u8g2_ext.setFontMode(1);
  u8g2_ext.setFontDirection(0);
  u8g2_ext.setForegroundColor(BLACK);
  u8g2_ext.setBackgroundColor(WHITE);
  u8g2_ext.setFont(u8g2_font_6x10_mf);

  u8g2_ext.setCursor(EXT_HEADER_FORMAT_X, EXT_HEADER_FORMAT_Y);
  display_ext.fillRect(0, 0, SCREEN_WIDTH, EXT_HEADER_HEIGHT, WHITE);
  u8g2_ext.print(film_formats[selected_format].name);

  display_ext.drawLine(EXT_HEADER_DIVIDER_X, 0, EXT_HEADER_DIVIDER_X, EXT_HEADER_HEIGHT, BLACK);

  u8g2_ext.setCursor(EXT_HEADER_LENS_X, EXT_HEADER_LENS_Y);
  u8g2_ext.print(lenses[selected_lens].name);

  // Adjust cursor for battery percentage display
  if (bat_per == BATTERY_PERCENT_MAX) {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_FULL_X, 0, EXT_BATTERY_DIVIDER_FULL_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_FULL_X, EXT_HEADER_FORMAT_Y);
  } else if (bat_per < BATTERY_PERCENT_LOW_THRESHOLD) {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_LOW_X, 0, EXT_BATTERY_DIVIDER_LOW_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_LOW_X, EXT_HEADER_FORMAT_Y);
  } else {
    display_ext.drawLine(EXT_BATTERY_DIVIDER_MID_X, 0, EXT_BATTERY_DIVIDER_MID_X, EXT_HEADER_HEIGHT, BLACK);
    u8g2_ext.setCursor(EXT_BATTERY_CURSOR_MID_X, EXT_HEADER_FORMAT_Y);
  }
  u8g2_ext.print(bat_per); u8g2_ext.print(F("%"));

  if (frame_progress > 0)
  {
    float progressPercentage = frame_progress * PERCENT_SCALE;
    int progressWidth = progessBarWidth * (progressPercentage / PERCENT_SCALE);
    display_ext.drawRect(progressBarX, progressBarY, progessBarWidth, progressBarHeight, WHITE);
    display_ext.fillRect(progressBarX, progressBarY, progressWidth, progressBarHeight, WHITE);

    if (frame_progress != prev_frame_progress) {
      if (progressPercentage > 0 && progressPercentage < PERCENT_SCALE) {
        int greenValue = frame_progress * NEOPIXEL_COLOR_MAX;
        int redValue = (1 - frame_progress) * NEOPIXEL_COLOR_MAX;
        sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(redValue, greenValue, NEOPIXEL_OFF_B));
      }
      prev_frame_progress = frame_progress;
    }
  }
  else
  {
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_BLUE_R, NEOPIXEL_BLUE_G, NEOPIXEL_BLUE_B)); // Blue for no progress
    // No need to set cursor if nothing is printed here for this case
  }

  u8g2_ext.setForegroundColor(WHITE); // Text on black background for counter
  u8g2_ext.setBackgroundColor(BLACK);
  u8g2_ext.setFont(u8g2_font_10x20_mf);

  if (film_counter == 0 && frame_progress == 0) {
    u8g2_ext.setCursor(EXT_COUNTER_MESSAGE_X, EXT_COUNTER_TEXT_Y); u8g2_ext.print(F(" Load film."));
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_VIOLET_R, NEOPIXEL_VIOLET_G, NEOPIXEL_VIOLET_B)); // Violet
  } else if (film_counter == FILM_COUNTER_END) {
    u8g2_ext.setCursor(EXT_COUNTER_MESSAGE_X, EXT_COUNTER_TEXT_Y); u8g2_ext.print(F(" Roll end."));
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_VIOLET_R, NEOPIXEL_VIOLET_G, NEOPIXEL_VIOLET_B)); // Violet
  } else {
    u8g2_ext.setCursor((frame_progress > 0) ? EXT_COUNTER_VALUE_X_WITH_PROGRESS : EXT_COUNTER_VALUE_X_NO_PROGRESS, EXT_COUNTER_TEXT_Y); // Adjust cursor based on progress bar
    u8g2_ext.print(film_counter);
  }

  sspixel.show();
  display_ext.display();
}

void drawSleepUI()
{
  display.clearDisplay();
  display_ext.clearDisplay();

  u8g2_ext.setFontMode(1);
  u8g2_ext.setFontDirection(0);
  u8g2_ext.setForegroundColor(WHITE);
  u8g2_ext.setBackgroundColor(BLACK);
  u8g2_ext.setFont(u8g2_font_10x20_mf);

  const unsigned long animationPhase = millis() / SLEEP_TEXT_ANIMATION_INTERVAL_MS;
  const char *sleepLabel = (animationPhase % 2 == 0) ? "ZzzZZzZz" : "zZZzzZzZ";
  char animatedDots[4] = {' ', ' ', ' ', '\0'};
  int dotCount = static_cast<int>(animationPhase % 3) + 1;
  for (int i = 0; i < dotCount; i++)
  {
    animatedDots[i] = '.';
  }

  u8g2_ext.setCursor(EXT_SLEEP_TEXT_X, EXT_SLEEP_TEXT_Y);
  u8g2_ext.print(sleepLabel);
  u8g2_ext.print(animatedDots);

  display.display();
  display_ext.display();

  sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_OFF_R, NEOPIXEL_OFF_G, NEOPIXEL_OFF_B)); // Off
  sspixel.show();
}
// ---------------------
