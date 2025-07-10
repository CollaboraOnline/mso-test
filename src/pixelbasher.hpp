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

  std::vector<bool> sobelEdges(const BMP& bmp, int threshold = 15);

  void highlightEdges(BMP& bmp, const std::vector<bool>& edge_map);

  std::vector<bool> blurEdgeMask(const BMP& bmp, const std::vector<bool>& edge_map);

  void compareToBMP(BMP& base, const BMP& imported, bool enable_minor_differences);

  std::vector<uint8_t> colourPixel(Colour colour);
 };
#endif
