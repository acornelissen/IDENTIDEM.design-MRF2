#ifndef GLOBALS_H
#define GLOBALS_H

#include <Preferences.h> // For Preferences object type
#include <Arduino.h>     // For String type and int16_t
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

// Preferences
extern Preferences prefs;


// Variables
// ---------------------
// Lightmeter
extern int prev_iso;
extern int iso;
extern float prev_aperture;
extern float aperture;
extern float prev_lux;
extern float lux;
extern String shutter_speed;
extern int iso_index;
extern int aperture_index;

// Filter algorithm
extern int samples[SMOOTHING_WINDOW_SIZE];
extern int curReadIndex;
extern int sampleTotal;
extern int sampleAvg;

// Lens distance
extern int prev_lens_sensor_reading;
extern int lens_sensor_reading;
extern int lens_distance_raw;
extern String lens_distance_cm;

// LiDAR distance
extern int prev_distance;
extern int16_t distance;    // Distance to object in centimeters
extern String distance_cm;
extern int lidar_quality_level; // 0..4 (none, poor..excellent)
extern bool lidarEnabled;

// Battery gauge
extern int prev_bat_per;
extern int bat_per;

// Camera state
extern String ui_mode;
extern int config_step;
extern int calib_step;
extern int selected_lens;
extern int selected_format;
extern int calib_lens;
extern bool parallaxEnabled;

extern int calib_distance_set[CALIB_DISTANCE_COUNT];
extern int current_calib_distance;

extern int film_counter;
extern int prev_encoder_value;
extern int encoder_value;
extern float frame_progress;
extern float prev_frame_progress;

extern unsigned long lastActivityTime;
extern bool sleepMode;
// ---------------------

#endif // GLOBALS_H
