# MRF2 Firmware - Medium Format Rangefinder System

**Version**: 7.0  
**Platform**: ESP32-S3  
**Framework**: Arduino (PlatformIO)

## Overview

MRF2 Firmware drives the ESP32-S3-based medium format LiDAR rangefinder camera. For the hardware overview, PCBs, CAD, and BoM, see the root `README.md`; this document focuses on firmware build, architecture, and pin-level details.

## Hardware Interface (pins)

- **LiDAR Serial**: RX/TX pins on Hardware Serial 2
- **I2C Bus**: All I2C devices share the default I2C bus
- **Lens Position**: ADS1015 channel 1
- **Left Button**: GPIO 9 (internal pull-up)
- **Right Button**: GPIO 10 (internal pull-up)
- **Status LED**: Seesaw NeoPixel for visual feedback

## Software Dependencies

The project uses PlatformIO with the following libraries (versions resolved from the current build):

- Adafruit SSD1306 (2.5.13)
- Adafruit GFX Library (1.12.1)
- U8g2_for_Adafruit_GFX (1.8.0)
- Adafruit MAX1704X (1.0.3)
- DTS6012M_UART (1.2.0)
- BH1750 (1.3.0)
- Bounce2 (2.72.0)
- Adafruit SH110X (2.1.12)
- Adafruit seesaw Library (1.7.9)
- Adafruit ADS1X15 (2.5.0)
- Adafruit MPU6050 (2.2.6)
- Adafruit BusIO (1.17.1)
- Adafruit Unified Sensor (1.1.15)

## Project Structure

```
MRF2 Firmware/
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
│   └── inputs.h          # Input handling functions
├── src/              # Implementation files
│   ├── main.cpp          # Main program logic
│   ├── hardware.cpp      # Hardware instances
│   ├── globals.cpp       # Global variable definitions
│   ├── lenses.cpp        # Lens data arrays
│   ├── formats.cpp       # Film format data
│   ├── interface.cpp     # UI rendering implementation
│   ├── helpers.cpp       # Helper function implementations
│   ├── cyclefuncs.cpp    # Cycling logic
│   ├── setfuncs.cpp      # Sensor processing
│   └── inputs.cpp        # Button and encoder handling
├── platformio.ini    # PlatformIO configuration
└── README.md         # This file
```

## Firmware Architecture

### Main Program Flow

1. **Setup Phase** (`setup()` in main.cpp:47-133)
   - Disable WiFi/Bluetooth for power saving
   - Load saved preferences
   - Initialize all hardware components
   - Configure displays and show boot screen
   - Start sensor readings

2. **Main Loop** (`loop()` in main.cpp:135-174)
   - Check for sleep timeout (60 seconds)
   - Process button inputs
   - Update film counter
   - Read sensors (distance, light, battery)
   - Update appropriate UI based on mode

### Operating Modes

- **Main Mode**: Normal operation with rangefinder display
- **Config Mode**: Settings and configuration menu
- **Calibration Mode**: Lens calibration interface
- **Sleep Mode**: Low-power state with minimal display

### Key Features Implementation

#### Distance Measurement
- LiDAR sensor provides primary distance reading
- Moving average filter (15 samples) for stability
- Configurable offset compensation
- Range: 5cm to 18m

#### Light Metering
- Continuous lux readings from BH1750
- Automatic shutter speed calculation
- ISO range: 50-6400
- Aperture support for lens profiles

#### Lens System
- Mamiya Univeral Press Lenses
- Support for multiple lens profiles
- Position sensing via ADC
- 7-point calibration curve
- Automatic distance correlation

#### Film Management
- Multiple format support (6x4.5, 6x6, 6x7, 6x9, etc.)
- Frame counter with progress tracking
- Format-specific frame lines on display

## Building and Flashing

### Prerequisites
1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository
3. Connect ESP32-S3 board via USB

### Build Commands
```bash
# Build the project
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio run --target monitor
```

### First-Time Setup

1. Flash the firmware
2. Enter calibration mode to set up lens profiles
3. Configure ISO and aperture settings
4. Select appropriate film format

## Configuration

### Constants (mrfconstants.h)

- `FWVERSION`: Firmware version string
- `SLEEPTIMEOUT`: Auto-sleep delay (60000ms)
- `SMOOTHING_WINDOW_SIZE`: Filter window (15 samples)
- `LENS_INF_THRESHOLD`: Infinity focus threshold
- `LIDAR_OFFSET`: Distance calibration offset

### Preferences Storage

The system saves:

- Selected lens profile
- Film format selection
- ISO setting
- Aperture setting
- Film counter position
- Calibration data

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
