#ifndef MRFCONSTANTS_H
#define MRFCONSTANTS_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Firmware identity and boot behavior
// ---------------------------------------------------------------------------
#define FWVERSION "10.3.4"                  // Version shown in UI and release metadata.
const unsigned long SLEEP_BOOT_GRACE_MS = 15000; // Ignore sleep timer immediately after boot.

// ---------------------------------------------------------------------------
// Serial ports and sensor transport
// ---------------------------------------------------------------------------
const unsigned long SERIAL_BAUD_RATE = 115200; // USB serial console baud.
const unsigned long LIDAR_BAUD_RATE = 921600;  // DTS6012M UART baud.
const int LIDAR_SERIAL_PORT = 2;               // ESP32 hardware serial index used for LiDAR.

#define RXD2 RX // LiDAR RX mapped to board RX pin.
#define TXD2 TX // LiDAR TX mapped to board TX pin.

// ---------------------------------------------------------------------------
// User input buttons
// ---------------------------------------------------------------------------
const int BUTTON_LEFT_PIN = 9;                 // Left button GPIO.
const int BUTTON_RIGHT_PIN = 10;               // Right button GPIO.
const int BUTTON_DEBOUNCE_MS = 5;              // Software debounce interval.
const unsigned long BUTTON_SHORT_PRESS_MAX_MS = 1000; // Max press duration treated as "short".
const unsigned long BUTTON_LONG_PRESS_MIN_MS = 3000;  // Min press duration treated as "long".

// ---------------------------------------------------------------------------
// Seesaw and NeoPixel status LED
// ---------------------------------------------------------------------------
#define SS_NEOPIX 6      // Seesaw pin used by onboard NeoPixel.
#define SEESAW_ADDR 0x36 // I2C address for encoder/NeoPixel seesaw.

const unsigned long SEESAW_INIT_DELAY_MS = 10; // Delay after seesaw init before first access.
const int SEESAW_NEOPIXEL_BRIGHTNESS = 80;     // NeoPixel brightness cap (0..255).
const int NEOPIXEL_COUNT = 1;                  // Number of status pixels.
const int NEOPIXEL_INDEX = 0;                  // Active status pixel index.
const int NEOPIXEL_COLOR_MAX = 255;            // Max RGB channel value.
const int NEOPIXEL_BLUE_R = 0;                 // Blue status color R channel.
const int NEOPIXEL_BLUE_G = 0;                 // Blue status color G channel.
const int NEOPIXEL_BLUE_B = 255;               // Blue status color B channel.
const int NEOPIXEL_VIOLET_R = 238;             // Violet status color R channel.
const int NEOPIXEL_VIOLET_G = 130;             // Violet status color G channel.
const int NEOPIXEL_VIOLET_B = 238;             // Violet status color B channel.
const int NEOPIXEL_OFF_R = 0;                  // LED-off R channel.
const int NEOPIXEL_OFF_G = 0;                  // LED-off G channel.
const int NEOPIXEL_OFF_B = 0;                  // LED-off B channel.

// ---------------------------------------------------------------------------
// Analog lens position input
// ---------------------------------------------------------------------------
#define LENS_ADC_PIN 1 // ADS1015 channel for lens position sensor.

// ---------------------------------------------------------------------------
// Display and I2C bus setup
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH 128        // Main OLED width in pixels.
#define SCREEN_HEIGHT 128       // Main OLED height in pixels.
#define SCREEN_HEIGHT_EXT 32    // External OLED height in pixels.
#define OLED_RESET -1           // Shared reset pin (unused, set to -1).
#define SCREEN_ADDRESS 0x3D     // Main OLED I2C address.
#define SCREEN_ADDRESS_EXT 0x3C // External OLED I2C address.

const unsigned long DISPLAY_INIT_DELAY_MS = 1000;      // Main display power-up settle delay.
const unsigned long DISPLAY_EXT_INIT_DELAY_MS = 500;   // External display settle delay.
const unsigned long DISPLAY_BOOT_SCREEN_MS = 1000;     // Boot splash hold time.
const unsigned long LIDAR_SERIAL_STARTUP_DELAY_MS = 20; // LiDAR serial warm-up delay.
const uint8_t LIGHTMETER_I2C_ADDR = 0x23;              // BH1750 I2C address.
const uint8_t LIGHTMETER_CMD_POWER_DOWN = 0x00;        // BH1750 power-down command.
const uint8_t LIGHTMETER_CMD_POWER_ON = 0x01;          // BH1750 power-on command.
const int DISPLAY_ROTATION = 3;                        // Main display rotation setting.
const int DISPLAY_BOOT_TEXT_SIZE = 2;                  // Boot text scale on external display.
const int DISPLAY_I2C_FREQUENCY_HZ = 1000000;          // Fast I2C bus frequency for display writes.
const int SHARED_I2C_FREQUENCY_HZ  = 400000;           // Standard I2C speed restored after display writes.
const int DISPLAY_COMMAND_FLIP = 0xC8;                 // SH1107 vertical flip command.
const uint8_t OLED_CMD_DISPLAY_OFF = 0xAE;             // SH1107 command to blank the display.
const uint8_t OLED_CMD_DISPLAY_ON  = 0xAF;             // SH1107 command to enable the display.
const unsigned long FADE_STEP_INTERVAL_MS = 25;        // Delay between non-blocking fade steps.
const int FADE_STEP_DECREMENT = 0x20;                  // Contrast decrement per fade step.

// ---------------------------------------------------------------------------
// Lens ADC filtering, snap logic, and focus-ring rendering
// ---------------------------------------------------------------------------
const int SMOOTHING_WINDOW_SIZE = 13;           // Moving-average window for lens ADC smoothing.
const int LENS_INF_THRESHOLD = 5;               // Threshold around infinity marker (sensor units).
const int LENS_INFINITY_RAW = 9999999;          // Sentinel raw distance for infinity.
const int LENS_ACTIVITY_THRESHOLD = 3;          // Minimum ADC delta considered lens movement.
const int LENS_SNAP_DEADZONE = 1;               // Snap deadzone near calibrated points (close range).
const int LENS_SNAP_DEADZONE_FAR = 3;           // Snap deadzone for far focus points.
const float LENS_SNAP_FAR_DISTANCE_M = 3.0f;    // Distance threshold to switch to far deadzone.
const int LENS_ADC_SAMPLE_COUNT = 3;            // ADC samples averaged per read.
const int LENS_ADC_SAMPLE_DELAY_US = 200;       // Delay between ADC sub-samples.
const int LENS_ADC_QUIET_DELAY_MS = 1;          // Quiet time before ADC sampling.
const int LENS_ADC_MAIN_OFFSET = 4;             // UI-mode compensation offset for lens ADC.
const int LENS_SPIKE_DELTA_THRESHOLD = 8;       // Delta treated as potential ADC spike.
const int LENS_SPIKE_CONFIRMATION_COUNT = 2;    // Consecutive spike readings required for accept.
const int FOCUS_RADIUS_MIN = 3;                 // Minimum focus-ring radius in pixels.
const int FOCUS_RADIUS_MAX = 40;                // Maximum focus-ring radius in pixels.
const int FOCUS_RING_THICKNESS_MIN = 1;         // Minimum focus-ring thickness.
const int FOCUS_RING_THICKNESS_MAX = 5;         // Maximum focus-ring thickness.
const float FOCUS_RING_RADIUS_SMOOTHING = 0.15f;    // EMA factor for radius (lower = smoother).
const float FOCUS_RING_THICKNESS_SMOOTHING = 0.25f; // EMA factor for ring thickness.

// ---------------------------------------------------------------------------
// Unit conversion helpers
// ---------------------------------------------------------------------------
const float CM_TO_MM = 10.0f; // Centimeter-to-millimeter conversion factor.
const int CM_PER_METER = 100; // Centimeters per meter.

// ---------------------------------------------------------------------------
// LiDAR fusion and confidence pipeline tuning
// ---------------------------------------------------------------------------
#define RETICLE_OFFSET_X -5 // Main reticle X offset for optical alignment.
#define RETICLE_OFFSET_Y 0  // Main reticle Y offset for optical alignment.
const int LIDAR_DISTANCE_DIVISOR = 10;           // Raw LiDAR millimetre-to-centimetre divisor.
const uint16_t LIDAR_FRAME_RATE_FPS = 50;        // Sensor frame rate — lower = more integration time per frame.
const unsigned long LIDAR_NO_DATA_TIMEOUT_MS = 1000; // Hold last reading before showing placeholder.
const int LIDAR_FAR_SIGNAL_LOSS_CM = 300;         // Show "Inf." instead of "..." when signal lost above this distance.
const int LIDAR_RECOVERY_ERROR_THRESHOLD = 3;    // Errors before recovery path escalates.
const unsigned long LIDAR_RECOVERY_TIMEOUT_MS = 1500; // Timeout window triggering recovery.
const unsigned long LIDAR_RECOVERY_RETRY_BASE_MS = 250; // Initial retry backoff.
const unsigned long LIDAR_RECOVERY_RETRY_MAX_MS = 2000; // Max retry backoff.
const float LIDAR_LIBRARY_DISTANCE_SCALE = 1.0f; // Library-side linear distance scale.
const int LIDAR_LIBRARY_DISTANCE_OFFSET_MM = 400; // Library-side linear distance offset.
const int LIDAR_LIBRARY_MIN_INTENSITY_THRESHOLD = 20; // Library quality tier base; tiers are distance-scaled in v2.1.2+.
const int LIDAR_FUSION_MIN_INTENSITY = 40;       // Near-range (≤2m) minimum intensity — strict for accuracy.
const int LIDAR_FUSION_INTENSITY_NEAR_RANGE_CM = 200; // Near-range boundary (≤2m): super accurate.
const int LIDAR_FUSION_INTENSITY_MID_RANGE_CM = 500;  // Mid-range boundary (≤5m): less accurate.
const int LIDAR_FUSION_INTENSITY_FAR_RANGE_CM = 700;  // Far-range boundary (≤7m): even less accurate.
const int LIDAR_FUSION_MIN_INTENSITY_MID = 10;   // Mid-range minimum intensity (2–5m).
const int LIDAR_FUSION_MIN_INTENSITY_FAR = 3;    // Far-range minimum intensity (5–7m).
const int LIDAR_FUSION_MIN_INTENSITY_MAX_RANGE = 1; // Beyond-far minimum intensity (8m+): just get a value.
const int LIDAR_SNR_PERMILLE_TARGET_NEAR = 300;  // Target SNR (permille) for near returns (≤2m) — strict.
const int LIDAR_SNR_PERMILLE_TARGET_MID = 150;   // Target SNR (permille) for mid returns (2–5m) — moderate.
const int LIDAR_SNR_PERMILLE_TARGET_FAR = 40;    // Target SNR (permille) for far returns (5–7m) — relaxed.
const int LIDAR_SNR_PERMILLE_TARGET_MAX_RANGE = 10; // Target SNR (permille) at max range (8m+) — accept anything.
const int LIDAR_SNR_PERMILLE_HARD_REJECT = 8;    // SNR hard-reject floor (global).
const int LIDAR_SNR_HARD_REJECT_INTENSITY_MULTIPLIER = 2; // Extra rejection guard in low intensity.
const int LIDAR_SNR_PENALTY_DIVISOR = 30;        // Maps SNR deficit to confidence penalty (larger = gentler).
const int LIDAR_SNR_PENALTY_MAX = 8;             // Max SNR-based confidence penalty.
const int LIDAR_SNR_FALLBACK_PENALTY_MAX = 3;    // Max SNR penalty in fallback path.
const int LIDAR_FALLBACK_MIN_INTENSITY = 1;      // Minimum intensity accepted in fallback.
const int LIDAR_FALLBACK_BASE_CONFIDENCE = 25;   // Base confidence for fallback candidate.
const int LIDAR_FALLBACK_MAX_CONFIDENCE = 50;    // Ceiling confidence for fallback candidate.
const int LIDAR_CONFIDENCE_HIGH = 75;            // High-confidence threshold.
const int LIDAR_CONFIDENCE_MEDIUM = 55;          // Medium-confidence threshold.
const float LIDAR_MEDIUM_CONF_BLEND = 0.35f;     // Blend factor when confidence is medium.
const float LIDAR_LOW_CONF_BLEND_MIN = 0.20f;    // Minimum blend at low confidence.
const float LIDAR_LOW_CONF_BLEND_MAX = 0.35f;    // Maximum blend at low confidence.
const int LIDAR_LOW_CONF_MAX_STEP_CM = 300;      // Max accepted per-frame distance step at low confidence.
const int LIDAR_FUSION_AGREE_DELTA_CM = 30;      // Agreement delta between primary/secondary returns.
const int LIDAR_FUSION_CONF_BONUS = 6;           // Confidence bonus when candidates agree.
const int LIDAR_TEMPORAL_PENALTY_DIVISOR = 25;   // Maps temporal jump size to confidence penalty (larger = gentler).
const int LIDAR_TEMPORAL_PENALTY_MAX = 10;       // Max temporal confidence penalty.
const int LIDAR_PRIOR_PENALTY_MAX_POOR = 3;      // Prior penalty cap for poor candidate quality.
const int LIDAR_PRIOR_PENALTY_MAX_FAIR = 4;      // Prior penalty cap for fair candidate quality.
const int LIDAR_PRIOR_PENALTY_MAX_GOOD = 5;      // Prior penalty cap for good candidate quality.
const int LIDAR_PRIOR_PENALTY_MAX_EXCELLENT = 4; // Prior penalty cap for excellent candidate quality.
const int LIDAR_PRIOR_DEADBAND_CM = 25;          // No prior penalty inside this distance error band.
const int LIDAR_PRIOR_RANGE_NEAR_CM = 200;       // Near band for prior scaling (≤2m).
const int LIDAR_PRIOR_RANGE_MID_CM = 500;        // Mid band for prior scaling (≤5m).
const int LIDAR_PRIOR_RANGE_FAR_CM = 700;        // Far band for prior scaling (≤7m).
const float LIDAR_PRIOR_RANGE_SCALE_NEAR = 1.0f; // Prior penalty scale at near range (≤2m) — full influence.
const float LIDAR_PRIOR_RANGE_SCALE_MID = 0.6f;  // Prior penalty scale at mid range (2–5m).
const float LIDAR_PRIOR_RANGE_SCALE_FAR = 0.35f; // Prior penalty scale at far range (5–7m).
const float LIDAR_PRIOR_RANGE_SCALE_VERY_FAR = 0.2f; // Prior penalty scale at max range (8m+) — minimal influence.
const float LIDAR_LENS_PRIOR_WEIGHT_GOOD = 0.025f;    // Lens-prior pull when confidence is good.
const float LIDAR_LENS_PRIOR_WEIGHT_EXCELLENT = 0.012f; // Lens-prior pull when confidence is excellent.

const int LIDAR_RESIDUAL_POINT_COUNT = 6; // Number of residual correction table points.
const int LIDAR_RESIDUAL_DIST_CM[LIDAR_RESIDUAL_POINT_COUNT] = {50, 100, 150, 200, 500, 1000}; // Residual table X-axis.
const int LIDAR_RESIDUAL_DELTA_CM[LIDAR_RESIDUAL_POINT_COUNT] = {0, 0, 0, 0, 0, 0};             // Residual table Y-axis.

// ---------------------------------------------------------------------------
// Film counter/encoder filtering
// ---------------------------------------------------------------------------
const int FILM_COUNTER_SNAP_THRESHOLD = 1;       // Snap-to-frame threshold near known points.
const int FILM_COUNTER_END = 99;                 // Sentinel frame value for roll end.
const int FILM_COUNTER_FORWARD_HYSTERESIS = 2;   // Forward movement hysteresis in encoder ticks.
const unsigned long FILM_COUNTER_FORWARD_DEBOUNCE_MS = 35; // Forward movement debounce.
const int FILM_COUNTER_ACTIVITY_MIN_DELTA = 1;   // Raw encoder delta considered activity.
const bool FILM_COUNTER_ALLOW_REWIND = false;    // Allow backward counting when rewinding.
const int FILM_COUNTER_REWIND_HYSTERESIS = 4;    // Rewind hysteresis if rewind mode is enabled.
const unsigned long FILM_COUNTER_REWIND_DEBOUNCE_MS = 120; // Rewind debounce.

// LiDAR correction curve (cm). Below cutoff, apply a smooth correction based on one reference point.
const float LIDAR_CAL_CUTOFF_CM = 150.0f; // Upper bound where near-range correction is applied.
const float LIDAR_CAL_REF_RAW_CM = 130.0f; // Raw reference distance used for correction.
const float LIDAR_CAL_REF_TRUE_CM = 100.0f; // True distance for the correction reference.

// ---------------------------------------------------------------------------
// Frameline and parallax correction model
// ---------------------------------------------------------------------------
// +X = viewfinder is to the right of the lens; +Y = viewfinder is above the lens.
// Mamiya top finder is centered horizontally and sits above the lens.
#define PARALLAX_OFFSET_X_MM 0.0f   // Horizontal optical offset between lens and finder.
#define PARALLAX_OFFSET_Y_MM 30.0f  // Vertical optical offset between lens and finder.
#define PARALLAX_MIN_DISTANCE_MM 500.0f // Clamp close focus distance for stable shifts.
#define PARALLAX_MAX_SHIFT_PX 24        // Maximum frameline shift guardrail.

// Viewfinder overlay geometry limits used for frameline scaling.
#define DISTANCE_MIN 5  // Minimum optical distance used by frameline mapping.
#define DISTANCE_MAX 18 // Maximum optical distance used by frameline mapping.
const int LIDAR_DISPLAY_MIN_CM = 15;            // Display "<15cm" below this threshold.
const int LIDAR_DISPLAY_INF_THRESHOLD_CM = 1050; // Display "Inf." above this threshold.

// ---------------------------------------------------------------------------
// Battery and readout formatting
// ---------------------------------------------------------------------------
const int BATTERY_PERCENT_MAX = 100;          // Clamp battery percentage upper bound.
const int BATTERY_PERCENT_LOW_THRESHOLD = 10; // Low-battery threshold used by external UI layout.
const int DISTANCE_DECIMAL_PLACES = 1;        // Decimal precision for meter display text.

// ---------------------------------------------------------------------------
// Light meter and shutter text formatting
// ---------------------------------------------------------------------------
const float LIGHTMETER_SPEED_ROUND_SCALE = 1000.0f; // Round shutter speed to 1/1000s precision.
const float SPEED_SECONDS_THRESHOLD = 1.0f;         // Threshold between fractional and second display styles.
const float LIGHTMETER_MAX_SPEED_SECONDS = 1500.0f; // Cap shutter display at 25 minutes (25 * 60).
const int SPEED_TEXT_BUFFER_LEN = 10;               // Buffer size for shutter-speed text (sub-1s fallthrough path).
const int SPEED_TEXT_WIDTH = 4;                     // Max digit width for integer speed formatting.
const int SPEED_TEXT_DECIMALS_SHORT = 1;            // Decimals for shorter second values.
const int LIGHTMETER_EV_COMP_MIN_THIRDS = -9;       // EV compensation lower bound (-3 EV).
const int LIGHTMETER_EV_COMP_MAX_THIRDS = 9;        // EV compensation upper bound (+3 EV).
const int LIGHTMETER_SMOOTHING_MODE_MIN = 0;        // Smoothing enum minimum.
const int LIGHTMETER_SMOOTHING_MODE_MAX = 3;        // Smoothing enum maximum.
const int LIGHTMETER_SMOOTHING_MODE_COUNT = 4;      // Number of smoothing modes.

// ---------------------------------------------------------------------------
// Sleep timeout mode enum values
// ---------------------------------------------------------------------------
const int SLEEP_TIMEOUT_MODE_MIN = 0;   // Smallest valid sleep timeout mode.
const int SLEEP_TIMEOUT_MODE_MAX = 5;   // Largest valid sleep timeout mode.
const int SLEEP_TIMEOUT_MODE_COUNT = 6; // Number of sleep timeout options.
const int SLEEP_TIMEOUT_MODE_OFF = 0;   // Disable auto-sleep.
const int SLEEP_TIMEOUT_MODE_15S = 1;   // 15-second auto-sleep.
const int SLEEP_TIMEOUT_MODE_30S = 2;   // 30-second auto-sleep.
const int SLEEP_TIMEOUT_MODE_1M = 3;    // 1-minute auto-sleep.
const int SLEEP_TIMEOUT_MODE_1M30S = 4; // 90-second auto-sleep.
const int SLEEP_TIMEOUT_MODE_2M = 5;    // 2-minute auto-sleep.

// ---------------------------------------------------------------------------
// Persisted defaults and preferences schema
// ---------------------------------------------------------------------------
const int DEFAULT_ISO = 400;                        // Default ISO on first boot.
const int DEFAULT_ISO_INDEX = 5;                    // Default index into ISOS[].
const int DEFAULT_SELECTED_LENS = 1;                // Default lens profile index.
const int DEFAULT_SELECTED_FORMAT = 4;              // Default film format index.
const int DEFAULT_EXPOSURE_COMP_THIRDS = 0;         // Default EV compensation (third-stops).
const int DEFAULT_METER_SMOOTHING_MODE = 2;         // Default light meter smoothing mode.
const bool DEFAULT_SHOW_EV_READOUT = false;         // EV readout hidden by default.
const int DEFAULT_SLEEP_TIMEOUT_MODE = SLEEP_TIMEOUT_MODE_1M; // Default auto-sleep mode.
const int DEFAULT_LIDAR_IDLE_TIMEOUT_MODE = SLEEP_TIMEOUT_MODE_1M; // Default awake-idle LiDAR standby timeout.
const int DEFAULT_FRAME_ONE_OFFSET = 0;             // Default film frame-1 tuning offset.
const int DEFAULT_FRAME_SPACING_OFFSET = 0;         // Default film frame-spacing tuning offset.
const int DEFAULT_LEVEL_TRIM_LANDSCAPE_DECI_DEG = 0;    // Default landscape level trim in 0.1-degree units.
const int DEFAULT_LEVEL_TRIM_PORTRAIT_POS_DECI_DEG = 0; // Default portrait (+) level trim in 0.1-degree units.
const int DEFAULT_LEVEL_TRIM_PORTRAIT_NEG_DECI_DEG = 0; // Default portrait (-) level trim in 0.1-degree units.
const uint16_t PREFS_SCHEMA_VERSION = 2;            // Preferences schema version for migration.

// ---------------------------------------------------------------------------
// Film frame tuning ranges
// ---------------------------------------------------------------------------
const int FRAME_TUNING_MIN = -10; // Minimum frame tuning value.
const int FRAME_TUNING_MAX = 10;  // Maximum frame tuning value.

// ---------------------------------------------------------------------------
// Main loop scheduler cadence and wake thresholds
// ---------------------------------------------------------------------------
const unsigned long LOOP_INPUT_INTERVAL_MS = 5;           // Input polling cadence while awake.
const unsigned long LOOP_FILM_COUNTER_INTERVAL_MS = 5;    // Film counter update cadence.
const unsigned long LOOP_FILM_COUNTER_IDLE_INTERVAL_MS = 75; // Film counter cadence when advance lever is stable.
const unsigned long LOOP_FILM_COUNTER_ACTIVE_HOLD_MS = 500;  // Keep fast film polling after recent movement.
const unsigned long LOOP_SLEEP_CHECK_INTERVAL_MS = 50;    // Sleep-state check cadence.
const uint64_t     LOOP_SLEEP_LIGHT_SLEEP_US = 100000ULL; // Light sleep duration between sensor polls (µs).
const unsigned long LOOP_LIDAR_INTERVAL_MS = 25;          // LiDAR update cadence.
const unsigned long LOOP_LENS_INTERVAL_MS = 25;           // Lens ADC + mapping cadence.
const unsigned long LOOP_LENS_IDLE_INTERVAL_MS = 100;     // Lens ADC cadence when focus ring is stable.
const unsigned long LOOP_LENS_ACTIVE_HOLD_MS = 750;       // Keep fast lens polling after recent focus movement.
const unsigned long LOOP_LIGHTMETER_INTERVAL_MS = 100;    // Light meter update cadence.
const unsigned long LOOP_LIGHTMETER_IDLE_INTERVAL_MS = 500; // Light meter cadence when scene/settings are stable.
const unsigned long LOOP_LIGHTMETER_ACTIVE_HOLD_MS = 1500;  // Keep fast light-meter polling after recent changes.
const unsigned long LOOP_BATTERY_INTERVAL_MS = 5000;      // Battery gauge update cadence.
const unsigned long LOOP_UI_INTERVAL_MS = 50;             // UI redraw cadence (~20 FPS).
const unsigned long LOOP_UI_MAIN_REFRESH_MS = 100;        // Forced redraw cadence in Main mode when state is stable.
const unsigned long LOOP_UI_HEALTH_REFRESH_MS = 1000;     // Forced redraw cadence for Health idle timer updates.
const unsigned long LOOP_PREFS_FLUSH_INTERVAL_MS = 200;   // Preferences flush check cadence.
const int CPU_FREQ_ACTIVE_MHZ = 240;                      // CPU frequency while device is awake.
const int CPU_FREQ_SLEEP_MHZ  = 80;                       // CPU frequency while device is sleeping.
const float LIGHTMETER_ACTIVITY_DELTA_LUX = 1.0f;         // Lux delta that keeps light-meter polling in fast mode.
const int SLEEP_WAKE_ENCODER_DELTA = 1;                   // Encoder delta to wake device.
const int SLEEP_WAKE_LENS_DELTA = 8;                      // Lens ADC delta to wake device.

// ---------------------------------------------------------------------------
// Config menu indexes: setup root
// ---------------------------------------------------------------------------
// Config menu indexes are plain enums so they remain int-compatible with
// config_step.  Each menu declares a COUNT sentinel so that _MAX is
// derived automatically — adding a new item without updating the list
// triggers a compile error from the static_assert below.
enum ConfigRootStep
{
  CONFIG_ROOT_STEP_FILM_MENU = 0,  // Enter Film submenu.
  CONFIG_ROOT_STEP_LENS_MENU,      // Enter Lens submenu.
  CONFIG_ROOT_STEP_METER_MENU,     // Enter Light Meter submenu.
  CONFIG_ROOT_STEP_UI_MENU,        // Enter UI Settings submenu.
  CONFIG_ROOT_STEP_RESET,          // Enter reset-frame confirmation.
  CONFIG_ROOT_STEP_HEALTH,         // Enter health diagnostics screen.
  CONFIG_ROOT_STEP_EXIT,           // Exit setup to main UI.
  CONFIG_ROOT_STEP_COUNT
};
const int CONFIG_ROOT_STEP_MAX = CONFIG_ROOT_STEP_EXIT;
static_assert(CONFIG_ROOT_STEP_COUNT - 1 == CONFIG_ROOT_STEP_MAX,
              "CONFIG_ROOT_STEP_MAX does not match enum count");

// Config menu indexes: Film submenu.
enum ConfigFilmStep
{
  CONFIG_FILM_STEP_FORMAT = 0,          // Film format selector.
  CONFIG_FILM_STEP_CURRENT_FRAME,       // Current frame selector.
  CONFIG_FILM_STEP_FRAME_ONE_OFFSET,    // Frame-1 offset tuning.
  CONFIG_FILM_STEP_FRAME_SPACING,       // Frame spacing tuning.
  CONFIG_FILM_STEP_BACK,                // Back to setup root.
  CONFIG_FILM_STEP_COUNT
};
const int CONFIG_FILM_STEP_MAX = CONFIG_FILM_STEP_BACK;
static_assert(CONFIG_FILM_STEP_COUNT - 1 == CONFIG_FILM_STEP_MAX,
              "CONFIG_FILM_STEP_MAX does not match enum count");

// Config menu indexes: Lens submenu.
enum ConfigLensStep
{
  CONFIG_LENS_STEP_LENS = 0,      // Lens selector.
  CONFIG_LENS_STEP_PARALLAX,      // Parallax toggle.
  CONFIG_LENS_STEP_CALIB,         // Enter lens calibration.
  CONFIG_LENS_STEP_BACK,          // Back to setup root.
  CONFIG_LENS_STEP_COUNT
};
const int CONFIG_LENS_STEP_MAX = CONFIG_LENS_STEP_BACK;
static_assert(CONFIG_LENS_STEP_COUNT - 1 == CONFIG_LENS_STEP_MAX,
              "CONFIG_LENS_STEP_MAX does not match enum count");

// Config menu indexes: Meter submenu.
enum ConfigMeterStep
{
  CONFIG_METER_STEP_ISO = 0,       // ISO selector.
  CONFIG_METER_STEP_EV_COMP,       // EV compensation selector.
  CONFIG_METER_STEP_SMOOTHING,     // Smoothing selector.
  CONFIG_METER_STEP_EV_READOUT,    // EV readout toggle.
  CONFIG_METER_STEP_BACK,          // Back to setup root.
  CONFIG_METER_STEP_COUNT
};
const int CONFIG_METER_STEP_MAX = CONFIG_METER_STEP_BACK;
static_assert(CONFIG_METER_STEP_COUNT - 1 == CONFIG_METER_STEP_MAX,
              "CONFIG_METER_STEP_MAX does not match enum count");

// Config menu indexes: UI Settings submenu.
enum ConfigUiStep
{
  CONFIG_UI_STEP_HORIZON_LANDSCAPE = 0,  // Landscape horizon trim.
  CONFIG_UI_STEP_HORIZON_PORTRAIT_POS,   // Portrait + horizon trim.
  CONFIG_UI_STEP_HORIZON_PORTRAIT_NEG,   // Portrait - horizon trim.
  CONFIG_UI_STEP_SLEEP_TIMEOUT,          // Sleep timeout selector.
  CONFIG_UI_STEP_LIDAR_IDLE_TIMEOUT,     // LiDAR idle-timeout selector.
  CONFIG_UI_STEP_BACK,                   // Back to setup root.
  CONFIG_UI_STEP_COUNT
};
const int CONFIG_UI_STEP_MAX = CONFIG_UI_STEP_BACK;
static_assert(CONFIG_UI_STEP_COUNT - 1 == CONFIG_UI_STEP_MAX,
              "CONFIG_UI_STEP_MAX does not match enum count");

// ---------------------------------------------------------------------------
// Main UI numeric formatting and level-aid tuning
// ---------------------------------------------------------------------------
const int APERTURE_DECIMAL_PLACES = 1;      // Decimal precision for aperture display.
const float LEVEL_PITCH_SCALE = 25.0f;      // Landscape pitch sensitivity scaling.
const float LEVEL_ROLL_SCALE = 0.5f;        // Landscape roll sensitivity scaling.
const float LEVEL_DEADZONE = 0.03f;         // Level deadzone around zero tilt (radians).
const float LEVEL_PITCH_SCALE_PORTRAIT = 12.0f; // Portrait pitch sensitivity scaling.
const float LEVEL_ROLL_SCALE_PORTRAIT = 1.0f;   // Portrait roll sensitivity scaling.
const float LEVEL_PORTRAIT_ROLL_DEVIATION_SIGN = 1.0f; // Portrait deviation polarity.
const int LEVEL_TRIM_MIN_DECI_DEG = -300;   // Minimum user-adjustable horizon trim (-30.0deg).
const int LEVEL_TRIM_MAX_DECI_DEG = 300;    // Maximum user-adjustable horizon trim (+30.0deg).
const int LEVEL_TRIM_STEP_DECI_DEG = 25;    // Horizon trim increment size (2.5deg).
const float LEVEL_PORTRAIT_ENTER_RAD = 1.00f; // Enter portrait mode threshold.
const float LEVEL_PORTRAIT_EXIT_RAD = 0.75f;  // Exit portrait mode threshold.
const int LEVEL_LINE_MARGIN_PX = 10;        // Horizon line horizontal margin.
const int LEVEL_VERTICAL_LINE_LENGTH = 30;  // Center marker vertical line length.

// ---------------------------------------------------------------------------
// Main display layout coordinates
// ---------------------------------------------------------------------------
const int MAIN_HEADER_HEIGHT = 15;         // Header bar height.
const int MAIN_RETICLE_CENTER_Y_OFFSET = 5; // Reticle center vertical offset.
const int MAIN_RETICLE_CENTER_RADIUS = 3;  // Reticle center dot radius.
const int MAIN_ISO_X = 2;                  // ISO label X position.
const int MAIN_ISO_Y = 7;                  // ISO label Y position.
const int MAIN_APERTURE_X = 46;            // Aperture label X position.
const int MAIN_APERTURE_Y = 7;             // Aperture label Y position.
const int MAIN_SHUTTER_X = 2;              // Shutter label X position.
const int MAIN_SHUTTER_Y = 14;             // Shutter label Y position.
const int MAIN_DISTANCE_X = 68;            // LiDAR distance label X position.
const int MAIN_DISTANCE_Y = 7;             // LiDAR distance label Y position.
const int MAIN_LENS_X = 68;                // Lens distance label X position.
const int MAIN_LENS_Y = 14;                // Lens distance label Y position.
const int MAIN_LIDAR_QUALITY_X = 123;      // LiDAR quality indicator origin X.
const int MAIN_LIDAR_QUALITY_Y = 2;        // LiDAR quality indicator origin Y.
const int MAIN_LIDAR_QUALITY_SIZE = 2;     // LiDAR quality block size.
const int MAIN_LIDAR_QUALITY_GAP = 1;      // Gap between LiDAR quality blocks.

// ---------------------------------------------------------------------------
// Setup/calibration/health UI layout coordinates
// ---------------------------------------------------------------------------
const int CONFIG_TITLE_X = 3;              // Setup title X position.
const int CONFIG_TITLE_Y = 15;             // Setup title Y position.
const int CONFIG_HEADER_PADDING_Y = 4;     // Vertical padding below setup title.
const int CONFIG_ITEM_X = 3;               // Setup item X position.
const int CONFIG_ITEM_Y_START = 22;        // Setup item Y start.
const int CONFIG_ITEM_Y_STEP = 9;          // Setup item Y spacing.
const int CONFIG_FOOTER_Y = 121;           // Setup footer Y position.
const int CALIB_TITLE_X = 3;               // Calibration title X position.
const int CALIB_TITLE_Y = 15;              // Calibration title Y position.
const int CALIB_ITEM_X = 3;                // Calibration item X position.
const int CALIB_LENS_Y = 35;               // Calibration lens line Y position.
const int CALIB_DISTANCE_Y = 47;           // Calibration distance line Y position.
const int CALIB_PROGRESS_BAR_Y = 53;       // Calibration progress bar Y (below distance line).
const int CALIB_HELP_Y1 = 70;              // Calibration help line 1 Y.
const int CALIB_HELP_Y2 = 78;              // Calibration help line 2 Y.
const int CALIB_HELP_Y3 = 86;              // Calibration help line 3 Y.
const int HEALTH_TITLE_X = 3;              // Health title X position.
const int HEALTH_TITLE_Y = 15;             // Health title Y position.
const int HEALTH_ITEM_X = 3;               // Health item X position.
const int HEALTH_ITEM_Y_START = 30;        // Health item start Y.
const int HEALTH_ITEM_Y_STEP = 10;         // Health item line spacing.
const int HEALTH_FOOTER_Y = 112;           // Health footer Y position.

// ---------------------------------------------------------------------------
// External display layout coordinates
// ---------------------------------------------------------------------------
const int EXT_HEADER_HEIGHT = 10;          // External header bar height.
const int EXT_HEADER_FORMAT_X = 2;         // Format label X.
const int EXT_HEADER_FORMAT_Y = 8;         // Format label Y.
const int EXT_HEADER_DIVIDER_X = 33;       // Header divider X.
const int EXT_HEADER_LENS_X = 37;          // Lens label X.
const int EXT_HEADER_LENS_Y = 8;           // Lens label Y.
const int EXT_BATTERY_DIVIDER_FULL_X = 100; // Battery divider X when >=100%.
const int EXT_BATTERY_CURSOR_FULL_X = 104;  // Battery cursor X when >=100%.
const int EXT_BATTERY_DIVIDER_LOW_X = 111;  // Battery divider X for low percentage layout.
const int EXT_BATTERY_CURSOR_LOW_X = 115;   // Battery cursor X for low percentage layout.
const int EXT_BATTERY_DIVIDER_MID_X = 103;  // Battery divider X for mid percentage layout.
const int EXT_BATTERY_CURSOR_MID_X = 107;   // Battery cursor X for mid percentage layout.
const int EXT_PROGRESS_BAR_WIDTH = 90;      // Progress bar width.
const int EXT_PROGRESS_BAR_HEIGHT = 17;     // Progress bar height.
const int EXT_PROGRESS_BAR_X = 34;          // Progress bar X origin.
const int EXT_PROGRESS_BAR_Y = 15;          // Progress bar Y origin.
const float PERCENT_SCALE = 100.0f;         // Percentage scaling helper.
const int EXT_COUNTER_TEXT_Y = 30;          // Counter text baseline Y.
const int EXT_COUNTER_MESSAGE_X = 8;        // "Load film"/"Roll end" message X.
const int EXT_COUNTER_VALUE_X_WITH_PROGRESS = 8; // Counter X when progress bar is visible.
const int EXT_COUNTER_VALUE_X_NO_PROGRESS = 60;  // Counter X when no progress bar is visible.
const int EXT_SLEEP_FACE_CX     = 52;       // Sleep indicator face centre X.
const int EXT_SLEEP_FACE_CY     = 16;       // Sleep indicator face centre Y.
const int EXT_SLEEP_FACE_RADIUS = 12;       // Sleep indicator face radius (px).
const int EXT_SLEEP_ZZZ_X       = 70;       // Sleep indicator "Zzz" text cursor X.
const int EXT_SLEEP_ZZZ_Y       = 22;       // Sleep indicator "Zzz" text cursor Y.

// ---------------------------------------------------------------------------
// Static option lists and calibration flow thresholds
// ---------------------------------------------------------------------------
const int ISOS[] = {50, 80, 100, 125, 200, 400, 500, 640, 800, 1600, 3200, 6400}; // Supported ISO list.
const int CALIB_DISTANCE_COUNT = 10;         // Max calibration point count across all lens profiles.
const int CALIB_SAMPLE_COUNT = 8;            // Samples captured per calibration point.
const int CALIB_SAMPLE_DELAY_MS = 5;         // Delay between calibration samples.
const int CALIB_MIN_INLIER_COUNT = 5;        // Minimum inliers for a stable reading.
const int CALIB_OUTLIER_MAX_DELTA = 6;       // Max sample delta to still count as inlier.
const int CALIB_INLIER_SPREAD_MAX = 10;      // Max spread across accepted inliers.
const int CALIB_MONOTONIC_MIN_STEP = 1;      // Minimum monotonic step between calibration readings.
const int CALIB_CAPTURE_STATUS_NONE = 0;     // Calibration capture status: no error.
const int CALIB_CAPTURE_STATUS_UNSTABLE = 1; // Calibration capture status: unstable reading.
const int CALIB_CAPTURE_STATUS_NON_MONOTONIC = 2; // Calibration capture status: invalid sequence.
const unsigned long CALIB_ERROR_HOLD_MS = 2000; // Minimum display time for calibration errors.
const int CALIB_COMPLETE_LED_PULSES = 3;     // Number of green LED pulses on calibration complete.
const unsigned long CALIB_COMPLETE_LED_ON_MS = 80;  // LED on duration per pulse.
const unsigned long CALIB_COMPLETE_LED_OFF_MS = 80; // LED off duration between pulses.
const unsigned long CALIB_COMPLETE_HOLD_MS = 1500;  // Hold success screen after LED pulses.
const int CALIB_STATUS_Y1 = 98;              // Calibration status line 1 Y.
const int CALIB_STATUS_Y2 = 106;             // Calibration status line 2 Y.

// ---------------------------------------------------------------------------
// Light meter exposure constant
// ---------------------------------------------------------------------------
const int K = 20; // Calibration constant used in EV/shutter equations.

#endif // MRFCONSTANTS_H
