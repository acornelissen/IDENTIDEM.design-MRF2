# Changelog

All notable firmware changes by released `FWVERSION`, reconstructed from git history.

## Unreleased

### Documentation tooling

- The user-manual UI mockups are now generated from the real firmware draw
  code instead of being hand-drawn, so they render exactly like the device.
  A new `native_screenshots` build env compiles `interface.cpp` /
  `interface_config_screens.cpp` against software display canvases and writes
  a pixel-exact SVG per screen; `scripts/generate-screenshots.sh` regenerates
  them after any UI change. No firmware behavior change (the device build
  excludes the new `src/screenshots/` sources).
- User-manual accuracy and readability pass: distance-state and troubleshooting
  tables, plus previously undocumented details (backward-wired calibration
  error, EV Readout main-screen behavior, Health "Prefs" states, Factory Reset
  screen, uncalibrated-lens glyph).

## 10.7.0 - 2026-07-15

### Repo-wide review: LiDAR recovery fixes, regression guards, delivery-pipeline hardening

Findings from a five-track review (security, correctness, tests, CI/release, docs), all fixed. Verified on device including a test roll.

Firmware fixes:

- **LiDAR recovery no longer resets a healthy sensor while the scene is unmeasurable.** Aiming at open sky, past 18 m, or into hard sun streams valid frames whose candidates are all filtered out; those paths fed no event to the recovery state machine, so the first poll landing between frames escalated to a full reset/enable cycle roughly every 1.5 s and climbed the Health screen `Recoveries:` counter for as long as the drought lasted. Well-formed-but-filtered frames now count as link-healthy; the display behavior (`Inf?`, held readings) is unchanged, and true sensor silence still recovers.
- **LiDAR recovery backoff now actually gates retries.** Every timeout/error event rewrote the next-attempt deadline to "now", so the 250 ms - 2 s exponential backoff never applied; a genuinely failing sensor would have been reset at the 25 ms poll cadence with no settle time. The deadline now arms only on the transition into recovery and survives millis() rollover.
- **Health screen no longer reports downgraded prefs as "Defaults".** After flashing a newer firmware and rolling back, settings load best-effort but the Prefs line fell through to `Defaults`, pointing support triage the wrong way. It now reads `vN newer`.
- **Builds without a light meter no longer latch the meter task at its fast 100 ms cadence.** A single aperture or ISO change looked permanently "pending" because the no-meter path never synced the change markers; it now decays back to the normal 500 ms poll.
- Manual frame cycling now seeds the encoder through the same monotonic-clamped tuning-offset formula the frame estimator uses. The two copies had drifted (cyclefuncs lacked the clamp); divergence was latent within today's format tables but is now structurally impossible.
- Sign-compare hardening in the ISO/lens/format cycle functions: a negative index can no longer wrap past the bounds reset.

Documentation:

- **Setup > Lens > Focus offset (shipped in 10.6.0) is now in the user manual** and the lens-menu mockup.
- Fixed the manual's defaults table (sleep timeout is `1m30s`) and the PANO frame range (`0..20`). Refreshed the firmware README's project tree and library list.

Regression guards (native suite grows 86 to 106 cases):

- All NVS preference keys hoisted into `prefs_keys.h` with tests pinning the 15-char NVS limit (`selected_format` sits exactly at it) and key uniqueness — the v10.3.5 silent-persistence bug class.
- The v10.4.7 "setFrameRate is boot-only" rule is now a tested invariant: the recovery attempt sequence lives in a host-testable unit driven by a call-recording fake that asserts zero frame-rate writes and that the calibration profile is re-applied even when enable fails.
- Per-field mutation tests for the Main and Menu redraw-skip signatures — the v10.6.0 stale-menu bug class; a field dropped from either hash now names itself in a failing test.
- The runtime state-machine suite compiles the production sleep-timeout tables instead of a hand-maintained copy; the loaded-prefs clamp (the only defense against corrupted NVS indexing arrays) is extracted into `prefs_clamp_logic` and driven with corrupted inputs; light-meter EV-compensation direction, EV100 reference points, smoothing-mode clamps, and the iso=0 display cap are pinned; the frame cycle/estimate roundtrip is pinned across every format and the full tuning range; prefs downgrade and legacy-migration branches are pinned.

Delivery pipeline:

- **CI now exists.** Every pull request and push to main runs the native test suite, the ESP32 build, and a version-consistency check across the six release files; the web-updater deploy is gated on the tests passing.
- **update.mrf2.com now self-hosts the flasher.** esp-web-tools 10.2.1 is vendored into the site (previously loaded from the unpkg CDN with no integrity protection, meaning a CDN compromise could have served builders modified flashing code). `scripts/vendor-esp-web-tools.sh` regenerates the bundle for upgrades.
- **Reproducible builds.** PlatformIO and all thirteen libraries are exact-pinned, GitHub Actions are pinned to commit SHAs, and every published build ships a `sha256sums.txt` (also printed in the CI log) so binary tampering is detectable.
- **Tagged releases own their version slot on the updater.** A commit landing on main after a tag without a version bump can no longer silently replace the tagged binary, and unreleased main builds are labeled `unreleased main @ <sha>` in the version picker and the Latest Build row.
- Internal planning notes under `docs/` are no longer published to the public site.

## 10.6.3 - 2026-07-13

### Boot animation: fix bottom-edge text artifacts, tune speed

- Fixed stray version-text glyphs painted along the bottom of the external display at the start of the boot animation. Adafruit_GFX text wrap was left at its default (on), so while the version sat off the right edge the cursor wrapped to the left edge one line lower and drew there. Text wrap is now disabled for the boot screen, so off-screen glyphs are clipped as intended.
- Sped the animation up a little: per-frame delay 80 ms to 65 ms and the settle hold 500 ms to 400 ms (~1.7 s total, down from ~2.1 s). Verify the feel on device.

## 10.6.2 - 2026-07-13

### Slower boot animation

- Slowed the external-display film-advance animation so the film feed is easier to watch: 20 frames at 80 ms plus a 500 ms settle hold (~2.1 s total, up from ~1 s). Timing only — no logic change. Verify the feel on device.

## 10.6.1 - 2026-07-13

### External display boot animation

- Replaced the static `MRF X.Y.Z` boot splash on the external OLED with a film-advance animation: sprocket perforations tick along the top and bottom edges while the version rides in from the right and settles centred, like loading a fresh roll. The per-frame geometry lives in a new host-tested `boot_animation_logic` module; the draw loop stays thin. Boot timing is unchanged (~1 s). Verify boot timing and legibility on device.

## 10.6.0 - 2026-07-13

### Whole-codebase review fixes (safety, persistence, input, rendering, lifecycle)

Findings from a broader firmware review covering everything outside the measurement chain, all fixed:

- **Fixed a boot-loop crash when the status-pixel seesaw board is absent or fails init.** The external UI wrote to the NeoPixel unconditionally; the vendored library leaves its pixel buffer unallocated until `begin()` succeeds, so the first write panicked and the reboot repeated the same failure. All writes are now gated on `hardware.statusPixel`, matching the rest of the per-peripheral degrade-gracefully design.
- **Fixed a dead NVS key.** `prev_encoder_value` (18 chars) exceeded the ESP32 Preferences 15-char key limit, so every write silently failed and every read returned the default. Renamed to `prev_enc_val`; no migration needed since the old key never held real data.
- **Long-press wake no longer drops into the Setup/Health menu.** The long-press handlers lacked the wake guard both short-press handlers already have, so holding a button to wake the camera also acted on the newly-woken mode in the same call.
- **Completing lens calibration now clamps the aperture to the new lens.** Finishing calibration on a newly-selected lens left the previous lens's f-stop active until the user touched an aperture button or rebooted, giving a wrong exposure readout right after the normal calibrate-a-new-lens flow.
- **Setup > Lens > Focus offset and Setup > LiDAR > Distance offset now redraw immediately.** Both settings changed and persisted correctly but the menu's redraw-skip signature didn't include them, so the screen showed the stale number until an unrelated field happened to change.
- **A size-mismatched lens-calibration blob in NVS can no longer silently wipe a lens to "Inf." on every reading.** The load path now requires an exact size match before trusting a stored blob.
- **Legacy preferences migration now commits the schema marker last**, so a power loss mid-migration causes a retry on next boot instead of permanently orphaning the still-present legacy calibration data.
- Sleep fade now starts from the current display brightness instead of always flashing to near-maximum first.
- The light meter now retries a failed sleep/wake I2C command a couple of times before disabling itself, so one transient bus glitch no longer costs the meter for the rest of the session.
- The calibration error message ("Unstable reading" etc.) now clears reliably at its hold-time boundary instead of lingering until an unrelated redraw.
- Per-point calibration capture now cleans up the LiDAR UART afterward, same as the completion step already did.
- The deferred NVS flush now has a 10 s maximum-postponement backstop, so continuous activity (e.g. winding several frames) can no longer delay the write indefinitely.

### Accuracy review fixes (ToF/measurement chain)

Findings from an independent accuracy review of the measurement chain, all fixed:

- **Focus ring no longer renders against a dead LiDAR reading.** After signal loss or idle standby the distance readout showed a placeholder, but the focus-assist ring (and the parallax fallback) kept comparing the lens position against the last accepted distance — indefinitely. The measurement is now invalidated with the display, and only the aiming reticle draws while no live reading exists.
- **Light meter displays the nearest standard shutter speed.** The old buckets floored to the faster speed, a systematic 0 to -1 stop (mean half-stop) underexposure for anyone setting the displayed speed. Selection is now nearest-in-stops, with a half-stop courtesy band beyond each end of the 1/1000..1/2 table. *Verify on a test roll or against a reference meter: the lux calibration scale (1.77) was tuned while the old bias existed. If readings now run consistently hot, re-derive the scale — don't revert the rounding.*
- **Waking from sleep or standby no longer trips a spurious LiDAR recovery.** The first post-wake poll inevitably has no frame yet; with the pre-sleep timestamp still in the recovery state, the timeout fired instantly, running a reset+enable back-to-back with the wake enable (the v10.4.7 hazard) and inflating the Health screen's Recoveries counter on every wake. Recovery timing now restarts on every sensor enable.
- **Post-wake light readings start fresh.** The lux smoothing EMA survived sleep, so waking in different light blended in up to ~80% pre-sleep level for the first ~1.5 s. The EMA now resets when the meter wakes.
- **Switching lenses recomputes the focus distance immediately** instead of keeping the previous lens's table mapping until the focus ring moved.
- **The near-range LiDAR correction survives offset-pref changes.** The 130 cm -> 100 cm anchor was measured at the default 400 mm geometry offset; changing the offset pref shifted the correction's input frame and silently invalidated the anchor. The correction now evaluates in the default-offset frame and re-adds the delta.
- **Rounding instead of truncation** in the LiDAR mm->cm conversion (0-9 mm short bias on every reading), the lens-table interpolation (up to 1 cm short), and the battery percentage (which could also print "-0%" at power-on).
- **Boot no longer shows a bogus 1.0 m lens distance.** The lens smoothing window was zero-filled at boot and the spike filter latched the warm-up artefact for ~15 polls; the window is now primed with the first real ADC read.
- **Far focus marks only snap when the distance is actually close.** The fixed 3-count far deadzone spans metres on sparse tables (a reading interpolating to 8.5 m displayed "10.0m"); snapping now also requires the interpolated distance within 8% of the mark.
- **Dropouts flag the held reading.** During the 1 s grace window after signal loss the previous distance now shows as "Held:" instead of presenting as live, and the ~2 s calibration-complete celebration no longer leaves the LiDAR UART overflowed. The diagnostics fps count is normalised by the real window length.

### LiDAR driver update (DTS6012M_UART 3.0.0)

Bumped the sensor library dependency to `^3.0.0`. Public signatures, error codes, and the measurement/statistics layouts are unchanged, so the firmware builds and runs against it without code changes; two `DTSConfig` fields are appended. Notable behaviour changes and how the firmware handles them:

- **Quality grading and the median filter now exclude out-of-range readings**, so the sensor's valid-distance limit finally has teeth. The firmware previously set that limit from the frameline parallax cap (`DISTANCE_MAX`, 18 m); with the geometry offset added that clipped genuine far returns ~2 m short of the sensor's rated range. The library's `maxValidDistance_mm` is now set from a dedicated `LIDAR_LIBRARY_MAX_VALID_DISTANCE_MM` (20 m, the sensor's rating), decoupled from both the parallax cap and the 18 m display-to-"Inf." policy (`LIDAR_DISPLAY_INF_THRESHOLD_CM`).
- **The opt-in ambient-light gate (`maxSunlightBase`) is left disabled, deliberately.** The outdoor-range investigation established that `sunlightBase` reads flat outdoors (it measures ambient at the aperture, not the solar reflection off the target that causes the range cliff), so gating on it would reject valid frames without helping. Documented in `makeLidarConfig()` so it is not naively enabled later.
- **The median filter now drops POOR/under-threshold samples too.** When too few quality-valid samples remain it returns the invalid sentinel; the firmware already falls back to the raw primary distance in that case, so far/marginal readings still display, just unsmoothed.
- Carries the accuracy fixes first shipped in 2.8.0: raw distance 0 stays invalid instead of becoming a reading at the offset distance; AUTO CRC byte-order detection survives the host recovery path; and the median history clears on reset/standby so the first post-wake reading is not the previous subject's distance.
- Plus the 3.0.0 robustness work: a timeout no longer wipes a frame reassembling across `update()` calls (no permanent-TIMEOUT livelock), stale data is invalidated after a comms timeout, `enableSensor()` refreshes the library's own timeout clock, and `errorCount`/`getConsecutiveErrors()` now count timeouts.

### LiDAR driver update (DTS6012M_UART 2.7.0)

- Bumped the sensor library to `^2.7.0`, which fixes the frame-drain bug found during the range investigation (one `update()` now keeps the freshest queued frame instead of the oldest) and makes one-shot commands report real errors instead of false success.
- **Adapted to the new `update()` contract.** The library now returns `NO_NEW_DATA` (not `TIMEOUT`) whenever no complete frame arrived on a given poll — the normal case every time the loop runs faster than the frame rate. The recovery layer now treats `NO_NEW_DATA` as a benign, time-based event, so a healthy sensor no longer risks tripping spurious recovery. A genuine comms stall still surfaces as `TIMEOUT` after the no-data timeout. Added regression tests for the mapping and for the no-spurious-recovery guarantee.

### LiDAR Diagnostics screen

- The frame-rate line now shows a **measured** frames-per-second count (accepted frames over a rolling one-second window) instead of the sensor's boot-time self-report. The self-report reads 0 on current hardware because the DTS6012M does not answer the frame-rate query, so the old `act:` value was always misleading. The measured count is what confirmed, during the outdoor-range field investigation, that a lower frame rate is genuinely delivered yet does not extend range.

### Focus distance and LiDAR accuracy fixes

- **Lens focus readout fixed for the default 65mm lens.** The focus-distance table stored its sensor readings in descending order while the interpolation expects ascending, so between the calibrated marks the readout snapped to 1m (or `Inf.` at the near stop) instead of interpolating. The default table is now stored ascending, matching the convention the rest of the pipeline assumes, and a regression test guards that every calibrated lens table ascends. Verify against the marked distances on a calibrated 65mm lens.
- **Near-range correction no longer over-shrinks close subjects.** The single-reference power law pulled sub-metre readings far too low (a raw 65cm read out as 14cm and dropped below the display floor). The correction is now bounded so it can never remove more than 40% of the raw distance — an interim guard until the measured near-range table lands.
- **Confident far returns get a wide but finite trust margin.** A GOOD/EXCELLENT return past a near focus prior is still trusted, but only out to three times the prior. The sensor grades quality after normalising for distance, so a beam miss onto a bright far background can still read GOOD; a gross overshoot is now rejected regardless of grade.
- **Dual-peak fusion keeps the stronger reading's confidence.** When the primary and secondary returns agree, the fused confidence now takes the higher of the two (plus the agreement bonus) rather than the average, so a weak secondary can no longer demote a confident primary.
- **Focus distance interpolates in reciprocal-distance space.** Helicoid extension (what the linear sensor measures) tracks 1/distance, so the old linear-in-distance interpolation over-read across the sparse far marks. The between-mark readout is now physically correct and the far-mark stepping is softer.

### Lens calibration and focus robustness

- **Calibration requires readings that rise with distance.** A backwards-wired focus sensor captured a descending sequence the estimator reads inverted (the same class as the 65mm default bug, but reachable through the UI). Calibration now rejects it with a clear "readings decreasing — sensor wired backward?" prompt instead of silently storing an inverted table.
- **Uncalibrated lenses show `--` instead of `Inf.`** Selecting a lens with no calibration data previously displayed a believable `Lens: Inf.`; it now shows a neutral placeholder.
- **New Setup > Lens > Focus offset.** A per-camera focus fine-tune (ADC counts, applied in Main mode only) aligns the readout without a full recalibration and absorbs any Main-vs-calibration bias. Calibration still captures clean, so the stored table is unchanged.

### LiDAR recovery and responsiveness

- **The distance offset survives a partial recovery.** A recovery that reset the sensor but failed to re-enable it left the library distance offset zeroed — a silent ~40 cm short bias until the next fully successful recovery. The calibration profile (scale + offset) is now re-applied unconditionally after every reset.
- **Faster lens ADC sampling.** The lens-position ADC ran at 128 SPS, so a 3-sample read blocked the cooperative loop for ~24 ms and could delay buttons and the encoder. At 920 SPS the same read blocks ~3 ms; the averaging and spike filters absorb the slightly wider per-sample noise.

## 10.5.0 - 2026-06-21

Long-range LiDAR work: trust confident far returns, show the sensor's full rated range, and add a diagnostics screen so field testers can read back exactly what the sensor reports. Pairs with the v2 (MRF-Pro-v8) breakout's dedicated LiDAR regulator.

### LiDAR long-range behaviour

- **Confident far readings are no longer suppressed.** A GOOD/EXCELLENT return past the lens-focus prior is the sensor locking a real far subject, not a parallax beam miss, so the plausibility gate now trusts it. The allowed overshoot scales with the prior instead of a flat 2m, because beam-miss error grows with distance.
- **Display ceiling raised to the sensor's rated 18m** (was 10.5m), so genuine far subjects read out instead of collapsing to infinity early.

### LiDAR Diagnostics screen

- New **Setup > LiDAR > Diagnostics** screen showing live raw distance, intensity, sunlight base, SNR, quality, held state, requested vs actual frame rate, and error/recovery counts. It lets a tester read back exactly what the sensor returns while aiming at a target.
- The screen shows the **telemetry frame age** next to the frame rate (milliseconds when fresh, seconds when stale, `>99s` cap, `--` before the first frame) so stale values can't be mistaken for live ones.
- The sensor's actual frame rate is read back at boot so a future frame-rate experiment can confirm what was latched.

### Signal-loss readout

- A far-range dropout now shows **`Inf?`** instead of `Inf.`, so a lost signal above 3m is marked as a guess rather than passing for a real 18m measurement. The placeholder persists across a continuing dropout (e.g. aimed at the sky) instead of flickering for a single frame. Standby (`Zzz`) and near dropouts still resolve to `...` so wake starts clean.

### Boot screen

- The boot version text on the external display now shrinks to fit the screen width instead of wrapping, so three-part versions render cleanly.

### Tests

- Added coverage for the gate (confident far reads not suppressed, overshoot scales with prior), the display ceiling, the `Inf?` placeholder helper, and the telemetry-age formatter including `millis()` wrap. Test count: 53 → 66.

## 10.4.10 - 2026-05-31

### LiDAR plausibility gate

Field reports of the distance readout appearing to "cap" at short range traced to the lens-prior plausibility gate (added in 10.4.6) holding the previous value too aggressively when the lens is focused close. This release retunes it and makes the hold visible.

- **Faster release on a deliberate re-aim.** The gate no longer waits a blind 8 frames before accepting a far reading. It now releases as soon as two consecutive rejected readings agree (a real far subject the user has aimed at), and keeps an absolute 3-frame cap so a noisy beam-miss can never pin a stale value indefinitely. The release decision is a new pure, unit-tested helper (`updatePlausibilityHold`).
- **Wider focus coverage.** The gate now applies out to 3m of lens focus (was 2m), so the parallax beam-miss guard also covers portrait and group distances.
- **"Held:" indicator.** When the gate is showing the previous value instead of a live measurement, the main-screen label changes from `Dist:` to `Held:`, so a held reading reads as intentionally frozen rather than broken.

Verify in the field: with the lens focused near, panning to a farther subject should update within a frame or two, and the label should briefly read `Held:` while it does.

### Tests

- Added coverage for the stable-vs-noisy release logic and the reset path; updated the gate boundary tests for the new 3m focus threshold. Test count: 50 → 53.

## 10.4.9 - 2026-05-16

Internal refactoring release plus a setup-menu reorganisation. The menu reorg is the one user-visible change; everything else is structural. Pending field testing.

### Setup menu reorganisation

- The setup menu hierarchy has been restructured around user mental categories. Persistence is untouched — your settings come back where you left them, just in their new home.
  - **Reset frame counter** is now in **Setup > Film** (it's a film action, not navigation).
  - **Frame 1 offset** and **Frame spacing** moved to **Setup > Film > Frame counter tuning** (one-time calibration knobs, demoted from the Film top level).
  - **LiDAR distance offset** and **LiDAR idle timeout** moved into a new **Setup > LiDAR** submenu (they were miscategorised under UI Settings).
  - **UI Settings** is renamed **Display** and trimmed: it now holds brightness, horizon line on/off, sleep timeout, horizon-trim sub-page, and reticle adjust.
  - The three horizon-trim rows moved to **Setup > Display > Horizon trim** so they group cleanly.

### Subtle UX

- **Portrait/landscape hysteresis is now self-healing after a long config detour.** When the user spends time in a config menu and rotates the device, the level renderer's cached `portraitMode` could stay stuck at the old orientation until the user rotated past the hysteresis bands again. The renderer now resets the cache whenever it has been idle for >1s.
- **`toggleLidar()` now flips `lidarEnabled` before re-applying the calibration profile**, matching the order already used by the boot and retry paths. Functionally inert today; removes an ordering inconsistency.

### Internals (no user-visible behaviour change)

- **New host-testable logic modules** extracted out of hardware-coupled files: `lens_spike_logic`, `frameline_layout_logic`, `ui_signature_logic`, `formatting_logic`, `lidar_runtime_state` bundling. Each new module ships with unit tests.
- **LiDAR pipeline cleanups** in `lidar_logic.cpp`: distance-tier lookups, DataQuality profile table, shared candidate-validation gate, shared fuse-or-pick-best selection.
- **`handleRightButtonShortPress` split** into per-mode handlers; the dispatcher is now a clean switch on `ui_mode`.
- **Shared U8G2 text-defaults helper** consolidates four near-identical OLED-prep sequences in interface.cpp.
- **Display-layout pixel coordinates** moved out of `mrfconstants.h` into a dedicated `interface_layout_constants.h` so layout tweaks don't force a full-tree rebuild.
- **`globals.h` regrouped by domain** (Lightmeter / Lens / LiDAR / UI / Calibration / Frame counter / Sleep / Health). Module-private caches (lens moving-average buffer, `prev_distance`, `prev_lens_sensor_reading`) demoted to file-scope statics in the only module that touches them.
- **`helpers.cpp` reorganized**: lens moving-average buffer now lives next to `calcMovingAvg`; `cmToReadable` moved into its own pure module.
- **Sleep-wake baseline ownership centralised** in `finaliseSleepServices`; removed unreachable lazy-init paths in the wake-poll functions.

### Test coverage added

- Regression guards for the v10.4.7 LiDAR-recovery incident class (state machine resilience under sustained failure), the v10.4.6 lightmeter scale-split (`K * LIGHTMETER_LUX_CAL_SCALE` product + known-scene shutter band), and the v10.4.6 6x9/9x3 sensor-table alignment.
- New tests for hash primitives, lens spike filter, frameline scaling, cm-to-readable formatter, secondary-quality INVALID remapping, and the misnamed split lidar test.
- `DTSMeasurement` and `findLensById` builders consolidate test setup boilerplate.

## 10.4.8 - 2026-05-16

### Bug fixes

- **3x6 frame spacing (every-other-frame exposure on 120 film, issue #21)**: rewrite the 3x6 `sensor[]` table to use 120-film geometry. The original 3x6 row in `d4f73cf` was a verbatim copy of PANO with one entry appended — same per-frame encoder deltas as a 65mm-wide panoramic frame on 35mm film, applied to a 30mm-wide frame on 120 — so users were advancing roughly twice as far per frame as the mask allowed, leaving every-other-frame slot blank on the developed roll. New deltas are derived from a per-frame counts/mm regression across the four calibrated 120 formats (6x4.5, 6x6, 6x7, 6x9), evaluated at 30mm frame width + 5mm gap. Leading offset matches 6x7. Verify with a calibration roll; fine-tune via Setup > Frame counter tuning if needed.
- Strengthen `test_format_3x6_supports_21_frames` to assert 120-film geometry (leading delta ≥120, end sentinel 550) so future edits can't regress to PANO-shaped values.

## 10.4.7 - 2026-05-15

### Bug fixes

- **LiDAR recovery loop on sensitive DTS6012M units**: revert the v10.4.6 change that re-sent `setFrameRate(50)` from the recovery path. On some sensors, issuing `setFrameRate` immediately after `enableSensor()` (with no settle delay) leaves the unit in a state where the next `update()` times out, which retriggers recovery — a self-perpetuating loop that pinned the Health screen at `err:6` (UART timeout) with `Recoveries:` climbing indefinitely. Frame rate is once again set inline at boot only; the sensor retains it across enable/disable cycles. The distance scale/offset re-application in `applyLidarCalibrationProfile()` is preserved.

## 10.4.6 - 2026-05-02

### New menu items

- Add **LiDAR offset** to Setup > UI Settings (`0`–`800mm` in `10mm` steps, default `400mm`). Tunes the LiDAR reading to compensate for the sensor-to-lens-plane physical offset. Persisted to NVS; takes effect immediately so the live distance readout reflects the change without a reboot.

### UI improvements

- **Focus reticle adjustment** screen now shows the current X/Y offsets numerically with a `>` marker on the active axis, plus a fixed reference crosshair at the optical centre so the reticle dot's offset is visually unambiguous.
- **High-sunlight indicator** on the main UI: a small sun glyph appears next to the Dist readout when ambient IR (sensor `sunlightBase`) crosses a hysteresis threshold. Distinguishes "LiDAR struggling because bright" from "firmware broken." Initial thresholds need field tuning from real readings.

### Bug fixes

- **6x9 and 9x3 frame spacing**: reduce interframe advance by 1–2 encoder ticks per gap (cumulative 9 ticks at frame 8, recovering ~21mm = approximately 1/4 frame). Both formats shared the same out-of-tolerance sensor values.
- **Light meter overexposure of approximately 1.5 stops**: split the meter calibration constant `K` (now ISO-standard 12.5) from a new BH1750 mounting compensation `LIGHTMETER_LUX_CAL_SCALE` (1.77) that scales raw sensor lux up to actual scene illuminance.

### LiDAR pipeline robustness

- **Lens-prior plausibility gate**: when the lens is focused at or below 2m, reject LiDAR readings that overshoot the lens prior by more than 200cm. Catches the parallax beam-miss failure mode (LiDAR beam goes past the framed subject and finds distant background). Holds the previous valid reading instead. Falls through after 8 consecutive rejections so the user can deliberately re-focus past the previous target without being permanently stuck.
- **Subject-stable confidence boost**: when 5+ consecutive LiDAR readings stay within 5cm of each other, add +10 to the candidate's confidence (clamped at 95). Helps the temporal blend trust the locked-on value over occasional noise. Streak resets on dropout, on a jump beyond delta, or on plausibility-gate rejection.
- **Near-range temporal blend**: gentle 12% previous / 88% current blend when high-confidence readings are at or below 2m, reducing single-frame noise without introducing lag.
- Relax near-range SNR target from 300 to 180 permille so close subjects shot horizontally in full sun (open sky floods the photodetector with ambient IR) are no longer dropped.
- Defensively re-apply LiDAR frame rate after sensor recovery (previously only set at boot).
- Remove unused residual-correction lookup table and the unreachable double-correction wrapper around it. The power-law correction below 1.5m remains.

### Tests

- Test count: 31 → 38. Added pure-helper coverage for `isLidarReadingImplausible`, `applyStableConfidenceBoost`, `updateSunlightWarnState`.
- Update a stale assertion left over from the v10.4.5 near-distance precision feature (`150cm` now correctly asserts as `"1.50m"`).

## 10.4.5 - 2026-04-16

- Distance display below 2m now shows two decimal places (e.g. `1.85m`) instead of one.
- Add display brightness control to Setup > UI Settings:
  - **Brightness mode**: Auto (scales with ambient light from BH1750) or Manual.
  - **Brightness top** (Auto): sets the maximum brightness ceiling from 50% to 100% in 10% steps.
  - **Brightness level** (Manual): sets a fixed brightness from 5% to 100% in 5% steps.
  - Both settings persisted to NVS and survive reboots.
- Add **Horizon line** toggle to Setup > UI Settings (On/Off), persisted to NVS.

## 10.4.0 - 2026-04-15

- Add visual focus reticle offset adjustment (Setup > UI Settings > Focus reticle).
  - Step 1: L/R moves reticle horizontally; long press either button advances to step 2.
  - Step 2: L/R moves reticle vertically; long press either button saves and exits.
  - Offsets persisted to NVS and survive reboots. Range: -20 to +20 px.
- Reticle offset X/Y converted from compile-time constants to user-configurable settings.
- Fix right-button long press firing repeatedly on every loop iteration; it now fires exactly once per hold across all modes.

## 10.3.5 - 2026-04-02

- Reduce focus ring granularity from 1cm to 5cm steps so the ring locks when lidar and lens distances visually match, without snapping too early.
- Fix LiDAR idle timeout setting not persisting across reboots (NVS key exceeded ESP32 15-char limit).
- Change default sleep timeout to 1m30s and default LiDAR idle timeout to 1m.

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
