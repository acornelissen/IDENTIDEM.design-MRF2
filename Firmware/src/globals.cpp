#include "globals.h"
#include <Arduino.h>     // For String, millis()
#include <Preferences.h> // For Preferences object type
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE

// Preferences
Preferences prefs;

// Variables
// ---------------------
// Lightmeter
int prev_iso = 400;
int iso = 400;
float prev_aperture;
float aperture;
float prev_lux = 0;
float lux = 0;
String shutter_speed = "...";
int iso_index = 5;
int aperture_index;

// Filter algorithm
int samples[2][SMOOTHING_WINDOW_SIZE]; // the readings from the analog input
int curReadIndex[2] = {0, 0};          // the index of the current reading
int sampleTotal[2] = {0, 0};           // the running total
int sampleAvg[2] = {0, 0};

// Lens distance
int prev_lens_sensor_reading = 0;
int lens_sensor_reading = 0;
int lens_distance_raw = 0;
String lens_distance_cm = "...";

// LiDAR distance
int prev_distance = 0;
int16_t distance = 0;    // Distance to object in centimeters
int16_t strength = 0;    // Strength or quality of return signal
int16_t temperature = 0; // Internal temperature of Lidar sensor chip
String distance_cm = "...";

// Battery gauge
int prev_bat_per = 0;
int bat_per = 0;

// Camera state
String ui_mode = "main";
int config_step = 0;
int calib_step = 0;
int selected_lens = 1;
int selected_format = 3;
int calib_lens = 0;

int calib_distance_set[7] = {};
int current_calib_distance = 0;

int film_counter = 0;
int prev_encoder_value = 0;
int encoder_value = 0;
float frame_progress = 0;
float prev_frame_progress = 0;

unsigned long lastActivityTime; // Definition for the extern declaration
bool sleepMode = false;
// ---------------------