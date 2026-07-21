#ifndef CONFIG_HEADER_LOGIC_H
#define CONFIG_HEADER_LOGIC_H

#include <cstddef>

// Config-screen title font autoscaling.
//
// Titles are drawn with the 9x15 monospace font, but the longest breadcrumbs
// ("Setup > Display", "Display > Horizon", "Factory Reset?") are wider than the
// 128px display and used to clip. When the 9x15 rendering would overflow the
// available width, the header falls back to the 6x10 font, which fits every
// title we ship. This keeps the full text on screen instead of cutting it off.

// Width in pixels of a monospace title, given a per-character advance.
int configHeaderTextWidthPx(std::size_t titleChars, int charWidthPx);

// True when a 9x15 title of this length would not fit and should drop to 6x10.
bool configHeaderNeedsSmallFont(std::size_t titleChars, int availableWidthPx);

#endif // CONFIG_HEADER_LOGIC_H
