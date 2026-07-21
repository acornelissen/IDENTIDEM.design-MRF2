#ifndef SCREENSHOT_SVG_LOGIC_H
#define SCREENSHOT_SVG_LOGIC_H

#include <cstdint>
#include <string>

// Pure conversion of a 1-bit framebuffer (Adafruit GFXcanvas1 layout: rows of
// ceil(width/8) bytes, MSB-first, a set bit meaning a lit/white pixel) into an
// SVG string that renders pixel-exact on any viewer.
//
// The output is a black background with white pixels emitted as horizontal
// run-length <rect> elements. shape-rendering="crispEdges" keeps the scaled
// pixels sharp. The SVG's width/height are scale x the pixel dimensions while
// the viewBox stays at 1:1 pixel dimensions, matching the hand-drawn mockups
// (512x512 for the 128x128 main display, 512x128 for the 128x32 external one).
//
// Kept free of any hardware/Arduino dependency so it is unit-testable in the
// native core-tests suite.
std::string renderFrameBufferSvg(const uint8_t *buffer,
                                 int width,
                                 int height,
                                 int bytesPerRow,
                                 int scale);

#endif // SCREENSHOT_SVG_LOGIC_H
