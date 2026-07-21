// Pixel-exact user-manual screenshot generator (host, no hardware).
//
// Compiles the REAL firmware draw code (interface.cpp,
// interface_config_screens.cpp) against software-canvas display fakes, poses
// the global state for each screen to the manual's example camera
// (ISO400, f8, 1/125, 6x7, 65/6.3, 2.5 m), calls each parameterless draw
// function, and writes a pixel-exact SVG over the matching hand-drawn file in
// Documentation/user-manual/images/.
//
// Regenerate (from Firmware/):
//   pio run -e native_screenshots && .pio/build/native_screenshots/program
// or just: scripts/generate-screenshots.sh
//
// An optional argv[1] overrides the output directory (default
// ../Documentation/user-manual/images relative to Firmware/).

#include <cstring>
#include <fstream>
#include <string>

#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "interface.h"
#include "lenses.h"
#include "mrfconstants.h"

#include "screenshots/screenshot_svg_logic.h"

// ---------------------------------------------------------------------------
// Hardware objects declared extern in hardware.h. On device these live in
// hardware.cpp; here they are the inert / canvas-backed fakes.
// ---------------------------------------------------------------------------
TwoWire Wire;

Adafruit_seesaw encoder;
Adafruit_ADS1015 theads;
Adafruit_MPU6050 mpu;
seesaw_NeoPixel sspixel;
Bounce2::Button lbutton;
Bounce2::Button rbutton;
Adafruit_MAX17048 maxlipo;
BH1750 lightMeter;
DTS6012M_UART lidar;
Adafruit_SH1107 display(128, 128, &Wire);
Adafruit_SSD1306 display_ext(128, 32, &Wire);
U8G2_FOR_ADAFRUIT_GFX u8g2;
U8G2_FOR_ADAFRUIT_GFX u8g2_ext;

// getFocusRadius() lives in helpers.cpp (heavy hardware deps we don't link).
// The focus ring only needs a fixed, plausible radius for the main-UI shot.
int_fast16_t getFocusRadius()
{
  return 18;
}

namespace
{
std::string g_output_dir = "../Documentation/user-manual/images";

void writeSvg(const std::string &name, const CanvasDisplay &canvas)
{
  std::string svg = renderFrameBufferSvg(canvas.getBuffer(), canvas.width(),
                                         canvas.height(), canvas.getBytesPerRow(), 4);
  std::string path = g_output_dir + "/" + name + ".svg";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << svg;
}

// Common camera state shared by every screen. Individual fixtures override the
// few fields they exercise.
void poseCommonState()
{
  // Lightmeter
  iso = 400;
  aperture = 8.0f;
  std::strcpy(shutter_speed, "1/125");
  exposure_comp_thirds = 0;
  show_ev_readout = false;

  // Lens / format
  selected_lens = 1;   // 65/6.3 (calibrated)
  selected_format = 4; // 6x7
  lens_focus_offset = 0;
  parallaxEnabled = true;
  std::strcpy(lens_distance_cm, "2.5m");
  lens_distance_raw = 250;

  // LiDAR
  distance = 250;
  std::strcpy(distance_cm, "2.5m");
  lidar_quality_level = 3;
  lidar_distance_held = false;
  lidar_high_sunlight = false;
  lidarEnabled = true;

  // Frame counter / battery for the external display
  film_counter = 8;
  frame_progress = 0.4f;
  prev_frame_progress = -1.0f;
  bat_per = 78;

  // Reticle / display
  reticle_offset_x = DEFAULT_RETICLE_OFFSET_X; // -5
  reticle_offset_y = DEFAULT_RETICLE_OFFSET_Y; // 0
  reticle_adjust_step = 0;
  show_horizon_line = true;

  // Navigation
  config_step = 0;
  calib_step = 0;
  calib_lens = 1;
  current_calib_distance = 0;
  calib_capture_status = CALIB_CAPTURE_STATUS_NONE;

  // Health / diagnostics: healthy, all peripherals present.
  hardware.ads = true;
  hardware.mpu = true;
  hardware.mainDisplay = true;
  hardware.externalDisplay = true;
  hardware.batteryGauge = true;
  hardware.lightMeter = true;
  hardware.statusPixel = true;
  hardware.encoder = true;
  hardware.lidarSensor = true;

  prefsSchemaValid = true;
  prefsLoadedLegacy = false;
  prefsSchemaVersionLoaded = PREFS_SCHEMA_VERSION;
  last_lidar_error_code = 0;
  lidar_recovery_count = 0;
  std::strcpy(lidar_sensor_fw_version, "0102");

  // Level device (landscape); portrait shot re-poses this.
  g_stub_accel_x = 0.0f;
  g_stub_accel_y = 0.0f;
  g_stub_accel_z = 9.81f;
  g_stub_millis = 100000UL;
}
} // namespace

int main(int argc, char **argv)
{
  if (argc > 1)
  {
    g_output_dir = argv[1];
  }

  u8g2.begin(display);
  u8g2_ext.begin(display_ext);

  // ---- Main UI (landscape) ----
  poseCommonState();
  drawMainUI();
  writeSvg("main-ui", display);

  // ---- Main UI (portrait) ----
  poseCommonState();
  g_stub_accel_x = 0.0f;
  g_stub_accel_y = 9.81f; // rolled ~90deg
  g_stub_accel_z = 0.0f;
  drawMainUI();
  writeSvg("main-ui-portrait", display);

  // ---- External display ----
  poseCommonState();
  drawExternalUI();
  writeSvg("external-ui", display_ext);

  // ---- Sleep ----
  poseCommonState();
  drawSleepUI();
  writeSvg("sleep-ui", display_ext);

  // ---- Setup root ----
  poseCommonState();
  drawConfigUI();
  writeSvg("config-ui", display);

  // ---- Setup > Film ----
  poseCommonState();
  drawFilmConfigUI();
  writeSvg("config-film-ui", display);

  // ---- Setup > Lens ----
  poseCommonState();
  drawLensConfigUI();
  writeSvg("config-lens-ui", display);

  // ---- Setup > Meter ----
  poseCommonState();
  drawMeterConfigUI();
  writeSvg("config-meter-ui", display);

  // ---- Setup > LiDAR ----
  poseCommonState();
  drawLidarConfigUI();
  writeSvg("config-lidar-ui", display);

  // ---- Setup > LiDAR > Diagnostics ----
  poseCommonState();
  lidar_raw_distance_mm = 2480;
  std::strcpy(distance_cm, "2.4m");
  lidar_primary_intensity = 142;
  lidar_sunlight_base = 1200;
  lidar_snr_permille = 118;
  lidar_quality_level = 3;
  lidar_frame_rate_measured = 50;
  lidar_telemetry_ms = g_stub_millis - 84; // Age: 84ms
  drawLidarDiagnosticsUI();
  writeSvg("config-lidar-diagnostics-ui", display);

  // ---- Setup > Display ----
  poseCommonState();
  drawDisplayConfigUI();
  writeSvg("config-display-ui", display);

  // ---- Lens calibration: select lens (point 1/7) ----
  poseCommonState();
  calib_step = 0;
  current_calib_distance = 0;
  lens_sensor_reading = 312;
  drawCalibUI();
  writeSvg("calib-select-lens", display);

  // ---- Lens calibration: capture (point 4/7) ----
  poseCommonState();
  calib_step = 1;
  current_calib_distance = 3;
  lens_sensor_reading = 287;
  drawCalibUI();
  writeSvg("calib-distance", display);

  // ---- Calibration complete ----
  poseCommonState();
  drawCalibCompleteUI();
  writeSvg("calib-complete", display);

  // ---- Focus reticle adjust ----
  poseCommonState();
  drawReticleAdjustUI();
  writeSvg("config-reticle-adjust", display);

  // ---- System Health ----
  poseCommonState();
  drawHealthUI();
  writeSvg("health-ui", display);

  // ---- Reset frame counter confirm ----
  poseCommonState();
  drawResetConfirmUI();
  writeSvg("reset-confirm-ui", display);

  // ---- Factory reset confirm (new) ----
  poseCommonState();
  drawFactoryResetConfirmUI();
  writeSvg("factory-reset-confirm-ui", display);

  return 0;
}
