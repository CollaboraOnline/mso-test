#ifndef PIXELBASHER_HPP
#define PIXELBASHER_HPP

#include <vector>
#include "bmp.hpp"

class PixelBasher {
public:
  enum Colour {
    RED,
    YELLOW,
  };

  std::vector<bool> sobel_edges(const BMP& bmp, int threshold = 15);

  // void highlightEdges(BMP& bmp, const std::vector<bool>& edge_map);

  std::vector<bool> blur_edge_mask(const BMP& bmp, const std::vector<bool>& edge_map);

  void compare_to_bmp(BMP& base, const BMP& imported, bool enable_minor_differences);

  std::vector<uint8_t> colour_pixel(Colour colour);

  // std::vector<bool> threshold_layout_mask(const BMP& bmp, int threshold = 20);
 };
#endif
