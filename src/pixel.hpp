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

  bool differs_from(const Pixel& other, bool near_edge, int threshold = 40) {
    int avg_diff = (std::abs(red - other.red) + std::abs(green - other.green) + std::abs(blue - other.blue)) / 3;
    threshold = near_edge ? 225 : threshold;
    // std::cout << "avg_diff: " << avg_diff << " : threshold: " << threshold << std::endl;
    return avg_diff > threshold;
  }
};
#endif
