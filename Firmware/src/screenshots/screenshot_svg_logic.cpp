#include "screenshot_svg_logic.h"

namespace
{
// A set bit in the GFXcanvas1 buffer means the pixel is lit (white). Bits are
// packed MSB-first within each byte, one row after another.
bool pixelIsWhite(const uint8_t *buffer, int bytesPerRow, int x, int y)
{
  const uint8_t byte = buffer[(y * bytesPerRow) + (x / 8)];
  const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
  return (byte & mask) != 0;
}
} // namespace

std::string renderFrameBufferSvg(const uint8_t *buffer,
                                 int width,
                                 int height,
                                 int bytesPerRow,
                                 int scale)
{
  const int svgWidth = width * scale;
  const int svgHeight = height * scale;

  std::string out;
  out.reserve(4096);

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
  out += std::to_string(svgWidth);
  out += "\" height=\"";
  out += std::to_string(svgHeight);
  out += "\" viewBox=\"0 0 ";
  out += std::to_string(width);
  out += " ";
  out += std::to_string(height);
  out += "\" shape-rendering=\"crispEdges\">\n";

  out += "  <rect x=\"0\" y=\"0\" width=\"";
  out += std::to_string(width);
  out += "\" height=\"";
  out += std::to_string(height);
  out += "\" fill=\"#000000\"/>\n";

  // Emit each contiguous run of white pixels on a row as a single rect. Runs
  // never cross a row boundary because the scan restarts at each row.
  for (int y = 0; y < height; y++)
  {
    int x = 0;
    while (x < width)
    {
      if (!pixelIsWhite(buffer, bytesPerRow, x, y))
      {
        x++;
        continue;
      }

      int runStart = x;
      while (x < width && pixelIsWhite(buffer, bytesPerRow, x, y))
      {
        x++;
      }
      int runLength = x - runStart;

      out += "  <rect x=\"";
      out += std::to_string(runStart);
      out += "\" y=\"";
      out += std::to_string(y);
      out += "\" width=\"";
      out += std::to_string(runLength);
      out += "\" height=\"1\" fill=\"#ffffff\"/>\n";
    }
  }

  out += "</svg>\n";
  return out;
}
