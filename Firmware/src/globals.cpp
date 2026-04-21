#include "globals.h"
#include <Arduino.h>
#include <Preferences.h> // For Preferences object type
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

// Preferences
Preferences prefs;

// Variables
// ---------------------
// Lightmeter
int prev_iso = DEFAULT_ISO;
int iso = DEFAULT_ISO;
float prev_aperture;
float aperture;
float lux = 0;
float ev_readout = 0;
char shutter_speed[16] = "...";
int iso_index = DEFAULT_ISO_INDEX;
int aperture_index;
int exposure_comp_thirds = DEFAULT_EXPOSURE_COMP_THIRDS;
int meter_smoothing_mode = DEFAULT_METER_SMOOTHING_MODE;
bool show_ev_readout = DEFAULT_SHOW_EV_READOUT;
int sleep_timeout_mode = DEFAULT_SLEEP_TIMEOUT_MODE;
int lidar_idle_timeout_mode = DEFAULT_LIDAR_IDLE_TIMEOUT_MODE;
int level_trim_landscape_deci_deg = DEFAULT_LEVEL_TRIM_LANDSCAPE_DECI_DEG;
int level_trim_portrait_pos_deci_deg = DEFAULT_LEVEL_TRIM_PORTRAIT_POS_DECI_DEG;
int level_trim_portrait_neg_deci_deg = DEFAULT_LEVEL_TRIM_PORTRAIT_NEG_DECI_DEG;
int reticle_offset_x = DEFAULT_RETICLE_OFFSET_X;
int reticle_offset_y = DEFAULT_RETICLE_OFFSET_Y;
bool brightness_auto = DEFAULT_BRIGHTNESS_AUTO;
int brightness_manual_pct = DEFAULT_BRIGHTNESS_MANUAL_PCT;
int brightness_auto_top_pct = DEFAULT_BRIGHTNESS_AUTO_TOP_PCT;
bool show_horizon_line = DEFAULT_SHOW_HORIZON_LINE;

// Filter algorithm
int samples[SMOOTHING_WINDOW_SIZE];
int curReadIndex = 0;
int sampleTotal = 0;
int sampleAvg = 0;

// Lens distance
int prev_lens_sensor_reading = 0;
int lens_sensor_reading = 0;
int lens_distance_raw = 0;
char lens_distance_cm[16] = "...";

// LiDAR distance
int prev_distance = 0;
int16_t distance = 0;    // Distance to object in centimeters
char distance_cm[16] = "...";
int lidar_quality_level = 0;
bool lidarEnabled = true;

// Battery gauge
int bat_per = 0;

// Camera state
UiMode ui_mode = UiMode::Main;
int config_step = 0;
int calib_step = 0;
int reticle_adjust_step = 0;
int selected_lens = DEFAULT_SELECTED_LENS;
int selected_format = DEFAULT_SELECTED_FORMAT;
int calib_lens = 0;
bool parallaxEnabled = true;

int calib_distance_set[CALIB_DISTANCE_COUNT] = {};
int current_calib_distance = 0;
int calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
unsigned long calib_capture_status_ms = 0;

int film_counter = 0;
int prev_encoder_value = 0;
int encoder_value = 0;
float frame_progress = 0;
float prev_frame_progress = 0;
int frame_one_offset = DEFAULT_FRAME_ONE_OFFSET;
int frame_spacing_offset = DEFAULT_FRAME_SPACING_OFFSET;

unsigned long lastActivityTime; // Definition for the extern declaration
bool sleepMode = false;

// Health/diagnostics
bool prefsSchemaValid = false;
bool prefsLoadedLegacy = false;
uint16_t prefsSchemaVersionLoaded = 0;
int last_lidar_error_code = 0;
int lidar_recovery_count = 0;
char lidar_sensor_fw_version[20] = "?";
bool adsReady = false;
bool mpuReady = false;
bool mainDisplayReady = false;
bool externalDisplayReady = false;
bool batteryGaugeReady = false;
bool lightMeterReady = false;
bool statusPixelReady = false;
bool encoderReady = false;
bool lidarSensorReady = false;
// ---------------------
