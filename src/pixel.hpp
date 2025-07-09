#ifndef PIXEL_HPP
#define PIXEL_HPP
#include <iostream>

struct Pixel {
  uint8_t blue {0};
  uint8_t green {0};
  uint8_t red {0};
  uint8_t alpha {0};

  std::string toString() const {
    return "R:" + std::to_string(static_cast<int>(red)) + " " +
           "G:" + std::to_string(static_cast<int>(green)) + " " +
           "B:" + std::to_string(static_cast<int>(blue)) + " " +
           "A:" + std::to_string(static_cast<int>(alpha)) + "\n";
  }

  bool differs_from(const Pixel& other, int threshold = 15) {
    int diff_red = std::abs(red - other.red);
    int diff_green = std::abs(green - other.green);
    int diff_blue = std::abs(blue - other.blue);
    int diff_alpha = std::abs(alpha - other.alpha);

    if (diff_red > threshold || diff_green > threshold || diff_blue > threshold || diff_alpha > threshold) {
      return true;
    }
    return false;
  }
};
#endif
