# Changelog

All notable firmware changes by released `FWVERSION`, reconstructed from git history.

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
  - Added awake-idle LiDAR standby that disables LiDAR after short inactivity in Main mode and wakes immediately on activity.
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
