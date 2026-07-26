# IDENTIDEM.design M(edium Format) (R)ange(F)inder Two (MRF2)

[![Build Guide](https://img.youtube.com/vi/_sIWXoqOFIU/0.jpg)](https://www.youtube.com/watch?v=_sIWXoqOFIU)

MRF2 is version two of an open hardware / open firmware medium-format rangefinder camera for **Mamiya Press / Universal Press lenses**. It combines custom PCBs, ESP32-S3 firmware, LiDAR-based lens coupled focusing, dual OLED displays, and printed mechanical parts so you can build a fully featured medium-format shooter with configurable lens profiles and film formats.

It is based on the [Panomicron Thulium](https://www.panomicron.com/thulium) designed by Oscar Oweson, has been heavily modified, but has kept the best ideas from that camera.

Keep checking back regularly as I keep making improvements as I use my own copy of this camera. 
You can [support me on Patreon](https://patreon.com/IDENTIDEMdesign), or just show me your builds and the photos you're making with the camera!


## Repo at a Glance

- `Firmware/` – ESP32-S3 Arduino firmware (full details in `Firmware/README.md`)
- `PCBs/` – KiCad projects + production Gerbers, in two supported revisions (see **Which board version?** below):
  - `PCBs/v2.0/` (MRF-Pro-v8) – adds a dedicated low-noise LiDAR regulator on the breakout, and reworks the main board's FPC power delivery and switch: v1 fed the breakout from the switched Feather 3.3 V rail on FPC pin 6, with pin 6 and pin 1 shorted together on the breakout so pin 1's stray tie to Feather D11 sat on the same rail; v2 gives the LDO its own `VBAT` feed on pin 1 and adds full deterministic shutdown.
  - `PCBs/v1.0/` (MRF-Pro-v7.5) – simpler boards; great for short-range and landscape use.
  - Each revision holds `Main PCB/` (main board) and `Breakout/` (sensor breakout) sub-folders.
- `3MF/` – Print-ready 3MF files organized by part type (body, accessories, masks, fine parts)
- `STEP & F3D/` – Full assembly CAD (`MRF2-complete.f3d`) and shared STEP export (`MRF2-complete.step`)
- `OrcaSlicer/` – Slicer project files/profiles for printing

## Documentation Index

- `Documentation/flash-firmware/README.md` – Building and flashing the firmware on macOS, Linux, and Windows (VS Code or CLI)
- `Documentation/web-updater/README.md` – GitHub Pages browser updater architecture and deployment
- `Documentation/user-manual/USER_MANUAL.md` – On-device UI and calibration user manual
- `Documentation/wiring/README.md` – Power-switch/buttons and breakout-to-LiDAR harness wiring diagrams, for both board revisions
- `Firmware/README.md` – Firmware architecture, configuration, and CLI build commands

## Camera Features

- "LiDAR" distance measurement for accurate focus
- Dual OLED UI: 128×128 main in-viewfinder display + 128×32 external status display
- Light meter & exposure calculation for multiple ISO values
- Lens coupled via a high-accuracy linear position sensor
- Electronic frame counting using a rotary encoder
- Battery monitoring and on-device configuration, like in-camera lens calibration

## Which board version: v1 or v2?

Two board revisions are supported, and **the latest firmware runs on both, unchanged** — there are no board-specific builds, pin maps, or config flags. Connectors, sensors, displays, optics, printed parts, and the main board's component placement and outline are common to both, **but the main board's LiDAR power delivery was significantly reworked, alongside the breakout hardware.** On v1 the breakout ran entirely off one rail: the Feather's 3.3 V leaves the main board through the J4 power switch and comes back on FPC pin 6, and the breakout ties FPC pins 1 and 6 together, so the LiDAR's laser supply, its logic supply and the STEMMA QT bus all hang off the same undecoupled net. That is the laser-pulse brownout this respin fixes. Pin 1 also picks up a stray tie to Feather **D11** on the main board, which does nothing useful (the firmware never touches that GPIO) and blocks D11 for anything else. v2 splits the rails: pin 1 now carries `VBAT` straight to the breakout's own LDO, pin 6 stays the switched `3.3OUT` that gates it, and D11 is freed. Choose by the LiDAR range you need:

| | **v1 — MRF-Pro-v7.5** (`PCBs/v1.0/`) | **v2 — MRF-Pro-v8** (`PCBs/v2.0/`) |
| --- | --- | --- |
| Breakout | Bare — just the three connectors | Same connectors **+ a dedicated LiDAR LDO** (8 extra SMD parts) |
| FPC power delivery (main board) | One rail for everything: switched Feather 3.3 V on pin 6, with pin 1 shorted to it on the breakout and also tied to Feather **D11** (stray, unused) | Two rails: pin 1 carries **`VBAT`** to the breakout's LDO, pin 6 carries the gated, switched **`3.3OUT`** that enables it — D11 freed |
| LiDAR power | Shared Feather 3.3 V rail, no decoupling at the sensor | Isolated low-noise 3.3 V regulated **at the sensor** (TLV75533 off VBAT) |
| LiDAR range | Good up close and at landscape distance; **falls off at mid distance and in bright ambient** | **Full rated range indoors and in shade; direct sun still caps range** (~3 m on a sunlit target — a sensor optics limitation, not power-related; no board revision or firmware fixes it) |
| Power switch (main board) | Basic on/off — single shared 3.3 V net through J4 | Reworked **J4 DPDT**, two independent poles (gate `3.3OUT`; hold MCU `EN` low) for full deterministic shutdown (≤1 µA off) — same connector, board outline, and component placement, traces rerouted |
| Build effort | **Lowest** — breakout is a bare board, no SMD assembly | A bit more — breakout is an SMD assembly job (JLCPCB BOM/CPL provided) |
| Best for | Portraits ~1–3 m and landscapes; the cheapest, easiest build | Full-range focus indoors and in shade, better (not unlimited) in direct sun — the fuller-featured build |

- **Pick v1** for the simplest, cheapest build if you mostly shoot within a few metres or at landscape distances. Its LiDAR range can still be improved with the [hand-solder decoupling stopgap](Documentation/hardware-errata/lidar-stage1-decoupling.md), a v1-only remedy that v2 builds in.
- **Pick v2** for the sensor's full distance range indoors and in shade, deterministic power-off, and a main board that actually delivers real power to the LiDAR breakout, at the cost of the extra SMD parts on the breakout. Direct bright sun still caps range on either revision — that's the sensor fighting solar infrared at its own wavelength, not a supply problem this board fixes. The [LiDAR power errata](Documentation/hardware-errata/README.md) explains the why, and [field test protocol](Documentation/hardware-errata/lidar-field-test.md) covers all three lighting conditions including direct sun.

Building either? See the **[wiring guide](Documentation/wiring/README.md)** for the power-switch/buttons harness and breakout-to-LiDAR harness diagrams, per revision.

## Build Path (high level)

1. **Order PCBs**: Pick your version (see [Which board version?](#which-board-version-v1-or-v2)), then upload the Gerbers from that revision's `Main PCB/Gerber` and `Breakout/Gerber` folders — the `.gbrjob` files go straight to most fabs. For v2, the breakout is also an assembly job (the LDO SMD parts); its JLCPCB BOM/CPL live in `PCBs/v2.0/Breakout/JLCPCB/`.
2. **Assemble electronics**: Populate the main and breakout boards with the BoM below (through-hole and SMT mix), attach the Feather ESP32-S3, and wire the displays/sensors via STEMMA QT/Qwiic where applicable.
3. **Print the body**: Slice/print the 3MF models (see `3MF/` and any profiles in `OrcaSlicer/`). Fit tolerances may depend on your printer and material.
4. **Load firmware**: Follow `Documentation/flash-firmware/README.md` (macOS/Linux/Windows, VS Code or CLI), then run first-time calibration.
5. **Integrate**: Install the PCBs and displays into the printed parts, route wiring, and verify focus/meters on the bench before loading film.

## Bill of Materials

Marketplace listings (Amazon/AliExpress) change frequently. Treat those links as examples and verify specs before ordering.

### Electronics

| Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
| Feather ESP32-S3 (Adafruit 5477) | 1 | Main MCU, MAX17048 fuel gauge, 4MB flash / 2MB PSRAM | [Adafruit](https://www.adafruit.com/product/5477) |
| MRF2 Main PCB | 1 | Use Gerbers in your chosen revision's `Main PCB/Gerber` (`PCBs/v2.0/` or `PCBs/v1.0/`) | Any fab |
| MRF2 Breakout PCB | 1 | Use Gerbers in your chosen revision's `Breakout/Gerber`. **v2 breakout is an SMD assembly job** (see the LDO parts below) | Any fab |
| B-type 8-pin FPC ribbon cable | 1 | Links main PCB to breakout | [Amazon](https://www.amazon.co.uk/sourcing-map-Ribbon-Flexible-Printer/dp/B0F4K1KMKR/) |
| 8-pin JST-SH cable | 1 | For power switch and buttons, 10cm should be plenty | [AliExpress Search](https://www.aliexpress.com/w/wholesale-8-pin-jst%252525252dsh-cable.html) |
| 6-pin JST-SH cable | 1 | Connect the LiDAR sensor to the breakout board - some cutting and soldering needed on the connector supplied with the sensor | [AliExpress Search](https://www.aliexpress.com/w/wholesale-6-pin-jst%252525252dsh-cable.html) |
| LiDAR DTS6012M | 1 | Distance sensing | [AliExpress search](https://www.aliexpress.com/wholesale?SearchText=DTS6012M+UART) |
| 404R10KL1.0 Linear-position sensor | 1 | Lens position sensor | [Newark](https://www.newark.com/tt-electronics-bi-technologies/404r10kl1-0/linear-motion-potentiometer-10kohm/dp/15M2426) |
| Light sensor BH1750 breakout (Adafruit 4681) | 1 | Lux readings | [Adafruit](https://www.adafruit.com/product/4681) |
| IMU MPU6050 breakout (Adafruit 3886) | 1 | Orientation | [Adafruit](https://www.adafruit.com/product/3886) |
| ADC ADS1015 breakout (Adafruit 1083) | 1 | Lens position | [Adafruit](https://www.adafruit.com/product/1083) |
| STEMMA QT Rotary Encoder (Adafruit 5880) | 1 | Film advance tracking | [Adafruit](https://www.adafruit.com/product/5880) |
| 1.12" 128×128 SH1107 OLED (Adafruit 5297) | 1 | Main display | [Adafruit](https://www.adafruit.com/product/5297) |
| 128×32 SSD1306 OLED (I2C) | 1 | External status display | [AliExpress search](https://www.aliexpress.com/w/wholesale-128x32-oled-display-ssd1306.html) |
| JST-SH/Qwiic/STEMMA QT cables | 6-8 (extras if you want to shorten / modify) | I2C daisy-chain and encoder | [Adafruit](https://www.adafruit.com/product/4210) |
| LiPo cell (3.7V, 820mAh) - 44 x 30.5 x 6.8 mm | 1 | **CHECK POLARITY** | [Amazon](https://www.amazon.co.uk/dp/B082152887) |
| 8mm momentary button | 2 | Select buttons | [Amazon](https://www.amazon.co.uk/dp/B07S1MNB8C) |
| 6mm DPDT slide switch | 1 | Power switch | [Amazon](https://www.amazon.co.uk/dp/B07H9VPK1J) |

### Components for custom PCBs — connectors (both versions)

The same connectors populate v1 and v2:

| Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
| 0.5mm pitch 8-pin FPC connector (FPC-05F-8PH20, C2856797) | 2 | One on the main PCB, one on the breakout | [JLCPCB](https://jlcpcb.com/partdetail/XUNPU-FPC_05F8PH20/C2856797) |
| SH 1.0mm 8-pin connector (ZX-SH1.0-8PWT, C7430450) | 1 | Main board J4 — power switch and buttons | [JLCPCB](https://jlcpcb.com/partdetail/Megastar-ZX_SH1_08PWT/C7430450) |
| SH 1.0mm 6-pin connector (ZX-SH1.0-6PWT, C7430448) | 1 | Breakout J7 — LiDAR sensor | [JLCPCB](https://jlcpcb.com/partdetail/Megastar-ZX_SH1_06PWT/C7430448) |
| SH 1.0mm 4-pin connector (ZX-SH1.0-4PWT, C7430446) | 1 | Breakout J6 — STEMMA QT | [JLCPCB](https://jlcpcb.com/partdetail/Megastar-ZX_SH1_04PWT/C7430446) |

### v2 breakout only — dedicated LiDAR regulator

These SMD parts populate the v2 breakout's LDO (the extra cost/effort over v1). **v1's breakout has none of these** — it just carries the connectors above. Full design background is in the [LiDAR power errata](Documentation/hardware-errata/README.md).

| Ref | Part | LCSC | Role |
| --- | --- | --- | --- |
| U1 | TLV75533 LDO, SOT-23-5 | C404027 | Dedicated 3.3 V regulator for the LiDAR |
| Cin1 | 1 µF, 0402 | C52923 | LDO input decoupling |
| Cin2 | 10 µF, 0805 | C15850 | LDO input bulk |
| C1 | 47 µF, 1206 | C96123 | Laser-rail bulk (at the sensor's laser pin) |
| C2 | 1 µF, 0603 | C15849 | LDO output stability |
| C3 | 100 nF, 0402 | C1525 | High-frequency bypass |
| FB1 | 0 Ω jumper, 0805 | C17477 | Splits the logic/laser rails (fit a ferrite later if needed) |
| R1 | 100 kΩ, 0402 | C25741 | LDO enable pulldown (clean power-off) |

### Hardware

| Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
| Heat-set inserts | as needed | M2x4 & M3x4 | Local fasteners supplier |
| Countersunk screws | as needed | M2x4, M2x6, M3x6, M3x8 countersunk screws | Local fasteners supplier |
| M3x6 thumb screw | 1 | To hold the door closed | Local fasteners supplier |
| Steel rod - for door hinge | 1 | Cut to length: ~82mm | [Amazon](https://www.amazon.co.uk/HXJDAM-Stainless-Steel-Round-Diameter/dp/B0D2V6WB1Y) |
| 2-turn door handle spring | 1 | For advance lever auto-return | [Amazon](https://www.amazon.co.uk/dp/B076VRBT4M) |
| CSK8PP one-way bearing / sprag clutch | 1 | For advance lever, make sure inner and outer are keyed | [AliExpress Search](https://www.aliexpress.com/w/wholesale-CSK8PP.html) |
| Cold shoe | 1 | Mount on top | [Amazon](https://www.amazon.co.uk/Anwenk-Adapter-Bracket-Standard-Monitor/dp/B01MD29V3X) |
| 20x5x2mm bar magnets | as needed | 1 for the body, 1 for each mask you print | [Amazon](https://amzn.eu/d/ejitdei) |
| 5x2mm round magnets | 2 | 1 for the body, 1 for the key | [Amazon](https://www.amazon.co.uk/100PCS-Magnets-Neodymium-Whiteboards-Picture/dp/B09H2W3TQZ) |

### Optics

| Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
| 20.0mm Dia. x 70.0mm FL plano-convex lens (PCX) | 1 | Rear viewfinder lens | [Edmund Optics](https://www.edmundoptics.co.uk/p/200mm-dia-x-700mm-fl-uncoated-plano-convex-lens/2705) |
| 25.0mm Dia. x -25 FL plano-concave lens (PCV) | 1 | Front viewfinder lens | [Edmund Optics](https://www.edmundoptics.co.uk/p/250mm-dia-x25-fl-uncoated-plano-concave-lens/5542/) |
| 30x30x1.1mm 50R/50T beam splitter | 1 | Sits between front and rear optics | [AliExpress Search](https://www.aliexpress.com/w/wholesale-30x30x1.1-50R%25252F50T-beam-splitter.html) |


## Cost Estimate (ballpark)

Assumes single-quantity retail buys (Adafruit/Amazon/AliExpress), optics from Edmund Optics, and a rough USD/GBP conversion; excludes shipping/VAT/import and assembly labor.

- Electronics (MCU, sensors, displays, cables, LiPo, switches): ~$125 / ~£100
- PCB share (DIY assembly; amortized per build): ~$10 / ~£6  \[full 5× PCB batch ~ $35 / £28\]
- Hardware/fasteners/mech bits: ~$25 / ~£20
- Optics (lenses + beam splitter): ~$115 / ~£90
- Printed parts material: ~$25 / ~£20
- Rough per-build total: ~$300 / ~£235 (add shipping/taxes and any PCB batch overhead you keep)

## CAD & Printed Parts

- Full assembly in `STEP & F3D/MRF2-complete.f3d` (Fusion 360) and `MRF2-complete.step` (generic).
- Print-ready 3MFs are grouped by function under `3MF/`. Use provided slicer profiles in `OrcaSlicer/` as a starting point and tweak for your printer/material.

## PCB Notes

- Open the KiCad projects for your revision: **v2** `PCBs/v2.0/Main PCB/KiCAD/MRF-Pro-v8.kicad_pro` + `PCBs/v2.0/Breakout/KiCAD/MRF-Pro-v8-breakout.kicad_pro`; **v1** `PCBs/v1.0/Main PCB/KiCAD/MRF-Pro-v7.5.kicad_pro` + `PCBs/v1.0/Breakout/KiCAD/MRF-Pro-v7.5-breakout.kicad_pro`.
- Fabrication: the provided Gerber sets include copper, mask, paste, silkscreen, drills, and a `.gbrjob` for auto-detection at most PCB fabs.
- Revs: `PCBs/v2.0/` (`MRF-Pro-v8`) adds the dedicated LiDAR LDO on the breakout and reworks the main board's LiDAR power delivery: v1 carried the whole breakout on one switched 3.3 V net (FPC pin 6 out of J4, shorted to pin 1 on the breakout, with pin 1 also strapped to Feather D11); v2 rewires them to `VBAT`/`3.3OUT` and splits the J4 switch into a DPDT for full deterministic shutdown (same component placement and board outline, traces rerouted). `PCBs/v1.0/` (`MRF-Pro-v7.5`) is the simpler, unfixed original design. See [Which board version?](#which-board-version-v1-or-v2) and the [LiDAR power errata](Documentation/hardware-errata/README.md) for the design background.

## Firmware

Firmware source and build instructions live in `Firmware/README.md`. PlatformIO is used for builds/flash/monitoring. Calibrate lenses and film formats after first flash.

For browser-based updates, use the GitHub Pages firmware updater:
`https://update.mrf2.com/`

If a user reports the updater hanging (for example at "Preparing installation"), enable hidden debug mode by adding `?debug=1`:
`https://update.mrf2.com/?debug=1`

Then:
1. Reproduce the issue.
2. Click **Copy Debug Report**.
3. Send the JSON report back for diagnosis.

## License

Firmware and design files are released under the GNU GPL v3.0 (see `LICENSE`). 
