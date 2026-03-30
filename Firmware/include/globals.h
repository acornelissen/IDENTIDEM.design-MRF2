#ifndef GLOBALS_H
#define GLOBALS_H

#include <Preferences.h> // For Preferences object type
#include <stdint.h>
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

enum class UiMode : uint8_t
{
  Main,
  Config,
  ConfigFilm,
  ConfigLens,
  ConfigMeter,
  ConfigUi,
  Calib,
  ResetConfirm,
  Health,
  FactoryResetConfirm
};

// Preferences
extern Preferences prefs;


// Variables
// ---------------------
// Lightmeter
extern int prev_iso;
extern int iso;
extern float prev_aperture;
extern float aperture;
extern float lux;
extern float ev_readout;
extern char shutter_speed[16];
extern int iso_index;
extern int aperture_index;
extern int exposure_comp_thirds;
extern int meter_smoothing_mode;
extern bool show_ev_readout;
extern int sleep_timeout_mode;
extern int lidar_idle_timeout_mode;
extern int level_trim_landscape_deci_deg;
extern int level_trim_portrait_pos_deci_deg;
extern int level_trim_portrait_neg_deci_deg;

// Filter algorithm
extern int samples[SMOOTHING_WINDOW_SIZE];
extern int curReadIndex;
extern int sampleTotal;
extern int sampleAvg;

// Lens distance
extern int prev_lens_sensor_reading;
extern int lens_sensor_reading;
extern int lens_distance_raw;
extern char lens_distance_cm[16];

// LiDAR distance
extern int prev_distance;
extern int16_t distance;    // Distance to object in centimeters
extern char distance_cm[16];
extern int lidar_quality_level; // 0..4 (none, poor..excellent)
extern bool lidarEnabled;

// Battery gauge
extern int bat_per;

// Camera state
extern UiMode ui_mode;
extern int config_step;
extern int calib_step;
extern int selected_lens;
extern int selected_format;
extern int calib_lens;
extern bool parallaxEnabled;

extern int calib_distance_set[CALIB_DISTANCE_COUNT];
extern int current_calib_distance;
extern int calib_capture_status;
extern unsigned long calib_capture_status_ms;

extern int film_counter;
extern int prev_encoder_value;
extern int encoder_value;
extern float frame_progress;
extern float prev_frame_progress;
extern int frame_one_offset;
extern int frame_spacing_offset;

extern unsigned long lastActivityTime;
extern bool sleepMode;

// Health/diagnostics
extern bool prefsSchemaValid;
extern bool prefsLoadedLegacy;
extern uint16_t prefsSchemaVersionLoaded;
extern int last_lidar_error_code;
extern int lidar_recovery_count;
extern char lidar_sensor_fw_version[20];
extern bool adsReady;
extern bool mpuReady;
extern bool mainDisplayReady;
extern bool externalDisplayReady;
extern bool batteryGaugeReady;
extern bool lightMeterReady;
extern bool statusPixelReady;
extern bool encoderReady;
extern bool lidarSensorReady;
// ---------------------

#endif // GLOBALS_H
