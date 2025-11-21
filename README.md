# IDENTIDEM.design M(edium Format) (R)ange(F)inder Two (MRF2)

[![Build Guide](https://img.youtube.com/vi/sIWXoqOFIU_/0.jpg)](https://www.youtube.com/watch?v=sIWXoqOFIU_)

MRF2 is version two of an open hardware / open firmware medium-format rangefinder camera for Mamiya Press lenses. It combines custom PCBs, ESP32-S3 firmware, LiDAR-based lens coupled focusing, dual OLED displays, and printed mechanical parts so you can build a fully featured medium-format shooter with configurable lens profiles and film formats.

It is based on the [Panomicron Thulium](https://www.panomicron.com/thulium) designed by Oscar Oweson, has been heavily modified, but has kept the best ideas from that camera. 😉


## 📦 Repo at a Glance

- `Firmware/` – ESP32-S3 Arduino firmware (full details in `Firmware/README.md`)
- `PCBs/Main PCB/` – KiCad project + production Gerbers for the main board
- `PCBs/Breakout/` – KiCad project + Gerbers for the sensor breakout board
- `3MF/` – Print-ready 3MF files organized by part type (body, accessories, masks, fine parts)
- `STEP & F3D/` – Full assembly CAD (`MRF2-complete.f3d`) and shared STEP export (`MRF2-complete.step`)
- `OrcaSlicer/` – Slicer project files/profiles for printing

## 📷 Camera Features

- "LiDAR" distance measurement for accurate focus
- Dual OLED UI: 128×128 main in-viefinder display + 128×32 external status display
- Light meter & exposure calculation for multiple ISO values
- Lens coupled via a high-accuracy linear position sensor
- Electronic frame counting using a rotary encoder
- Battery monitoring and on-device configuration, like in-camera lens calibration

## 🛠 Build Path (high level)

1. **Order PCBs**: Use the Gerbers in `PCBs/Main PCB/Gerber` and `PCBs/Breakout/Gerber`. The `.gbrjob` files can be uploaded directly to most fabs.
2. **Assemble electronics**: Populate the main and breakout boards with the BoM below (through-hole and SMT mix), attach the Feather ESP32-S3, and wire the displays/sensors via STEMMA QT/Qwiic where applicable.
3. **Print the body**: Slice/print the 3MF models (see `3MF/` and any profiles in `OrcaSlicer/`). Fit tolerances may depend on your printer and material.
4. **Load firmware**: Follow `Firmware/README.md` to build/flash with PlatformIO, then run first-time calibration.
5. **Integrate**: Install the PCBs and displays into the printed parts, route wiring, and verify focus/meters on the bench before loading film.

## 🧱 Bill of Materials

### Electronics

| Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
| Feather ESP32-S3 (Adafruit 5477) | 1 | Main MCU, includes MAX17048 fuel gauge | [Adafruit](https://www.adafruit.com/product/5477) |
| MRF2 Main PCB | 1 | Use Gerbers in `PCBs/Main PCB/Gerber` | Any fab |
| MRF2 Breakout PCB | 1 | Use Gerbers in `PCBs/Breakout/Gerber` | Any fab |
B-type 8-pin FPC ribbon cable | 1 | Links main PCB to breakout | [Amazon](https://www.amazon.co.uk/sourcing-map-Flexible-Ribbon-Player/dp/B0FKBRHH4J)
8-pin JST-SH cable | 1 | For power switch and buttons, 10cm should be plenty | [AliExpress Search](https://www.aliexpress.com/w/wholesale-8-pin-jst%252525252dsh-cable.html)
6-pin JST-SH cable | 1 | Connect the LiDAR sensor to the breakout board - some cutting and soldering needed on the connector supplied with the sensor | [AliExpress Search](https://www.aliexpress.com/w/wholesale-8-pin-jst%252525252dsh-cable.html)
| LiDAR DTS6012M | 1 | Distance sensing | [AliExpress search](https://www.aliexpress.com/wholesale?SearchText=DTS6012M+UART) |
| 404R10KL1.0 Linear-position sensor | 1 | Lens position sensor | [DigiKey](https://www.digikey.co.uk/en/products/detail/tt-electronics-bi/404R10KL1-0/2408603)
| Light sensor BH1750 breakout (Adafruit 4681) | 1 | Lux readings | [Adafruit](https://www.adafruit.com/product/4681) |
| IMU MPU6050 breakout (Adafruit 3886) | 1 | Orientation | [Adafruit](https://www.adafruit.com/product/3886) |
| ADC ADS1015 breakout (Adafruit 1083) | 1 | Lens position | [Adafruit](https://www.adafruit.com/product/1083) |
| STEMMA QT Rotary Encoder (Adafruit 5880) | 1 | Film advance tracking | [Adafruit](https://www.adafruit.com/product/5880) |
| 1.12" 128×128 SH1107 OLED (Adafruit 5297) | 1 | Main display | [Adafruit](https://www.adafruit.com/product/5297) |
| 128×32 SSD1306 OLED (I2C) | 1 | External status display | [AliExpress search](https://www.aliexpress.com/w/wholesale-128x32-oled-display-ssd1306.html) |
| JST-SH/Qwiic/STEMMA QT cables | several | I2C daisy-chain and encoder | [Adafruit](https://www.adafruit.com/?q=stemma%20qt%20cable&sort=BestMatch) |
| LiPo cell (3.7V, 820mmAh) - 44x30.5x6.8xmm | 1 | **CHECK POLARITY** | [Amazon](https://www.amazon.co.uk/dp/B082152887) |
| 8mm Momentary Button| 2 | Select buttons | [Amazon](https://www.amazon.co.uk/dp/B07S1MNB8C)
| 6mm DPDT slide switch | 1 | Power switch | [Amazon](https://www.amazon.co.uk/dp/B07H9VPK1J)

### Hardware

Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
Heat-set inserts | as needed | M2x4 & M3x4 | Local fasteners supplier |
Counter-sunk screws | as needed | M2x4, M2x6, M3x6, M3x8 counter-sunk screws  | Local fasteners supplier |
M3x6 Thumb Screw | 1 | To hold the door closed | Local fasteners supplier |
Steel rod - for door hinge | 1 | Cut to length: ~82mm | [Amazon](https://www.amazon.co.uk/HXJDAM-Stainless-Steel-Round-Diameter/dp/B0D2V6WB1Y) |
2 Turn Door Handle Spring | 1 | For advance lever auto-return | [Amazon](https://www.amazon.co.uk/dp/B076VRBT4M) |
CSK8PP One-way bearing / sprag clutch | 1 | For advance-lever, make sure inner and outer keyed | [AliExpress Search](https://www.aliexpress.com/w/wholesale-CSK8PP.html)
Cold-shoe | 1 | Mount on top | [Amazon](https://www.amazon.co.uk/Anwenk-Adapter-Bracket-Standard-Monitor/dp/B01MD29V3X)

### Optics

Item | Qty | Notes | Example sources |
| --- | --- | --- | --- |
20.0mm Dia. x 70.0mm FL Plano-convex lens (PCX) | 1 | Rear viewfinder lens | [Edmunds Optics](https://www.edmundoptics.co.uk/p/200mm-dia-x-700mm-fl-uncoated-plano-convex-lens/2705) |
25.0mm Dia. x -25 FL Planco-concave lens (PCV) | 1 | Front viewfinder lens | [Edmunds Optics](https://www.edmundoptics.co.uk/p/250mm-dia-x25-fl-uncoated-plano-concave-lens/5542/)
30x30x1.1mm 50R/50T beam splitter | 1 | Sits between front and rear optics | [AliExpress Search](https://www.aliexpress.com/w/wholesale-30x30x1.1-50R%25252F50T-beam-splitter.html)


## 💰 Cost Estimate (ballpark)

Assumes single-quantity retail buys (Adafruit/Amazon/AliExpress), optics from Edmund Optics, and £1 ≈ $1.27; excludes shipping/VAT/import and assembly labor.

- Electronics (MCU, sensors, displays, cables, LiPo, switches): ~$125 / ~£100
- PCB share (DIY assembly; amortized per build): ~$10 / ~£6  \[full 5× PCB batch ~ $35 / £28\]
- Hardware/fasteners/mech bits: ~$25 / ~£20
- Optics (lenses + beam splitter): ~$115 / ~£90
- Printed parts material: ~$25 / ~£20
- Rough per-build total: ~$300 / ~£235 (add shipping/taxes and any PCB batch overhead you keep)

## 🖨 CAD & Printed Parts

- Full assembly in `STEP & F3D/MRF2-complete.f3d` (Fusion 360) and `MRF2-complete.step` (generic).
- Print-ready 3MFs are grouped by function under `3MF/`. Use provided slicer profiles in `OrcaSlicer/` as a starting point and tweak for your printer/material.

## 🤖 PCB Notes

- Open the KiCad projects: `PCBs/Main PCB/KiCAD/MRF-Pro-v7.5.kicad_pro` and `PCBs/Breakout/KiCAD/MRF-Pro-v7.5-breakout.kicad_pro`.
- Fabrication: the provided Gerber sets include copper, mask, paste, silkscreen, drills, and a `.gbrjob` for auto-detection at most PCB fabs.
- Rev: current files are labeled v7.5. Check for updates before ordering.

## 💾 Firmware

Firmware source and build instructions live in `Firmware/README.md`. PlatformIO is used for builds/flash/monitoring. Calibrate lenses and film formats after first flash.

## 📜 License

Firmware and design files are released under the GNU GPL v3.0 (see `LICENSE`). 
