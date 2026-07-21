#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

// Pure module, no hardware. Included directly because the native test env is
// built with test_build_src=no.
#include "../../src/screenshots/screenshot_svg_logic.cpp"

namespace
{
// Build a zeroed GFXcanvas1-style buffer for a width x height 1-bit image.
std::vector<uint8_t> makeBuffer(int width, int height, int &bytesPerRow)
{
  bytesPerRow = (width + 7) / 8;
  return std::vector<uint8_t>(static_cast<size_t>(bytesPerRow) * height, 0);
}

void setPixel(std::vector<uint8_t> &buffer, int bytesPerRow, int x, int y)
{
  buffer[(y * bytesPerRow) + (x / 8)] |= static_cast<uint8_t>(0x80 >> (x & 7));
}

int countOccurrences(const std::string &haystack, const std::string &needle)
{
  int count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos)
  {
    count++;
    pos += needle.size();
  }
  return count;
}
} // namespace

void setUp() {}
void tearDown() {}

void test_header_dimensions_scale_and_viewbox()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(128, 128, bytesPerRow);

  std::string svg = renderFrameBufferSvg(buffer.data(), 128, 128, bytesPerRow, 4);

  TEST_ASSERT_TRUE(svg.find("width=\"512\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("height=\"512\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("viewBox=\"0 0 128 128\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("shape-rendering=\"crispEdges\"") != std::string::npos);
  // Black background rect present.
  TEST_ASSERT_TRUE(svg.find("fill=\"#000000\"") != std::string::npos);
}

void test_external_display_dimensions()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(128, 32, bytesPerRow);

  std::string svg = renderFrameBufferSvg(buffer.data(), 128, 32, bytesPerRow, 4);

  TEST_ASSERT_TRUE(svg.find("width=\"512\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("height=\"128\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("viewBox=\"0 0 128 32\"") != std::string::npos);
}

void test_empty_buffer_has_no_white_rects()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(32, 8, bytesPerRow);

  std::string svg = renderFrameBufferSvg(buffer.data(), 32, 8, bytesPerRow, 4);

  // Only the background is filled; no white pixels emitted.
  TEST_ASSERT_EQUAL_INT(0, countOccurrences(svg, "fill=\"#ffffff\""));
}

void test_single_pixel_emits_one_rect_at_coords()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(32, 8, bytesPerRow);
  setPixel(buffer, bytesPerRow, 5, 3);

  std::string svg = renderFrameBufferSvg(buffer.data(), 32, 8, bytesPerRow, 4);

  TEST_ASSERT_EQUAL_INT(1, countOccurrences(svg, "fill=\"#ffffff\""));
  TEST_ASSERT_TRUE(svg.find("<rect x=\"5\" y=\"3\" width=\"1\" height=\"1\" fill=\"#ffffff\"/>") !=
                   std::string::npos);
}

void test_horizontal_run_coalesces_into_single_rect()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(32, 8, bytesPerRow);
  setPixel(buffer, bytesPerRow, 4, 2);
  setPixel(buffer, bytesPerRow, 5, 2);
  setPixel(buffer, bytesPerRow, 6, 2);

  std::string svg = renderFrameBufferSvg(buffer.data(), 32, 8, bytesPerRow, 4);

  TEST_ASSERT_EQUAL_INT(1, countOccurrences(svg, "fill=\"#ffffff\""));
  TEST_ASSERT_TRUE(svg.find("<rect x=\"4\" y=\"2\" width=\"3\" height=\"1\" fill=\"#ffffff\"/>") !=
                   std::string::npos);
}

void test_gap_splits_into_two_rects()
{
  int bytesPerRow = 0;
  std::vector<uint8_t> buffer = makeBuffer(32, 8, bytesPerRow);
  setPixel(buffer, bytesPerRow, 4, 2);
  setPixel(buffer, bytesPerRow, 5, 2);
  // gap at 6
  setPixel(buffer, bytesPerRow, 7, 2);

  std::string svg = renderFrameBufferSvg(buffer.data(), 32, 8, bytesPerRow, 4);

  TEST_ASSERT_EQUAL_INT(2, countOccurrences(svg, "fill=\"#ffffff\""));
  TEST_ASSERT_TRUE(svg.find("<rect x=\"4\" y=\"2\" width=\"2\" height=\"1\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("<rect x=\"7\" y=\"2\" width=\"1\" height=\"1\"") != std::string::npos);
}

void test_runs_do_not_cross_row_boundary()
{
  int bytesPerRow = 0;
  const int width = 8;
  std::vector<uint8_t> buffer = makeBuffer(width, 4, bytesPerRow);
  // Last pixel of row 0 and first pixel of row 1 are both lit but must stay
  // two separate rects.
  setPixel(buffer, bytesPerRow, width - 1, 0);
  setPixel(buffer, bytesPerRow, 0, 1);

  std::string svg = renderFrameBufferSvg(buffer.data(), width, 4, bytesPerRow, 4);

  TEST_ASSERT_EQUAL_INT(2, countOccurrences(svg, "fill=\"#ffffff\""));
  TEST_ASSERT_TRUE(svg.find("<rect x=\"7\" y=\"0\" width=\"1\" height=\"1\"") != std::string::npos);
  TEST_ASSERT_TRUE(svg.find("<rect x=\"0\" y=\"1\" width=\"1\" height=\"1\"") != std::string::npos);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_header_dimensions_scale_and_viewbox);
  RUN_TEST(test_external_display_dimensions);
  RUN_TEST(test_empty_buffer_has_no_white_rects);
  RUN_TEST(test_single_pixel_emits_one_rect_at_coords);
  RUN_TEST(test_horizontal_run_coalesces_into_single_rect);
  RUN_TEST(test_gap_splits_into_two_rects);
  RUN_TEST(test_runs_do_not_cross_row_boundary);
  return UNITY_END();
}
