#include "interface.h"

#include <Arduino.h>
#include <Adafruit_Sensor.h> // For sensors_event_t
#include <math.h>            // For atan2, sqrt, cos, sin, abs

#include "globals.h"
#include "hardware.h"
#include "mrfconstants.h"
#include "lenses.h"
#include "formats.h"
#include "helpers.h" // For getFocusRadius

struct ParallaxShift
{
  float x;
  float y;
};

static void getFormatLongShortMm(const FilmFormat &format, float &longMm, float &shortMm)
{
  if (format.frame_mm_width >= format.frame_mm_height)
  {
    longMm = format.frame_mm_width;
    shortMm = format.frame_mm_height;
  }
  else
  {
    longMm = format.frame_mm_height;
    shortMm = format.frame_mm_width;
  }
}

static const FilmFormat *getBaseFormat()
{
  for (size_t i = 0; i < NUM_FILM_FORMATS; ++i)
  {
    if (film_formats[i].id == 67)
    {
      return &film_formats[i];
    }
  }
  return &film_formats[selected_format];
}

static float getParallaxDistanceMm()
{
  // Prefer lens focus distance so framelines track focus changes, but only if calibrated.
  if (lenses[selected_lens].calibrated && lens_distance_raw == 9999999)
  {
    // Treat infinity as no parallax shift and avoid falling back to LiDAR.
    return 0.0f;
  }
  if (lenses[selected_lens].calibrated && lens_distance_raw > 0 && lens_distance_raw != 9999999)
  {
    float lens_mm = static_cast<float>(lens_distance_raw) * 10.0f;
    return max(lens_mm, PARALLAX_MIN_DISTANCE_MM);
  }

  if (distance > 0)
  {
    float lidar_mm = static_cast<float>(distance) * 10.0f;
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

  if ((PARALLAX_OFFSET_X_MM == 0.0f && PARALLAX_OFFSET_Y_MM == 0.0f) || lenses[selected_lens].focal_mm <= 0)
  {
    return shift;
  }

  float distance_mm = getParallaxDistanceMm();
  if (distance_mm <= 0.0f)
  {
    return shift;
  }

  float formatLongMm = 0.0f;
  float formatShortMm = 0.0f;
  getFormatLongShortMm(film_formats[selected_format], formatLongMm, formatShortMm);
  if (formatLongMm <= 0.0f || formatShortMm <= 0.0f)
  {
    return shift;
  }

  float shiftX_mm = lenses[selected_lens].focal_mm * PARALLAX_OFFSET_X_MM / distance_mm;
  float shiftY_mm = lenses[selected_lens].focal_mm * PARALLAX_OFFSET_Y_MM / distance_mm;

  float pxPerMmX = static_cast<float>(frameWidthPx) / formatLongMm;
  float pxPerMmY = static_cast<float>(frameHeightPx) / formatShortMm;

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
  display.fillRect(0, 0, 128, 15, WHITE);
  display.drawLine(64, 0, 64, 128, BLACK);
  u8g2.setCursor(2, 7);
  u8g2.print(F("ISO"));
  u8g2.print(iso);
  u8g2.setCursor(46, 7);
  u8g2.print(F("f"));
  if (aperture == static_cast<int>(aperture))
  {
    u8g2.print(static_cast<int>(aperture));
  }
  else
  {
    u8g2.print(aperture, 1);
  }
  u8g2.setCursor(2, 14);
  u8g2.print(shutter_speed);
  u8g2.setCursor(68, 7);
  u8g2.print(F("Dist:"));
  u8g2.print(distance_cm);
  u8g2.setCursor(68, 14);
  u8g2.print(F("Lens:"));
  u8g2.print(lens_distance_cm);

  int framelineX = lenses[selected_lens].framelines[0];
  int framelineY = lenses[selected_lens].framelines[1];
  int framelineW = lenses[selected_lens].framelines[2];
  int framelineH = lenses[selected_lens].framelines[3];
  int baseFramelineX = framelineX;
  int baseFramelineY = framelineY;

  float baseLongMm = 0.0f;
  float baseShortMm = 0.0f;
  float formatLongMm = 0.0f;
  float formatShortMm = 0.0f;
  const FilmFormat *baseFormat = getBaseFormat();
  getFormatLongShortMm(*baseFormat, baseLongMm, baseShortMm);
  getFormatLongShortMm(film_formats[selected_format], formatLongMm, formatShortMm);

  int new_width = framelineW;
  int new_height = framelineH;
  if (baseLongMm > 0.0f && baseShortMm > 0.0f)
  {
    float widthScale = formatLongMm / baseLongMm;
    float heightScale = formatShortMm / baseShortMm;
    new_width = static_cast<int>(roundf(framelineW * widthScale));
    new_height = static_cast<int>(roundf(framelineH * heightScale));
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
  int rectCenterY = (baseFramelineY + framelineH / 2 - 5) + RETICLE_OFFSET_Y;
 
  // Draw a circle at the center of the rectangle
  display.fillCircle(rectCenterX, rectCenterY, 3, WHITE);
  display.drawCircle(rectCenterX, rectCenterY, getFocusRadius(), WHITE);

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float x = a.acceleration.x;
  float y = a.acceleration.y;
  float z = a.acceleration.z;

  // Convert accelerometer readings into angles
  float pitch_scale = 25;
  float roll_scale = 0.5;
  float pitch_angle = atan2(x, sqrt(x*x + z*z)); // Renamed to avoid conflict
  float roll_angle = atan2(y, sqrt(x*x + z*z));  // Renamed to avoid conflict

  // Define the deadzone
  float deadzone = 0.03;

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
  float length = SCREEN_WIDTH - 10;

  // Calculate the start and end points of the line
  float startX = rectCenterX - length/2 * cos(roll_angle);
  float startY =  rectCenterY - length/2 * sin(roll_angle) + pitch_angle;
  float endX = rectCenterX + length/2 * cos(roll_angle);
  float endY =  rectCenterY + length/2 * sin(roll_angle) + pitch_angle;

  // Draw the line on the display
  display.drawLine(startX, startY, endX, endY, WHITE);   

  // Define the length of the vertical line
  int vertLineLength = 30;

  // Calculate the start and end points of the vertical line
  int vertLineStartY = rectCenterY - vertLineLength / 2;
  int vertLineEndY = rectCenterY + vertLineLength / 2;

  // Draw the vertical line on the display
  display.drawLine(rectCenterX, vertLineStartY, rectCenterX, vertLineEndY, WHITE);

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
  u8g2.setCursor(3, 15);
  u8g2.print(F("Setup"));

  u8g2.setFont(u8g2_font_4x6_mf);
  
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

  u8g2.setCursor(3, 26);
  setItemColors(config_step == 0);
  u8g2.print(F(" ISO:")); u8g2.print(iso); u8g2.print(F(" "));

  u8g2.setCursor(3, 37);
  setItemColors(config_step == 1);
  u8g2.print(F(" Format:")); u8g2.print(film_formats[selected_format].name); u8g2.print(F(" "));

  u8g2.setCursor(3, 48);
  setItemColors(config_step == 2);
  u8g2.print(F(" Lens:")); u8g2.print(lenses[selected_lens].name); u8g2.print(F(" "));

  u8g2.setCursor(3, 59);
  setItemColors(config_step == 3);
  u8g2.print(F(" Lens Calib. > "));

  u8g2.setCursor(3, 70);
  setItemColors(config_step == 4);
  u8g2.print(F(" Reset count >> "));

  u8g2.setCursor(3, 81);
  setItemColors(config_step == 5);
  u8g2.print(F(" Exit >> "));

  u8g2.setCursor(3, 100);
  u8g2.setBackgroundColor(BLACK); // Reset for footer
  u8g2.setForegroundColor(WHITE);
  u8g2.print(F(" IDENTIDEM.design MRF "));
  u8g2.print(FWVERSION);

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
  u8g2.setCursor(3, 15);
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

  u8g2.setCursor(3, 35);
  setItemColors(calib_step == 0);
  u8g2.print(F(" Lens:")); u8g2.print(lenses[calib_lens].name); u8g2.print(F(" "));

  u8g2.setCursor(3, 47);
  setItemColors(calib_step == 1);
  u8g2.print(F(" ")); u8g2.print(CALIB_DISTANCES[current_calib_distance], 1);
  u8g2.print(F("m: ")); u8g2.print(lens_sensor_reading); u8g2.print(F(" "));

  u8g2.setBackgroundColor(BLACK); // Reset for instructions
  u8g2.setForegroundColor(WHITE);

  if (calib_step == 0)
  {
    u8g2.setCursor(3, 70); u8g2.print(F(" (L) to Cycle"));
    u8g2.setCursor(3, 81); u8g2.print(F(" (R) to Select"));
  }
  else
  {
    u8g2.setCursor(3, 70); u8g2.print(F(" (L) to Select"));
    u8g2.setCursor(3, 81); u8g2.print(F(" (R) to Cancel"));
  }

  display.display();
}

void drawExternalUI()
{
  int progessBarWidth = 90;
  int progressBarHeight = 17;
  int progressBarX = 34;
  int progressBarY = 15;

  display_ext.clearDisplay();

  u8g2_ext.setFontMode(1);
  u8g2_ext.setFontDirection(0);
  u8g2_ext.setForegroundColor(BLACK);
  u8g2_ext.setBackgroundColor(WHITE);
  u8g2_ext.setFont(u8g2_font_6x10_mf);

  u8g2_ext.setCursor(2, 8);
  display_ext.fillRect(0, 0, 128, 10, WHITE);
  u8g2_ext.print(film_formats[selected_format].name);

  display_ext.drawLine(33, 0, 33, 10, BLACK);

  u8g2_ext.setCursor(37, 8);
  u8g2_ext.print(lenses[selected_lens].name);

  // Adjust cursor for battery percentage display
  if (bat_per == 100) {
    display_ext.drawLine(100, 0, 100, 10, BLACK); u8g2_ext.setCursor(104, 8);
  } else if (bat_per < 10) {
    display_ext.drawLine(111, 0, 111, 10, BLACK); u8g2_ext.setCursor(115, 8);
  } else {
    display_ext.drawLine(103, 0, 103, 10, BLACK); u8g2_ext.setCursor(107, 8);
  }
  u8g2_ext.print(bat_per); u8g2_ext.print(F("%"));

  if (frame_progress > 0)
  {
    float progressPercentage = frame_progress * 100;
    int progressWidth = progessBarWidth * (progressPercentage / 100);
    display_ext.drawRect(progressBarX, progressBarY, progessBarWidth, progressBarHeight, WHITE);
    display_ext.fillRect(progressBarX, progressBarY, progressWidth, progressBarHeight, WHITE);

    if (frame_progress != prev_frame_progress) {
      if (progressPercentage > 0 && progressPercentage < 100) {
        int greenValue = frame_progress * 255;
        int redValue = (1 - frame_progress) * 255;
        sspixel.setPixelColor(0, sspixel.Color(redValue, greenValue, 0));
      }
      prev_frame_progress = frame_progress;
    }
  }
  else
  {
    sspixel.setPixelColor(0, sspixel.Color(0, 0, 255)); // Blue for no progress
    // No need to set cursor if nothing is printed here for this case
  }

  u8g2_ext.setForegroundColor(WHITE); // Text on black background for counter
  u8g2_ext.setBackgroundColor(BLACK);
  u8g2_ext.setFont(u8g2_font_10x20_mf);

  if (film_counter == 0 && frame_progress == 0) {
    u8g2_ext.setCursor(8, 30); u8g2_ext.print(F(" Load film."));
    sspixel.setPixelColor(0, sspixel.Color(238, 130, 238)); // Violet
  } else if (film_counter == 99) {
    u8g2_ext.setCursor(8, 30); u8g2_ext.print(F(" Roll end."));
    sspixel.setPixelColor(0, sspixel.Color(238, 130, 238)); // Violet
  } else {
    u8g2_ext.setCursor( (frame_progress > 0) ? 8 : 60, 30); // Adjust cursor based on progress bar
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

  u8g2_ext.setCursor(8, 22);
  u8g2_ext.print(F("ZzzZZzZz..."));

  display.display();
  display_ext.display();

  sspixel.setPixelColor(0, sspixel.Color(0, 0, 0)); // Off
  sspixel.show();
}
// ---------------------
