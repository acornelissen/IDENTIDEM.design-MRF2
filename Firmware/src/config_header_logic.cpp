#include "config_header_logic.h"

namespace
{
// Monospace advance widths of the two header fonts (u8g2 *_mf fonts).
constexpr int HEADER_FONT_LARGE_CHAR_W = 9; // u8g2_font_9x15_mf
} // namespace

int configHeaderTextWidthPx(std::size_t titleChars, int charWidthPx)
{
  return static_cast<int>(titleChars) * charWidthPx;
}

bool configHeaderNeedsSmallFont(std::size_t titleChars, int availableWidthPx)
{
  return configHeaderTextWidthPx(titleChars, HEADER_FONT_LARGE_CHAR_W) > availableWidthPx;
}
