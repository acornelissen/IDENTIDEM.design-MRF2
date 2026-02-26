# MRF2 Firmware - Medium Format Rangefinder System

**Version**: 10.1.3  
**Platform**: ESP32-S3  
**Framework**: Arduino (PlatformIO)

## Overview

MRF2 Firmware drives the ESP32-S3-based medium format LiDAR rangefinder camera. For the hardware overview, PCBs, CAD, and BoM, see the root `README.md`; this document focuses on firmware architecture, configuration, and pin-level details.

## Hardware Interface (pins)

- **LiDAR Serial**: Hardware Serial 2 at 921600 baud
- **I2C Bus**: All I2C devices share the default I2C bus
- **Lens Position**: ADS1015 channel 1
- **Left Button**: GPIO 9 (internal pull-up)
- **Right Button**: GPIO 10 (internal pull-up)
- **Status LED**: Seesaw NeoPixel at address 0x36

## Software Dependencies

The project uses PlatformIO with the following libraries (declared in `platformio.ini`):

- Adafruit SSD1306 (^2.5.9)
- Adafruit GFX Library (^1.11.9)
- U8g2_for_Adafruit_GFX (^1.8.0)
- Adafruit MAX1704X (^1.0.3)
- DTS6012M_UART (^2.1.1)
- BH1750 (^1.3.0)
- Bounce2 (^2.72)
- Adafruit SH110X (^2.1.10)
- Adafruit seesaw Library (^1.7.6)
- Adafruit ADS1X15 (^2.5.0)
- Adafruit MPU6050 (^2.2.6)
- Adafruit BusIO (^1.16.0)
- Adafruit Unified Sensor (^1.1.14)

## Project Structure

```
Firmware/
├── include/          # Header files
│   ├── mrfconstants.h    # System constants and configuration
│   ├── hardware.h        # Hardware initialization declarations
│   ├── globals.h         # Global variables and state
│   ├── lenses.h          # Lens profiles and calibration data
│   ├── formats.h         # Film format definitions
│   ├── interface.h       # UI drawing functions
│   ├── helpers.h         # Utility functions
│   ├── cyclefuncs.h      # Cycling and selection functions
│   ├── setfuncs.h        # Sensor reading and setting functions
│   ├── inputs.h          # Input handling functions
│   ├── activity.h        # Shared activity/sleep state helpers
│   ├── loop_runtime.h    # Runtime scheduler and awake/sleep loop orchestration
│   ├── lidar_logic.h     # LiDAR fusion and correction logic
│   ├── lens_logic.h      # Lens sensor-to-distance mapping logic
│   ├── film_counter_logic.h # Film counter interpolation logic
│   └── lightmeter_logic.h # Shutter speed formatting logic
├── src/              # Implementation files
│   ├── main.cpp          # Main program logic
│   ├── hardware.cpp      # Hardware instances
│   ├── globals.cpp       # Global variable definitions
│   ├── lenses.cpp        # Lens data arrays
│   ├── formats.cpp       # Film format data
│   ├── interface.cpp     # UI rendering implementation
│   ├── helpers.cpp       # Helper function implementations
│   ├── activity.cpp      # Activity and sleep state implementation
│   ├── loop_runtime.cpp  # Runtime scheduler and task execution pipeline
│   ├── cyclefuncs.cpp    # Cycling logic
│   ├── setfuncs.cpp      # Sensor orchestration
│   ├── lidar_logic.cpp   # LiDAR processing pipeline
│   ├── lens_logic.cpp    # Lens distance estimation
│   ├── film_counter_logic.cpp # Film counter estimation
│   ├── lightmeter_logic.cpp # Light-meter/shutter conversion
│   └── inputs.cpp        # Button and encoder handling
├── platformio.ini    # PlatformIO configuration
└── README.md         # This file
```

## Firmware Architecture

### Main Program Flow

1. **Setup Phase** (`setup()` in `src/main.cpp`)
   - Disable WiFi/Bluetooth for power saving
   - Load saved preferences
   - Initialize all hardware components
   - Configure displays and show boot screen
   - Start sensor readings

2. **Main Loop** (`loop()` in `src/main.cpp`)
   - Delegate runtime scheduling to `runLoopRuntimeIteration()` in `src/loop_runtime.cpp`
   - Check configurable sleep timeout (`Off`, `15s`, `30sec`, `1m`, `1m30s`, `2m`; default `1m`)
   - Apply adaptive sensor polling and state-aware UI redraws to reduce idle power draw
   - Process button inputs
   - Update film counter
   - Read sensors (distance, light, battery)
   - Update appropriate UI based on mode

### Operating Modes

- **Main Mode**: Normal operation with rangefinder display
- **Config Mode**: Settings and configuration menu
- **Setup Root Menu**: film submenu, lens submenu, light-meter submenu, UI settings submenu, system health screen, reset, and exit
- **Film Submenu**: format selection, frame-1 offset, and frame-spacing offset
- **Lens Submenu**: Lens profile, parallax correction toggle, and lens calibration entry
- **Light Meter Submenu**: ISO, EV compensation, smoothing strength, EV readout toggle
- **UI Settings Submenu**: landscape/portrait horizon trims, sleep timeout, and LiDAR idle timeout
- **Health Screen**: firmware/prefs status, LiDAR error and recovery counters, and idle timer
- **Calibration Mode**: Lens calibration interface
- **Sleep Mode**: Low-power state with minimal display

### Key Features Implementation

#### Distance Measurement
- DTS6012M v2 provides primary and secondary returns per sample
- Confidence scoring combines data quality, intensity, ambient sunlight ratio (SNR), temporal consistency, and lens-position prior
- Two-stage correction: library scale/offset in mm, then curve/residual correction in cm
- Confidence-aware temporal smoothing (accept, blend, or hold previous reading)
- In Main mode, LiDAR idle standby timeout is user-configurable (`Off`, `15s`, `30sec`, `1m`, `1m30s`, `2m`; default `1m`)
- While LiDAR is in idle standby, the distance readout shows `Zzz`
- Distance display uses `cm` below `1m`, with `<15cm` near clamp and `Inf.` above `10.5m`
- Range: 5cm to 18m

#### Light Metering
- Continuous lux readings from BH1750
- Automatic shutter speed calculation
- ISO range: 50-6400
- Aperture support for lens profiles
- EV compensation control (+/- in 1/3-stop steps)
- Smoothing control (`Off`, `Low`, `Medium`, `High`)

#### Lens System
- Mamiya Press / Universal Press lenses
- Support for multiple lens profiles
- Position sensing via ADC
- Lens-specific calibration curves (up to 10 points, depending on lens)
- Automatic distance correlation

#### Film Management
- Multiple format support (PANO, 3x6, 6x4.5, 6x6, 6x7, 9x3, 6x9)
- Frame counter with progress tracking
- Format-specific frame lines on display

#### Level Aid
- Horizon line with accelerometer-based tilt compensation
- Automatic landscape/portrait orientation handling
- Per-orientation horizon trim controls (`Horizon L`, `Horizon P+`, `Horizon P-`) in `2.5deg` steps (`-30deg..+30deg`)

## Building and Flashing

For the step-by-step VS Code + PlatformIO workflow, see `Documentation/flash-firmware/README.md`.

### CLI Build Commands
```bash
# Build the project
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio run --target monitor
```

### Core Logic Regression Tests
```bash
# Runs focused host-side tests for:
# frame counter, LiDAR fusion/formatting,
# lens distance calibration logic, and light meter output.
pio test -e native_core_tests
```

## Configuration

### Constants (mrfconstants.h)

- `FWVERSION`: Firmware version string
- `DEFAULT_SLEEP_TIMEOUT_MODE` / `SLEEP_TIMEOUT_MODE_*`: Auto-sleep options (`Off`, `15s`, `30sec`, `1m`, `1m30s`, `2m`)
- `DEFAULT_LIDAR_IDLE_TIMEOUT_MODE`: Awake-main LiDAR standby timeout default (`1m`)
- `SMOOTHING_WINDOW_SIZE`: Filter window (13 samples)
- `LENS_INF_THRESHOLD`: Infinity focus threshold
- `LIDAR_LIBRARY_DISTANCE_SCALE`: LiDAR library distance scaling factor
- `LIDAR_LIBRARY_DISTANCE_OFFSET_MM`: LiDAR library distance offset in mm
- `LIDAR_NO_DATA_TIMEOUT_MS`: Timeout before showing unavailable distance
- `LIDAR_FUSION_MIN_INTENSITY`: Minimum intensity gate for valid candidates
- `LIDAR_CONFIDENCE_HIGH` / `LIDAR_CONFIDENCE_MEDIUM`: Confidence thresholds for smoothing behavior
- `LIDAR_CAL_CUTOFF_CM` / `LIDAR_CAL_REF_RAW_CM` / `LIDAR_CAL_REF_TRUE_CM`: Near-range calibration curve parameters
- `LIDAR_RESIDUAL_DIST_CM` / `LIDAR_RESIDUAL_DELTA_CM`: Piecewise residual correction table

### Preferences Storage

The system saves:

- Selected lens profile
- Film format selection
- Frame 1 offset and frame spacing offset
- ISO setting
- Aperture setting
- Film counter and encoder position
- Parallax correction toggle state
- Sleep timeout mode
- LiDAR idle timeout mode
- UI horizon trim offsets (landscape, portrait `P+`, portrait `P-`)
- Light-meter EV/smoothing/readout settings
- Lens calibration data

## Troubleshooting

### Display Issues

- Ensure I2C addresses match your hardware (0x3D for main, 0x3C for external)
- Check wire connections

### LiDAR Communication

- Check RX/TX connections
- Ensure adequate power supply

### Button Response

- Check debounce timing (5ms default)
- Test with serial monitor

## License

Released under the GNU General Public License v3.0 (see `LICENSE` in the repo root or this directory). Portions are based on the LRF45 CircuitPython code; please retain upstream notices where applicable.

## Contributing

See the root `README.md` for repo-wide contributing guidance. For firmware changes, include a clear description, update docs, and test on hardware where possible.
