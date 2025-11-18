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
extern int samples[2][SMOOTHING_WINDOW_SIZE]; // the readings from the analog input
extern int curReadIndex[2];          // the index of the current reading
extern int sampleTotal[2];           // the running total
extern int sampleAvg[2];

// Lens distance
extern int prev_lens_sensor_reading;
extern int lens_sensor_reading;
extern int lens_distance_raw;
extern String lens_distance_cm;

// LiDAR distance
extern int prev_distance;
extern int16_t distance;    // Distance to object in centimeters
extern int16_t strength;    // Strength or quality of return signal
extern int16_t temperature; // Internal temperature of Lidar sensor chip
extern String distance_cm;

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

extern int calib_distance_set[7];
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