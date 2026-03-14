# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test Commands

```bash
# Build for hardware (ESP32-S3 / Adafruit Feather)
pio run

# Upload to board
pio run --target upload

# Monitor serial output (115200 baud)
pio run --target monitor

# Run all host-side unit tests (core logic + runtime state machine)
pio test -e native_core_tests

# Run a single test suite by name (Unity framework, no per-test filtering)
pio test -e native_core_tests --filter test_core_logic_regression
pio test -e native_core_tests --filter test_runtime_state_machine
```

The `native_core_tests` environment compiles for the host (no Arduino runtime). Hardware-dependent code is excluded via `test_build_src = no`; stubs are supplied by [test_stubs/](test_stubs/).

## Architecture Overview

### Entry point and main loop

[src/main.cpp](src/main.cpp) handles `setup()` (disable Wi-Fi/BT radios, load prefs, init all peripherals in order) and delegates the entire `loop()` body to `runLoopRuntimeIteration()` in [src/loop_runtime.cpp](src/loop_runtime.cpp).

### Runtime scheduler (`loop_runtime.cpp`)

This is the heart of the firmware. It owns:

- **`LoopRuntimeState`** — all scheduler timestamps, UI cache state, sleep/wake flags, and activity baselines. It is a single file-scope object (no globals for scheduler state).
- **Sleep/awake branching** — every iteration calls `updateSleepMode()` then dispatches to `runSleepTasks()` or `runAwakeTasks()`.
- **Adaptive polling** — film counter, lens ADC, and light meter intervals all switch between a fast cadence and a slow idle cadence based on how recently the relevant signal last changed (`selectAdaptiveInterval`).
- **Dirty UI rendering** — primary display (main OLED or config/health overlay) is redrawn only when a FNV-1a hash of the relevant global state changes, or a forced-refresh deadline is reached. External OLED redraws are hashed independently.
- **LiDAR idle standby** — in Main mode, the LiDAR sensor can be put to sleep after a user-configurable idle timeout, distinct from the full device sleep timeout.
- **Preferences flush** — `flushPrefsIfDirty()` is called at a fixed cadence; NVS writes are coalesced, never done per-change.

### Global state (`globals.h` / `globals.cpp`)

All inter-module state lives as plain globals declared in [include/globals.h](include/globals.h) and defined in [src/globals.cpp](src/globals.cpp). There is no application-level singleton or dependency injection. Peripheral ready-flags (e.g., `adsReady`, `mpuReady`, `lidarSensorReady`) are checked before every peripheral access throughout the codebase.

### "Logic" modules (pure / testable)

The following modules are designed to be free of Arduino and hardware dependencies so they compile and test natively:

| Module | Purpose |
|---|---|
| `lidar_logic` | Candidate selection, confidence scoring, temporal blending, distance formatting |
| `lidar_recovery_logic` | Error/timeout detection, exponential backoff recovery state machine |
| `lens_logic` | ADC-to-distance interpolation, snap logic, infinity detection |
| `film_counter_logic` | Encoder-to-frame interpolation with hysteresis/debounce filter |
| `lightmeter_logic` | Lux-to-shutter-speed formatting |
| `calibration_logic` | Lens calibration sample stability and monotonicity validation |
| `prefs_migration_logic` | Schema version detection and legacy blob migration |

These modules expose pure functions operating on plain structs. All business logic should live here rather than in `setfuncs.cpp` or `loop_runtime.cpp`.

### Hardware-coupled modules

`setfuncs.cpp` bridges the logic modules to the real hardware globals (`lidar`, `theads`, `lightMeter`, etc.) and writes results into the global state. `inputs.cpp` handles Bounce2 button state and encoder reads, calling `registerActivity()` on any user input to reset the sleep timer.

### UI rendering (`interface.cpp`)

Draws all UI modes onto the SH1107 128×128 main OLED (via Adafruit SH110X + U8g2_for_Adafruit_GFX) and the SSD1306 128×32 external OLED. Mode dispatch is in `loop_runtime.cpp:drawPrimaryUiForCurrentMode()`. All layout coordinates and display constants are centralised in [include/mrfconstants.h](include/mrfconstants.h).

### Constants and configuration

[include/mrfconstants.h](include/mrfconstants.h) is the single source of truth for every tunable value: timing intervals, LiDAR fusion thresholds, display addresses, pin assignments, sleep timeout enums, menu step indexes, and preferences defaults. Prefer adding new constants here over magic numbers in `.cpp` files.

### Preferences (NVS)

Loaded at boot in `loadPrefs()` (`helpers.cpp`). Schema version is stored under `PREFS_SCHEMA_VERSION` (currently 2). On version mismatch or missing key, `prefs_migration_logic` selects `LOAD_DEFAULTS`, `MIGRATE_LEGACY`, or `LOAD_SCHEMA`. Dirty flag is set whenever a preference changes; `flushPrefsIfDirty()` in the scheduler does the actual NVS commit.

## Development Conventions

- **Language**: Use UK English in all comments, strings, documentation, and commit messages (e.g. "colour", "initialise", "behaviour").
- **TDD**: Write or update tests in `test/` before implementing new logic in a pure module. For hardware-coupled code that cannot be unit tested natively, verify manually on device.
- **SOLID**: Keep each module responsible for one concern. Pure logic modules (`lidar_logic`, `lens_logic`, etc.) must not take on hardware access; hardware-coupled code in `setfuncs.cpp` must not contain business logic.
- **DRY**: Extract repeated calculations or formatting patterns into helpers or constants in `mrfconstants.h` rather than duplicating them across modules.
- **Documentation**: After any user-visible change, update `README.md`, `CHANGELOG.md`, any relevant user manuals under `Documentation/`, and any UI SVGs that depict affected screens or menus.

## Git Commits

- **No co-author tag**: Never append "Co-Authored-By" lines to commit messages.


## Key Patterns

- **Peripheral guards**: always check the corresponding `*Ready` bool before accessing a peripheral object.
- **No blocking delays in loop**: all timing uses `shouldRunTask(nowMs, lastRunMs, intervalMs)` with cached timestamps; `delay()` is only used in `setup()`.
- **Test stub pattern**: [test_stubs/](test_stubs/) provides minimal `Arduino.h`, `Preferences.h`, and `DTS6012M_UART.h` shims. Test files `#include` the `.cpp` source directly to compile only the modules under test.
- **FNV-1a UI hashing**: before any display write, `buildMainUiSignature()` / `buildMenuUiSignature()` / `buildExternalUiSignature()` hash all state fields that affect that screen. Redraws are skipped if the hash is unchanged (and no forced-refresh interval has elapsed).
