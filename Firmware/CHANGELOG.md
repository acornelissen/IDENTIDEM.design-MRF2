# Changelog

All notable firmware changes by released `FWVERSION`, reconstructed from git history.

## 10.3.5 - 2026-03-31

- Reduce focus ring granularity from 1cm to 5cm steps so the ring locks when lidar and lens distances visually match, without snapping too early.

## 10.3.4 - 2026-03-30

- Retune LiDAR accuracy tiers to match subject distance importance:
  - ≤2m (was ≤3m): super accurate — intensity gate 40, SNR target 300‰, full prior influence.
  - 2–5m (was 3–7m): less accurate — intensity gate 10, SNR target 150‰, 60% prior influence.
  - 5–7m (new tier): even less accurate — intensity gate 3, SNR target 40‰, 35% prior influence.
  - 8m+ (was 7–12m): just get a value — intensity gate 1, SNR target 10‰, 20% prior influence.

## 10.3.3 - 2026-03-30

- Fix buffer overflow in lightmeter when exposure time reaches very large second values (uncapped `dtostrf` into a 10-byte buffer).
- Lightmeter exposure display now shows seconds in 0.5s increments (1, 1.5, 2, 2.5 …) and switches to minutes+seconds format (e.g. 1m30s) at one minute.
- Cap lightmeter exposure display at 25 minutes.

## 10.3.2 - 2026-03-20

- Updated DTS6012M_UART library to v2.5.3 (from v2.2.1). v2.5.2–2.5.3 fix stale circular buffer and `_lastValidFrameTime` not updating on `sendOneShot` timeout — both caused `getFirmwareVersion`/`setFrameRate` to starve the measurement stream. All `setFrameRate`/`getFirmwareVersion` calls now use the library API directly.
- Display sensor firmware version in system health screen.
- Use library `newDataAvailable()` flag and `getFilteredDistance()` median filter for jitter reduction.
- Set LiDAR frame rate to 50 fps for improved far-range integration time.
- Skip secondary candidate when no dual-peak target detected.
- Reset `prev_distance` on display clear to prevent stale temporal penalty.
- Show "Inf." instead of "..." when signal lost above 3 m.

## 10.3.1 - 2026-03-18

- LiDAR range and library update:
  - Updated DTS6012M_UART library to v2.2.1. Library now exposes IIC register access (`writeIICRegister`/`readIICRegister`), diagnostic histogram and SPAD heatmap streams, CRC AUTO mode with configurable auto-switch threshold, and `getTemperatureCode()`. `assessDataQuality()` scales the intensity threshold by inverse-square-law distance for fairer far-range quality assessment. Test stub updated to match v2.2.1 struct layout and enum values.
  - Retuned LiDAR fusion pipeline into three accuracy tiers: strict ≤3m (high accuracy), relaxed 4–7m, and very lax 8m+ (accept almost any return signal). Range boundaries widened from 2.2/5/10m to 3/7/12m.
  - Far-range intensity gates significantly lowered: mid 10→5, far 3→1, max-range unchanged at 1.
  - SNR targets relaxed at range: mid 200→100‰, far 120→25‰, max-range 80→10‰. Hard-reject floor 15→8‰.
  - Gentler SNR penalty: divisor 25→30, max 10→8 (fallback 5→3).
  - Fallback path: min intensity 2→1, base confidence 20→25, ceiling 40→50.
  - Temporal penalty reduced: divisor 20→25, max 14→10 — less suppression of noisy far-range frame-to-frame jumps.
  - Reset `prev_distance` to 0 when the display clears so the next valid reading is not penalised by a stale temporal jump.
  - Low-confidence per-frame step limit raised from 60→300cm for faster recovery after dropout.
  - Low-confidence blend weights widened 0.12–0.28→0.20–0.35.
- LiDAR UX improvements:
  - Signal loss at far range (last reading >3m) now shows "Inf." instead of "..." — tells the photographer the subject is beyond range, not that the sensor has failed.
  - Hold timeout increased from 750ms to 1000ms to bridge intermittent dropouts at the range boundary.
- Removed diagnostic serial logging from LiDAR pipeline.

## 10.3.0 - 2026-03-15

- Usability improvements:
  - Boot progress: main display now shows an "Initialising..." progress bar with a label naming the peripheral group being started (e.g. "Sensors", "Displays"), instead of staying blank for ~2 seconds.
  - Calibration UX: the calibration screen now shows a progress bar, a point counter (e.g. "3/7"), revised help text explaining the focus-ring workflow, and clearer error messages ("Hold lens still and retry" / "Increase focus distance") held on screen for a minimum of 2 seconds. A green LED pulse confirms each successful capture. On completion, a full-screen "Calibration complete!" message is shown with a triple green LED pulse, held for 1.5 seconds.
  - Calibration inlier check: sample stability validation now uses median-based spread instead of min/max range, making it more tolerant of occasional outlier readings.
  - Menu breadcrumbs: submenu headers now show the navigation path (e.g. "Setup > Film", "Setup > Lens", "Lens > Calibrate").
  - Expanded horizon trim labels: "Horizon L" / "Horizon P+" / "Horizon P-" are now "Horizon Landscape" / "Horizon Portrait+" / "Horizon Portrait-".
  - Calibration text wrap: moved "(R) to Cancel" to its own line on the calibration capture screen to prevent text overflowing the 128 px display width.
  - Calibration progress bar: inset to align with text edges instead of spanning full screen width.
  - Focus ring: increased max radius (30→40 px) and max thickness (3→5 px) for more prominent out-of-focus indication. Added EMA smoothing on the radius to reduce twitchiness near focus.
  - Calibration LED: green capture pulse now restores the previous LED colour instead of turning it off.
  - Sleep fade: the main OLED fades to black over ~200 ms using a non-blocking state machine before powering off, instead of blanking abruptly. Brightness is restored on wake.
  - Setup value previews: Film, Lens, and Meter entries on the root Setup menu show their active value inline (e.g. "Film: 6x7 >", "Lens: 65/6.3 >", "Meter: ISO400 >").
  - Health screen: added **Retry LiDAR** option (R button) when LiDAR failed to initialise, allowing re-initialisation without a power cycle.
  - Health screen: added **Factory Reset** option (long-press R) with confirmation screen. Clears all NVS preferences and reboots to defaults.
- LiDAR outdoor reliability:
  - Lowered near-range minimum intensity gate from 60 to 40 and SNR hard-reject floor from 40‰ to 25‰, reducing false `...` dropouts in bright sunlight.
  - Lowered SNR confidence penalty targets for near (420→300‰) and mid (280→200‰) ranges so readings lose less confidence under high ambient light.
  - Extended fallback candidate path to all ranges (previously blocked ≤220 cm), allowing low-confidence tracking at close distances when primary filtering rejects.
  - Increased no-data display timeout from 500 ms to 750 ms so brief dropouts hold the last valid reading instead of flashing `...`.
- Bug fixes (from 10.2.1):
  - Fixed calibration median index off-by-one: used lower median `(sample_count - 1) / 2` instead of upper median `sample_count / 2` for even sample counts.
  - Replaced `ev_readout == ev_readout` NaN self-comparison with explicit `isnan()` for clarity and robustness against `-ffast-math`.
  - Aperture index is now clamped after every lens switch, preventing out-of-bounds access when switching from a lens with more aperture stops to one with fewer.
- NVS flash wear reduction (from 10.2.1):
  - All `savePrefs()` calls in `cyclefuncs.cpp` now pass `PREFS_DIRTY_SETTINGS` instead of defaulting to `PREFS_DIRTY_ALL`.
- Code quality and maintainability:
  - Consolidated three cyclic value helpers into a single `cycleValueWrapping()` template.
  - Unified LiDAR display clearing into a single parameterised function.
  - Extracted `LENS_APERTURE_COUNT` constant and removed `sizeof` duplicates.
  - Extracted `markPrefsClean()` helper and simplified `getFocusRadius()`.
  - Removed redundant aperture bounds check and duplicate colour reset function.
  - Removed dead `prev_bat_per` and `prev_lux` globals.
  - Added compile-time `static_assert` validation for menu step constants.
  - Fixed external UI SVG progress bar clipping and frame counter alignment.
- Release metadata/docs:
  - Bumped `FWVERSION` to `10.3.0`.
  - Rewrote lens calibration section in user manual to explain focus-ring/sensor relationship.
  - Updated firmware README, user manual, CHANGELOG, and UI SVGs for all changes.

## 10.2.0 - 2026-02-28

- Power and runtime efficiency:
  - Replaced busy-polling in sleep mode with `esp_light_sleep_start()`, cutting CPU current between poll cycles from ~20–30 mA to ~0.8 mA.
  - Button GPIOs (`BUTTON_LEFT_PIN`, `BUTTON_RIGHT_PIN`) now configured as GPIO hardware wakeup sources during device sleep for immediate response.
  - Encoder and lens ADC polls unified on a 100 ms timer wakeup during device sleep (encoder previously polled at 50 ms).
  - MPU6050 now placed into hardware sleep on device sleep entry and woken on exit.
  - CPU frequency scaled down to 80 MHz during device sleep and restored to 240 MHz on wake.
  - Film counter idle polling interval increased 25 ms → 75 ms to reduce unnecessary ADC reads.
  - Battery gauge polling interval increased 1.5 s → 5 s (battery state changes slowly).
  - UI redraw tick slowed from ~30 Hz (33 ms) to 20 Hz (50 ms).
- Sleep indicator:
  - External OLED sleep indicator replaced: `ZzzZzzZZz...` text replaced with a minimal circle-face graphic plus `Zzz` label.
- Bug fixes (found during hardware testing):
  - Button wakeup from light sleep now fires correctly on both press and release. Button-release Bounce2 `rose()` events were previously missed because `checkButtons()` was only called on GPIO wakeup causes; the button release arrives on the subsequent timer wakeup, so `checkButtons()` now runs unconditionally on every wakeup cycle.
  - Battery percentage now appears immediately at boot instead of after the first 5-second battery poll cycle.
  - I2C bus speed is now restored to 400 kHz after each display write; the SH1107 display constructor previously left Wire at 1 MHz, which is above the MAX17048 rated maximum.
  - Battery gauge (`MAX17048`) now reliably reports as ready when sharing I2C address `0x36` with the Seesaw peripheral. The library's soft-reset sequence deliberately expects a NACK from the MAX17048 (which resets before acknowledging), but the Seesaw at the same address ACKed the write, causing `begin()` to report failure despite the gauge being operational. A fallback SOC plausibility check now sets `batteryGaugeReady` correctly.
- Testing:
  - Extended runtime state machine test suite: boundary tests for all five sleep timeout modes, `MODE_OFF` coverage, and constant-value guards for `LOOP_SLEEP_LIGHT_SLEEP_US`, `SLEEP_WAKE_ENCODER_DELTA`, and `SLEEP_WAKE_LENS_DELTA`.
- Code quality and maintainability:
  - Lifted hidden function-local statics in `setfuncs.cpp` into named namespace-scope structs (`LensSpikeFilterState`, `LensSnapState`, `LightMeterSmoothingState`, `LidarRecoveryState`) for explicit ownership and easier testing.
  - Split the `checkButtons()` monolith into three focused handlers: `handleLeftButtonShortPress()`, `handleRightButtonLongPress()`, `handleRightButtonShortPress()`.
  - Decomposed `drawLevelIndicator()` into `readAccelerometer()`, `updatePortraitMode()`, `computeLevelAngles()`, and `renderLevelLine()`.
  - Named magic OLED command bytes as `OLED_CMD_DISPLAY_OFF`/`OLED_CMD_DISPLAY_ON` constants.
  - Extracted `advanceMenuStep()` helper to eliminate five identical menu step-cycling blocks.
- Release metadata/docs:
  - Bumped `FWVERSION` to `10.2.0`.
  - Updated firmware README, user manual, and camera UI SVG snapshots.

## 10.1.2 - 2026-02-26
- Lens calibration and distance-scale updates:
  - Added per-lens calibration point-count support (up to `10` markers per lens profile).
  - Updated `150/5.6` marker set to `2, 2.5, 3, 5, 10`.
  - Updated `250/5.0` marker set to `2.5, 4, 5, 7, 8, 10, 15, 20, 30, 50`.
  - Added `250/8.0` profile with marker set `3.5, 4, 5, 7, 10, 15, 20, 30, 50`.
  - Calibration UI now reads target distances from the selected lens profile.
  - Lens snap/interpolation now uses active profile markers and ignores trailing unused slots.
  - Preferences loading now tolerates older saved calibration array sizes during schema evolution.
- Power and runtime efficiency:
  - Added awake-idle LiDAR standby that disables LiDAR after inactivity in Main mode and wakes immediately on activity.
  - LiDAR idle timeout is now user-configurable in `UI Settings` with the same options as sleep timeout (`Off`, `15s`, `30sec`, `1m`, `1m30s`, `2m`) and defaults to `1m`.
  - Main UI now shows `Dist: Zzz` while LiDAR is in idle standby.
  - Added state-aware UI redraw caching so displays redraw only when state changes (with bounded periodic refresh for Main/Health screens).
  - Added adaptive polling intervals for film counter, lens sensor, and light meter (fast when active, slower when stable).
- Code quality and maintainability:
  - Refactored loop orchestration from `main.cpp` into `loop_runtime.cpp` to isolate scheduling and sleep/awake task routing.
  - Decomposed `interface.cpp` into reusable rendering helpers for main/config/external displays.
- Release metadata/docs:
  - Consolidated release metadata under `10.1.2`.
  - Updated firmware README, user manual, and camera UI SVG snapshots.

## 10.1.1 - 2026-02-25
- Viewfinder/UI tuning:
  - Removed the boxed portrait-mode `P` indicator from the main display.
  - Increased `UI Settings` horizon trim granularity from `5deg` to `2.5deg` steps (`-30deg..+30deg`).
  - Added backward-compatible migration for older stored trim keys to the new `0.1deg` internal format.
- Release metadata/docs:
  - Bumped `FWVERSION` to `10.1.1`.
  - Updated firmware README, user manual, and camera UI SVG snapshots.

## 10.1.0 - 2026-02-24
- Film format and frame-counter updates:
  - Added `3x6` format (`30 x 56`) with `0..21` frame range.
  - Kept format ordering consistent in firmware/docs (`PANO`, `3x6`, `6x4.5`, ...).
  - Increased format-point capacity to support larger frame maps.
- Viewfinder level updates:
  - Added adaptive horizon behavior that auto-switches between landscape and portrait handling.
  - Added portrait-mode indicator (`P`) on the main display with high-contrast boxed rendering.
  - Added configurable horizon trim offsets in `UI Settings` for `Horizon L`, `Horizon P+`, and `Horizon P-` (`-30..+30`, `5` degree steps, defaults `0`).
  - Moved sleep timeout setting into `UI Settings`.
  - Tuned portrait roll/pitch behavior for more accurate in-hand leveling.
- Robustness and cleanup:
  - Removed redundant logic paths in activity/LiDAR/encoder handling.
  - Tightened bounds handling for frame/smoothing index usage.
  - Reduced variable shadowing and stale fallback checks.
- Release metadata/docs:
  - Bumped `FWVERSION` and updated README/manual/UI version snapshots.

## 10.0.3 - 2026-02-22
- LiDAR ambient-light handling:
  - Added sunlight-aware SNR scoring (`intensity` relative to `sunlightBase`) for primary/secondary/fallback candidates.
  - Low-SNR frames now reduce confidence first, with hard rejects reserved for very weak signals under strong ambient light.
- Setup menu ordering:
  - Moved `Reset frame counter` to sit directly above `Sleep timeout` in the root Setup menu.
- Docs/UI snapshots:
  - Updated firmware README, user manual, and camera setup/health UI SVG snapshots for 10.0.3.

## 10.0.2 - 2026-02-22
- LiDAR outdoor range tuning:
  - Lowered library minimum intensity threshold (`200` -> `80`) to preserve marginal long-range returns in bright light.
  - Relaxed mid/far fusion intensity gates (`14/8/4` -> `10/6/3`) to reduce long-range dropouts.
  - Reduced temporal penalty cap (`18` -> `14`) so far-distance updates recover faster after noisy frames.

## 10.0.1 - 2026-02-22
- Film submenu updates:
  - Added configurable `Current frame` setting in Film menu.
  - `Current frame` range is now constrained by selected format maximum (for example, `6x6: 0..12`, `6x7: 0..10`).
  - Manual frame changes now re-sync encoder position and frame-progress state.

## 10.0.0 - 2026-02-22
- Settings UI updates:
  - Split setup into focused submenus for Lens and Light Meter settings.
  - Added top-level configurable sleep timeout setting: `Off`, `15s`, `30sec`, `1m`, `1m30s`, `2m` (default `1m`).
  - Unified vertical padding under setup/menu headers for consistent layout.
- Runtime behavior:
  - Sleep timeout now uses the persisted menu setting instead of a fixed constant.
  - LiDAR recovery flow no longer escalates recovery attempts on filtered/noisy (non-error) frames.
  - LiDAR distance readout now shows values below `1m` in `cm`.
- Preferences:
  - Added persisted key for sleep timeout mode (`sleep_to_mode`), with load-time clamping.
- Release metadata/docs:
  - Updated firmware README, user manual, and UI SVG snapshots for release consistency.

## 9.6.0 - 2026-02-17
- Web updater improvements:
  - Added browser-based GitHub Pages firmware updater.
  - Improved updater UX (erase handling + reboot guidance).
  - Auto-retry for transient web installer initialization failures.
- Firmware/UI maintenance:
  - Refined LiDAR quality indicator layout/docs.
  - Removed dead firmware code paths.
  - Fixed film-counter regression causing immediate roll-end on first advance.
  - Suppressed first transient updater error dialog while retrying.
- Release commit: `73722aa`
- Range: `8abf464..73722aa`

## 9.5.0 - 2026-02-17
- LiDAR and sensing pipeline updates:
  - Stabilized lens ADC sampling/calibration behavior.
  - Migrated DTS6012M integration to v2 API with CRC AUTO.
  - Bumped `DTS6012M_UART` to `v2.1.1`.
  - Tuned v2 filtering for stronger confidence behavior.
  - Added dual-target LiDAR fusion with staged calibration/confidence gating.
  - Added LiDAR quality indicator blocks in the main UI.
- Firmware refactor:
  - Modularized logic and removed duplication across firmware modules.
- Documentation updates:
  - Added/updated firmware manual assets and LiDAR v2 documentation.
  - Removed legacy firmware docs/PDF references.
- Release commit: `8abf464`
- Range: `4a8e8ab..8abf464`

## 9.0.0 - 2026-01-19
- Improved runtime behavior:
  - LiDAR reads made non-blocking.
  - Added parallax toggle and viewfinder-based framelines.
  - Moved hardcoded values into `mrfconstants`.
  - Added reset confirmation UI flow.
  - Refined lens distance update behavior and trimmed constants.
- Fixed/adjusted display behavior:
  - Frameline sizing and wide-format overflow handling.
  - Out-of-range LiDAR display clamping.
  - LiDAR UI refresh after sleep.
  - Light-meter clamping and refresh on ISO/aperture change.
- Release commit: `4a8e8ab`
- Range: `e23a8d5..4a8e8ab`

## 7.5 - 2025-12-19
- Refined parallax behavior and LiDAR calibration constants.
- Updated reticle alignment and LiDAR offset tuning.
- Added near-range correction and parallax guardrail constants.
- Release commit: `e23a8d5`
- Range: `4b1a976..e23a8d5`

## 7.0 - 2025-11-18
- Initial public firmware import for MRF2.
- Baseline ESP32-S3 firmware structure, display/UI flow, sensor integration, and build config.
- Release commit: `4b1a976`
