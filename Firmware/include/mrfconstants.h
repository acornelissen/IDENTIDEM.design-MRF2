#ifndef MRFCONSTANTS_H
#define MRFCONSTANTS_H

// Constants
#define FWVERSION "9.0.0"
#define SLEEPTIMEOUT 60000

const unsigned long SERIAL_BAUD_RATE = 115200;
const unsigned long LIDAR_BAUD_RATE = 921600;
const int LIDAR_SERIAL_PORT = 2;

#define RXD2 RX
#define TXD2 TX

const int BUTTON_LEFT_PIN = 9;
const int BUTTON_RIGHT_PIN = 10;
const int BUTTON_DEBOUNCE_MS = 5;
const unsigned long BUTTON_SHORT_PRESS_MAX_MS = 1000;
const unsigned long BUTTON_LONG_PRESS_MIN_MS = 3000;

#define SS_SWITCH 24
#define SS_NEOPIX 6
#define SEESAW_ADDR 0x36 // Set this to the address of the seesaw

const unsigned long SEESAW_INIT_DELAY_MS = 10;
const int SEESAW_NEOPIXEL_BRIGHTNESS = 80;
const int NEOPIXEL_COUNT = 1;
const int NEOPIXEL_INDEX = 0;
const int NEOPIXEL_COLOR_MAX = 255;
const int NEOPIXEL_BLUE_R = 0;
const int NEOPIXEL_BLUE_G = 0;
const int NEOPIXEL_BLUE_B = 255;
const int NEOPIXEL_VIOLET_R = 238;
const int NEOPIXEL_VIOLET_G = 130;
const int NEOPIXEL_VIOLET_B = 238;
const int NEOPIXEL_OFF_R = 0;
const int NEOPIXEL_OFF_G = 0;
const int NEOPIXEL_OFF_B = 0;

#define LENS_ADC_PIN 1 // Set this to the pin you've soldered the lens position sensor to on the ADS1015

#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 128       // OLED display height, in pixels
#define SCREEN_HEIGHT_EXT 32    // OLED display height, in pixels
#define OLED_RESET -1           // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D     ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_ADDRESS_EXT 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

const unsigned long DISPLAY_INIT_DELAY_MS = 1000;
const unsigned long DISPLAY_EXT_INIT_DELAY_MS = 500;
const unsigned long DISPLAY_BOOT_SCREEN_MS = 1000;
const unsigned long LIDAR_SERIAL_STARTUP_DELAY_MS = 20;
const int DISPLAY_ROTATION = 3;
const int DISPLAY_BOOT_TEXT_SIZE = 2;
const int DISPLAY_BOOT_CURSOR_X = 20;
const int DISPLAY_BOOT_CURSOR_Y = 10;
const int DISPLAY_I2C_FREQUENCY_HZ = 1000000;
const int DISPLAY_COMMAND_FLIP = 0xC8;

const int SMOOTHING_WINDOW_SIZE = 13;
const int SENSOR_CHANNEL_COUNT = 2;
const int LENS_SENSOR_CHANNEL = 0;
const int LENS_INF_THRESHOLD = 5;
const int LENS_INFINITY_RAW = 9999999;
const int LENS_ACTIVITY_THRESHOLD = 3;
const int LENS_SNAP_DEADZONE = 1;
const int LENS_SNAP_DEADZONE_FAR = 3;
const float LENS_SNAP_FAR_DISTANCE_M = 3.0f;
const int LENS_ADC_SAMPLE_COUNT = 3;
const int LENS_ADC_SAMPLE_DELAY_US = 200;
const int LENS_ADC_QUIET_DELAY_MS = 1;
const int LENS_ADC_MAIN_OFFSET = 4;
const int FOCUS_RADIUS_MIN = 3;
const int FOCUS_RADIUS_MAX = 30;
const int FOCUS_RING_THICKNESS_MIN = 1;
const int FOCUS_RING_THICKNESS_MAX = 3;
const float FOCUS_RING_THICKNESS_SMOOTHING = 0.25f;

const float LENS_OUTLIER_THRESHOLD = 50.0;
const float CM_TO_MM = 10.0f;
const int CM_PER_METER = 100;

#define RETICLE_OFFSET_X -5
#define RETICLE_OFFSET_Y 0
#define LIDAR_OFFSET 40
const int LIDAR_DISTANCE_DIVISOR = 10;
const unsigned long LIDAR_NO_DATA_TIMEOUT_MS = 500;
const int FILM_COUNTER_SNAP_THRESHOLD = 1;
const int FILM_COUNTER_END = 99;

// LiDAR correction curve (cm). Below cutoff, apply a smooth correction based on a single reference point.
const float LIDAR_CAL_CUTOFF_CM = 150.0f;
const float LIDAR_CAL_REF_RAW_CM = 130.0f;
const float LIDAR_CAL_REF_TRUE_CM = 100.0f;

// Parallax correction (mm offsets between lens axis and viewfinder axis)
// +X = viewfinder is to the right of the lens; +Y = viewfinder is above the lens.
// Mamiya top finder is centered horizontally and sits above the lens; adjust magnitude as measured.
#define PARALLAX_OFFSET_X_MM 0.0f
#define PARALLAX_OFFSET_Y_MM 30.0f
#define PARALLAX_MIN_DISTANCE_MM 500.0f // clamp very close focus to avoid huge shifts
#define PARALLAX_MAX_SHIFT_PX 24        // guardrail on-screen shift

// Viewfinder overlay geometry (approximate) used to size framelines.

#define DISTANCE_MIN 5
#define DISTANCE_MAX 18

const int BATTERY_PERCENT_MAX = 100;
const int BATTERY_PERCENT_LOW_THRESHOLD = 10;
const int DISTANCE_DECIMAL_PLACES = 1;

const float LIGHTMETER_SPEED_ROUND_SCALE = 1000.0f;
const float SPEED_SECONDS_THRESHOLD = 1.0f;
const int SPEED_TEXT_BUFFER_LEN = 10;
const int SPEED_TEXT_WIDTH = 4;
const int SPEED_TEXT_DECIMALS_SHORT = 1;
const int SPEED_TEXT_DECIMALS_LONG = 2;

const int DEFAULT_ISO = 400;
const int DEFAULT_ISO_INDEX = 5;
const int DEFAULT_SELECTED_LENS = 1;
const int DEFAULT_SELECTED_FORMAT = 3;

const int CONFIG_STEP_MAX = 6;

const int APERTURE_DECIMAL_PLACES = 1;
const float LEVEL_PITCH_SCALE = 25.0f;
const float LEVEL_ROLL_SCALE = 0.5f;
const float LEVEL_DEADZONE = 0.03f;
const int LEVEL_LINE_MARGIN_PX = 10;
const int LEVEL_VERTICAL_LINE_LENGTH = 30;
const int MAIN_HEADER_HEIGHT = 15;
const int MAIN_RETICLE_CENTER_Y_OFFSET = 5;
const int MAIN_RETICLE_CENTER_RADIUS = 3;
const int MAIN_ISO_X = 2;
const int MAIN_ISO_Y = 7;
const int MAIN_APERTURE_X = 46;
const int MAIN_APERTURE_Y = 7;
const int MAIN_SHUTTER_X = 2;
const int MAIN_SHUTTER_Y = 14;
const int MAIN_DISTANCE_X = 68;
const int MAIN_DISTANCE_Y = 7;
const int MAIN_LENS_X = 68;
const int MAIN_LENS_Y = 14;
const int CONFIG_TITLE_X = 3;
const int CONFIG_TITLE_Y = 15;
const int CONFIG_ITEM_X = 3;
const int CONFIG_ITEM_Y_START = 26;
const int CONFIG_ITEM_Y_STEP = 11;
const int CONFIG_FOOTER_Y = 112;
const int CALIB_TITLE_X = 3;
const int CALIB_TITLE_Y = 15;
const int CALIB_ITEM_X = 3;
const int CALIB_LENS_Y = 35;
const int CALIB_DISTANCE_Y = 47;
const int CALIB_HELP_Y1 = 70;
const int CALIB_HELP_Y2 = 81;
const int EXT_HEADER_HEIGHT = 10;
const int EXT_HEADER_FORMAT_X = 2;
const int EXT_HEADER_FORMAT_Y = 8;
const int EXT_HEADER_DIVIDER_X = 33;
const int EXT_HEADER_LENS_X = 37;
const int EXT_HEADER_LENS_Y = 8;
const int EXT_BATTERY_DIVIDER_FULL_X = 100;
const int EXT_BATTERY_CURSOR_FULL_X = 104;
const int EXT_BATTERY_DIVIDER_LOW_X = 111;
const int EXT_BATTERY_CURSOR_LOW_X = 115;
const int EXT_BATTERY_DIVIDER_MID_X = 103;
const int EXT_BATTERY_CURSOR_MID_X = 107;
const int EXT_PROGRESS_BAR_WIDTH = 90;
const int EXT_PROGRESS_BAR_HEIGHT = 17;
const int EXT_PROGRESS_BAR_X = 34;
const int EXT_PROGRESS_BAR_Y = 15;
const float PERCENT_SCALE = 100.0f;
const int EXT_COUNTER_TEXT_Y = 30;
const int EXT_COUNTER_MESSAGE_X = 8;
const int EXT_COUNTER_VALUE_X_WITH_PROGRESS = 8;
const int EXT_COUNTER_VALUE_X_NO_PROGRESS = 60;
const int EXT_SLEEP_TEXT_X = 8;
const int EXT_SLEEP_TEXT_Y = 22;

const int ISOS[] = {50, 80, 100, 125, 200, 400, 500, 640, 800, 1600, 3200, 6400};
const float CALIB_DISTANCES[] = {1, 1.2, 1.5, 2, 3, 5, 10};
const int CALIB_DISTANCE_COUNT = sizeof(CALIB_DISTANCES) / sizeof(CALIB_DISTANCES[0]);
const int CALIB_SAMPLE_COUNT = 8;
const int CALIB_SAMPLE_DELAY_MS = 5;

const int K = 20;

#endif // MRFCONSTANTS_H
