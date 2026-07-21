#include "screenshot_svg_logic.h"

namespace
{
// Light anti-aliasing: the on-screen pixels are hard-edged, but at the 4x
// scale the manual displays them that harshness reads as jagged. A small
// Gaussian blur (fraction of a device pixel) softens the edges just enough to
// suggest an OLED without smearing 1px text strokes into illegibility.
constexpr const char *kSoftenStdDeviation = "0.35";

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
  out += "\">\n";

  out += "  <defs><filter id=\"soften\" x=\"-2%\" y=\"-2%\" width=\"104%\" height=\"104%\">";
  out += "<feGaussianBlur stdDeviation=\"";
  out += kSoftenStdDeviation;
  out += "\"/></filter></defs>\n";

  // Background stays crisp; only the lit pixels are softened.
  out += "  <rect x=\"0\" y=\"0\" width=\"";
  out += std::to_string(width);
  out += "\" height=\"";
  out += std::to_string(height);
  out += "\" fill=\"#000000\" shape-rendering=\"crispEdges\"/>\n";

  out += "  <g filter=\"url(#soften)\">\n";

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

  out += "  </g>\n";
  out += "</svg>\n";
  return out;
}
