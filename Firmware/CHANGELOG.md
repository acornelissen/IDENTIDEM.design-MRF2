# Changelog

All notable firmware changes by released `FWVERSION`, reconstructed from git history.

## 7.0 - 2025-11-18
- Initial public firmware import for MRF2.
- Baseline ESP32-S3 firmware structure, display/UI flow, sensor integration, and build config.
- Release commit: `4b1a976`

## 7.5 - 2025-12-19
- Refined parallax behavior and LiDAR calibration constants.
- Updated reticle alignment and LiDAR offset tuning.
- Added near-range correction and parallax guardrail constants.
- Release commit: `e23a8d5`
- Range: `4b1a976..e23a8d5`

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
